#pragma once

#include "esphome/core/component.h"
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

  void set_water_level_sensor(sensor::Sensor *sensor) { this->water_level_sensor_ = sensor; }
  void set_pump_output(output::BinaryOutput *output) { this->pump_output_ = output; }
  void set_target_level(float level) { this->target_level_ = level; }
  void set_critical_low_level(float level) { this->critical_low_level_ = level; }
  void set_fill_timeout(uint32_t timeout_ms) { this->fill_timeout_ms_ = timeout_ms; }
  void set_cooldown_duration(uint32_t duration_ms) { this->cooldown_duration_ms_ = duration_ms; }

  void request_fill();
  void reset_error();

  FillState get_state() const { return this->state_; }
  uint32_t get_fill_count() const { return this->fill_count_; }
  bool is_critical_low() const { return this->critical_low_; }

  const char *state_to_string(FillState state) const;

 protected:
  void transition_to_(FillState new_state);
  void pump_off_();

  sensor::Sensor *water_level_sensor_{nullptr};
  output::BinaryOutput *pump_output_{nullptr};

  float target_level_{80.0f};
  float critical_low_level_{15.0f};
  uint32_t fill_timeout_ms_{120000};
  uint32_t cooldown_duration_ms_{300000};

  FillState state_{STATE_IDLE};
  uint32_t state_start_ms_{0};
  uint32_t fill_count_{0};
  bool critical_low_{false};
};

}  // namespace water_controller
}  // namespace esphome
