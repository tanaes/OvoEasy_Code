#include "water_controller.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace water_controller {

static const char *const TAG = "water_controller";

// Uncertainty grows at this rate between calibration edges
static const float UNCERTAINTY_GROWTH_RATE = 0.1f;  // %/s (~6%/min)
static const float MAX_UNCERTAINTY = 50.0f;
// Max dt to integrate in one step (prevents huge jumps after sleep/reboot)
static const float MAX_DT_S = 60.0f;
// Debounce: require this many consecutive matching readings
static const uint8_t DEBOUNCE_THRESHOLD = 3;

void WaterController::setup() {
  this->state_ = STATE_IDLE;
  this->fill_count_ = 0;
  this->critical_low_ = false;
  this->pump_off_();
  this->last_model_update_ms_ = millis();

  // Boot calibration: distinguish sensor failure from "not full"
  if (this->water_level_sensor_ == nullptr) {
    ESP_LOGE(TAG, "Boot: water level sensor not configured!");
    this->estimated_level_pct_ = 0.0f;
    this->uncertainty_pct_ = MAX_UNCERTAINTY;
    this->debounced_full_ = false;
    this->debounce_count_ = 0;
  } else {
    float voltage = this->water_level_sensor_->state;
    if (std::isnan(voltage)) {
      // Sensor not ready at boot — safe defaults, no auto-fill
      ESP_LOGW(TAG, "Boot: water sensor NaN, safe defaults (no auto-fill)");
      this->estimated_level_pct_ = 0.0f;
      this->uncertainty_pct_ = MAX_UNCERTAINTY;
      this->debounced_full_ = false;
      this->debounce_count_ = 0;
    } else {
      bool initial_full = this->float_switch_active_high_
                          ? (voltage >= this->float_switch_threshold_v_)
                          : (voltage <= this->float_switch_threshold_v_);
      this->float_is_full_ = initial_full;
      this->debounced_full_ = initial_full;
      this->debounce_count_ = DEBOUNCE_THRESHOLD;

      if (initial_full) {
        this->estimated_level_pct_ = 100.0f;
        this->uncertainty_pct_ = 0.0f;
        ESP_LOGI(TAG, "Boot: float switch reads FULL, estimate=100%%");
      } else {
        // Not full on boot — conservative estimate, block auto-fill
        // until model drains estimate below fill_trigger naturally
        this->estimated_level_pct_ = 50.0f;
        this->uncertainty_pct_ = MAX_UNCERTAINTY;
        ESP_LOGI(TAG, "Boot: float switch reads NOT FULL, estimate=50%% (high uncertainty)");
      }
    }
  }

  ESP_LOGI(TAG, "Water controller initialized (edge-based + model estimator)");
}

bool WaterController::read_float_switch_() {
  if (this->water_level_sensor_ == nullptr) {
    return false;
  }
  float voltage = this->water_level_sensor_->state;
  if (std::isnan(voltage)) {
    return false;
  }
  return this->float_switch_active_high_
         ? (voltage >= this->float_switch_threshold_v_)
         : (voltage <= this->float_switch_threshold_v_);
}

void WaterController::update_model_(uint32_t now) {
  float dt_s = (now - this->last_model_update_ms_) / 1000.0f;
  if (dt_s <= 0.0f) {
    return;
  }
  if (dt_s > MAX_DT_S) {
    dt_s = MAX_DT_S;
  }

  // Integrate fill rate when pump is running
  if (this->state_ == STATE_FILLING) {
    this->estimated_level_pct_ += this->pump_fill_rate_pct_per_s_ * dt_s;
  }

  // Integrate drain rate when humidifier is active
  if (this->humidifier_active_) {
    this->estimated_level_pct_ -= this->drain_rate_pct_per_s_ * dt_s;
  }

  // Clamp estimate
  if (this->estimated_level_pct_ < 0.0f) this->estimated_level_pct_ = 0.0f;
  if (this->estimated_level_pct_ > 100.0f) this->estimated_level_pct_ = 100.0f;

  // Grow uncertainty only during active states (IDLE/FILLING)
  // In ERROR/COOLDOWN nothing changes physically, so uncertainty stays flat
  if (this->state_ == STATE_IDLE || this->state_ == STATE_FILLING) {
    this->uncertainty_pct_ += UNCERTAINTY_GROWTH_RATE * dt_s;
    if (this->uncertainty_pct_ > MAX_UNCERTAINTY) {
      this->uncertainty_pct_ = MAX_UNCERTAINTY;
    }
  }

  this->last_model_update_ms_ = now;
}

void WaterController::loop() {
  if (this->water_level_sensor_ == nullptr) {
    this->pump_off_();
    return;
  }

  float voltage = this->water_level_sensor_->state;
  if (std::isnan(voltage)) {
    this->pump_off_();
    return;
  }

  uint32_t now = millis();

  // Update model estimate (integrate rates over dt)
  this->update_model_(now);

  bool raw_full = this->float_switch_active_high_
                  ? (voltage >= this->float_switch_threshold_v_)
                  : (voltage <= this->float_switch_threshold_v_);

  // Debounce: require DEBOUNCE_THRESHOLD consecutive matching readings
  if (raw_full == this->float_is_full_) {
    if (this->debounce_count_ < DEBOUNCE_THRESHOLD) {
      this->debounce_count_++;
    }
  } else {
    this->float_is_full_ = raw_full;
    this->debounce_count_ = 1;
  }

  bool new_debounced = this->debounced_full_;
  if (this->debounce_count_ >= DEBOUNCE_THRESHOLD) {
    new_debounced = this->float_is_full_;
  }

  // Edge detection on debounced signal
  if (new_debounced != this->debounced_full_) {
    if (new_debounced) {
      // Rising edge: not-full -> full (calibration point)
      this->estimated_level_pct_ = 100.0f;
      this->uncertainty_pct_ = 0.0f;
      this->auto_fill_blocked_ = false;  // Sensor confirmed working, allow auto-fill
      ESP_LOGI(TAG, "Float switch: FULL (calibrated to 100%%)");
    } else {
      // Falling edge: full -> not-full (just dropped below full)
      this->estimated_level_pct_ = 95.0f;
      this->uncertainty_pct_ = 2.0f;
      ESP_LOGI(TAG, "Float switch: NOT FULL (estimate ~95%%)");
    }
    this->debounced_full_ = new_debounced;
  }

  // Update critical-low flag using pessimistic estimate
  // True level could be as low as (estimated - uncertainty)
  float pessimistic_level = this->estimated_level_pct_ - this->uncertainty_pct_;
  if (pessimistic_level < 0.0f) pessimistic_level = 0.0f;
  this->critical_low_ = (pessimistic_level < this->critical_low_pct_);

  // State machine
  uint32_t elapsed = now - this->state_start_ms_;

  switch (this->state_) {
    case STATE_IDLE:
      // Auto-fill requires: not full, below trigger, and not blocked
      // auto_fill_blocked_ prevents infinite ERROR->reset->fill loops
      // Unblocked by: first successful fill, manual fill request, or full edge detection
      if (!this->auto_fill_blocked_ &&
          !this->debounced_full_ &&
          this->estimated_level_pct_ <= this->fill_trigger_pct_) {
        this->transition_to_(STATE_FILLING);
        this->pump_output_->turn_on();
        ESP_LOGI(TAG, "Fill triggered: estimate=%.1f%%, trigger=%.1f%%",
                 this->estimated_level_pct_, this->fill_trigger_pct_);
      }
      break;

    case STATE_FILLING:
      if (this->debounced_full_) {
        // Float switch says full — fill complete
        this->pump_off_();
        this->transition_to_(STATE_COOLDOWN);
        this->fill_count_++;
        this->auto_fill_blocked_ = false;  // Successful fill proves system works
        ESP_LOGI(TAG, "Fill complete, count: %u", this->fill_count_);
      } else if (elapsed > this->fill_timeout_ms_) {
        this->pump_off_();
        this->transition_to_(STATE_ERROR);
        ESP_LOGE(TAG, "Fill timeout after %us! estimate=%.1f%%",
                 this->fill_timeout_ms_ / 1000, this->estimated_level_pct_);
      }
      break;

    case STATE_COOLDOWN:
      if (elapsed > this->cooldown_duration_ms_) {
        this->transition_to_(STATE_IDLE);
      }
      break;

    case STATE_ERROR:
      // Pump stays off. Wait for manual reset_error() call.
      break;
  }
}

void WaterController::request_fill() {
  if (this->state_ == STATE_IDLE) {
    if (this->debounced_full_) {
      ESP_LOGW(TAG, "Manual fill skipped: float switch already reads FULL");
      return;
    }
    ESP_LOGI(TAG, "Manual fill requested");
    this->auto_fill_blocked_ = false;  // Manual request implies user supervision
    this->transition_to_(STATE_FILLING);
    this->pump_output_->turn_on();
  } else {
    ESP_LOGW(TAG, "Cannot start fill: state is %s", this->state_to_string(this->state_));
  }
}

void WaterController::reset_error() {
  if (this->state_ == STATE_ERROR) {
    ESP_LOGI(TAG, "Error reset, returning to IDLE");
    this->transition_to_(STATE_IDLE);
  } else {
    ESP_LOGW(TAG, "reset_error called but state is %s", this->state_to_string(this->state_));
  }
}

void WaterController::transition_to_(FillState new_state) {
  ESP_LOGI(TAG, "State: %s -> %s", this->state_to_string(this->state_),
           this->state_to_string(new_state));
  this->state_ = new_state;
  this->state_start_ms_ = millis();
}

void WaterController::pump_off_() {
  if (this->pump_output_ != nullptr) {
    this->pump_output_->turn_off();
  }
}

const char *WaterController::state_to_string(FillState state) const {
  switch (state) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_FILLING:
      return "FILLING";
    case STATE_COOLDOWN:
      return "COOLDOWN";
    case STATE_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

void WaterController::dump_config() {
  ESP_LOGCONFIG(TAG, "Water Controller (edge-based + model estimator):");
  ESP_LOGCONFIG(TAG, "  Float Switch Threshold: %.2fV", this->float_switch_threshold_v_);
  ESP_LOGCONFIG(TAG, "  Pump Fill Rate: %.2f%%/s", this->pump_fill_rate_pct_per_s_);
  ESP_LOGCONFIG(TAG, "  Drain Rate: %.2f%%/s", this->drain_rate_pct_per_s_);
  ESP_LOGCONFIG(TAG, "  Fill Trigger: %.1f%%", this->fill_trigger_pct_);
  ESP_LOGCONFIG(TAG, "  Critical Low: %.1f%%", this->critical_low_pct_);
  ESP_LOGCONFIG(TAG, "  Fill Timeout: %us", this->fill_timeout_ms_ / 1000);
  ESP_LOGCONFIG(TAG, "  Cooldown: %us", this->cooldown_duration_ms_ / 1000);
}

}  // namespace water_controller
}  // namespace esphome
