#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/output/binary_output.h"

namespace esphome {
namespace water_controller {

enum FillState : uint8_t {
  STATE_IDLE = 0,
  STATE_FILLING = 1,
  STATE_COOLDOWN = 2,
  STATE_ERROR = 3,
};

class WaterController : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters (called from codegen)
  void set_water_level_sensor(sensor::Sensor *sensor) { this->water_level_sensor_ = sensor; }
  void set_pump_output(output::BinaryOutput *output) { this->pump_output_ = output; }
  void set_fill_timeout(uint32_t timeout_ms) { this->fill_timeout_ms_ = timeout_ms; }
  void set_cooldown_duration(uint32_t duration_ms) { this->cooldown_duration_ms_ = duration_ms; }

  // Model parameter setters (called from codegen + runtime sync)
  void set_float_switch_threshold(float volts) { this->float_switch_threshold_v_ = volts; }
  void set_float_switch_active_high(bool active_high) { this->float_switch_active_high_ = active_high; }
  void set_pump_fill_rate(float pct_per_s) { this->pump_fill_rate_pct_per_s_ = pct_per_s; }
  void set_drain_rate(float pct_per_s) { this->drain_rate_pct_per_s_ = pct_per_s; }
  void set_fill_trigger_level(float pct) { this->fill_trigger_pct_ = pct; }
  void set_critical_low_level(float pct) { this->critical_low_pct_ = pct; }

  // Humidifier state tracking (called from climate-humidity.yaml lambdas)
  void set_humidifier_active(bool active) { this->humidifier_active_ = active; }

  // Public interface
  void request_fill();
  void reset_error();

  FillState get_state() const { return this->state_; }
  uint32_t get_fill_count() const { return this->fill_count_; }
  bool is_critical_low() const { return this->critical_low_; }
  float get_estimated_level() const { return this->estimated_level_pct_; }
  float get_uncertainty() const { return this->uncertainty_pct_; }

  const char *state_to_string(FillState state) const;

 protected:
  void transition_to_(FillState new_state);
  void pump_off_();
  bool read_float_switch_();
  void update_model_(uint32_t now);

  // Hardware references
  sensor::Sensor *water_level_sensor_{nullptr};
  output::BinaryOutput *pump_output_{nullptr};

  // Configuration parameters
  float float_switch_threshold_v_{1.5f};
  bool float_switch_active_high_{false};  // true: voltage >= threshold = full; false: voltage <= threshold = full
  float pump_fill_rate_pct_per_s_{1.0f};
  float drain_rate_pct_per_s_{0.1f};
  float fill_trigger_pct_{30.0f};
  float critical_low_pct_{15.0f};
  uint32_t fill_timeout_ms_{120000};
  uint32_t cooldown_duration_ms_{300000};

  // State machine
  FillState state_{STATE_IDLE};
  uint32_t state_start_ms_{0};
  uint32_t fill_count_{0};
  bool critical_low_{false};

  // Model estimator state
  float estimated_level_pct_{100.0f};
  float uncertainty_pct_{50.0f};
  uint32_t last_model_update_ms_{0};

  // Float switch edge detection + debounce
  bool float_is_full_{false};
  uint8_t debounce_count_{0};
  bool debounced_full_{false};
  bool auto_fill_blocked_{true};  // Block auto-fill until first successful fill or manual request

  // Humidifier state (set externally)
  bool humidifier_active_{false};
};

}  // namespace water_controller
}  // namespace esphome
