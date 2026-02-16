#include "water_controller.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace water_controller {

static const char *const TAG = "water_controller";

void WaterController::setup() {
  this->state_ = STATE_IDLE;
  this->fill_count_ = 0;
  this->critical_low_ = false;
  this->pump_off_();
  ESP_LOGI(TAG, "Water controller initialized");
}

void WaterController::loop() {
  // Guard: if sensor not initialized, keep pump off
  if (this->water_level_sensor_ == nullptr) {
    this->pump_off_();
    return;
  }

  float level = this->water_level_sensor_->state;

  // Guard: if sensor returns NaN, keep pump off and stay in current state
  if (std::isnan(level)) {
    this->pump_off_();
    return;
  }

  // Update critical-low flag regardless of state
  this->critical_low_ = (level < this->critical_low_level_);

  uint32_t now = millis();
  uint32_t elapsed = now - this->state_start_ms_;

  switch (this->state_) {
    case STATE_IDLE:
      if (level < this->target_level_) {
        this->transition_to_(STATE_FILLING);
        this->pump_output_->turn_on();
      }
      break;

    case STATE_FILLING:
      if (level >= this->target_level_) {
        this->transition_to_(STATE_COOLDOWN);
        this->pump_off_();
        this->fill_count_++;
        ESP_LOGI(TAG, "Fill complete, count: %u, level: %.1f%%", this->fill_count_, level);
      } else if (elapsed > this->fill_timeout_ms_) {
        this->transition_to_(STATE_ERROR);
        this->pump_off_();
        ESP_LOGE(TAG, "Fill timeout after %us! Level: %.1f%%, target: %.1f%%",
                 this->fill_timeout_ms_ / 1000, level, this->target_level_);
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
    ESP_LOGI(TAG, "Manual fill requested");
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
  ESP_LOGCONFIG(TAG, "Water Controller:");
  ESP_LOGCONFIG(TAG, "  Target Level: %.1f%%", this->target_level_);
  ESP_LOGCONFIG(TAG, "  Critical Low: %.1f%%", this->critical_low_level_);
  ESP_LOGCONFIG(TAG, "  Fill Timeout: %us", this->fill_timeout_ms_ / 1000);
  ESP_LOGCONFIG(TAG, "  Cooldown: %us", this->cooldown_duration_ms_ / 1000);
}

}  // namespace water_controller
}  // namespace esphome
