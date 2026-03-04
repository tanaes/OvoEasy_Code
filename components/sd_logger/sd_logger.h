#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <string>
#include <vector>

#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

namespace esphome {
namespace sd_logger {

static const size_t MAX_BUFFER_ROWS = 120;

class SdLogger : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Configuration setters
  void set_cs_pin(int pin) { this->cs_pin_ = pin; }
  void set_retention_days(int days) { this->retention_days_ = days; }
  void set_flush_interval(uint32_t ms) { this->flush_interval_ms_ = ms; }

  // Public interface — called from YAML lambdas
  void log_row(const std::string &csv_line);
  bool is_mounted() const { return this->mounted_; }
  const char *get_status() const;

 protected:
  void mount_card_();
  void unmount_card_();
  void mark_card_removed_();
  bool check_card_present_();
  void flush_buffer_();
  void rotate_file_if_needed_();
  void cleanup_old_files_();
  std::string current_date_string_();

  // Configuration
  int cs_pin_{14};
  int retention_days_{30};
  uint32_t flush_interval_ms_{60000};

  // State
  bool mounted_{false};
  bool mount_attempted_{false};
  std::string mount_error_;
  sdmmc_card_t *card_{nullptr};
  sdspi_dev_handle_t sdspi_handle_{0};
  std::vector<std::string> buffer_;
  uint32_t last_flush_ms_{0};
  std::string current_file_date_;
  std::string current_file_path_;
  uint32_t last_remount_attempt_ms_{0};
  uint32_t last_health_check_ms_{0};
  bool stale_vfs_{false};
};

}  // namespace sd_logger
}  // namespace esphome
