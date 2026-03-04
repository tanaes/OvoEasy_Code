#include "sd_logger.h"
#include "esphome/core/log.h"

#include <cstring>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include "ff.h"
#include <cerrno>
#include <algorithm>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"

namespace esphome {
namespace sd_logger {

static const char *const TAG = "sd_logger";
static const char *const MOUNT_POINT = "/sdcard";
static const char *const BASE_DIR = "/sdcard/ovoeasy";
static const char *const LOG_DIR = "/sdcard/ovoeasy/logs";
static const char *const CSV_HEADER =
    "timestamp,temp,humidity,temp_sp,humid_sp,pid_output,"
    "water_level,water_state,system_state,current,"
    "sht45_t,sht45_h,hdc_t,hdc_h,bme_t,bme_h,bme_p";

void SdLogger::setup() {
  this->buffer_.reserve(MAX_BUFFER_ROWS);
  this->last_flush_ms_ = millis();

  this->mount_card_();
  if (this->mounted_) {
    this->cleanup_old_files_();
  }
}

void SdLogger::mount_card_() {
  this->mount_attempted_ = true;

  // If previous mount was lost to hot removal, clean up stale VFS/FatFS state.
  // esp_vfs_fat_sdcard_unmount is safe here because the VFS/FatFS structures
  // are in RAM — it won't try to write to the (now-present) new card.
  if (this->stale_vfs_) {
    ESP_LOGI(TAG, "Cleaning up stale VFS from previous hot removal");
    // card_ was nulled in mark_card_removed_, pass nullptr
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, nullptr);
    this->stale_vfs_ = false;
  }

  ESP_LOGI(TAG, "Mounting SD card on SPI2_HOST, CS=GPIO%d...", this->cs_pin_);

  // Step 1: Add SD card as a device on the EXISTING SPI bus.
  // ESPHome already initialized SPI2_HOST for the display, so we must NOT
  // call spi_bus_initialize() again. Instead we attach the SD card as
  // another device sharing the bus (arbitrated by CS pins).
  sdspi_device_config_t dev_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  dev_config.gpio_cs = static_cast<gpio_num_t>(this->cs_pin_);
  dev_config.host_id = SPI2_HOST;

  esp_err_t ret = sdspi_host_init_device(&dev_config, &this->sdspi_handle_);
  if (ret != ESP_OK) {
    this->mount_error_ = std::string("sdspi_host_init_device: ") + esp_err_to_name(ret);
    ESP_LOGW(TAG, "SD device init failed: %s (0x%x)", esp_err_to_name(ret), ret);
    this->mounted_ = false;
    return;
  }
  ESP_LOGI(TAG, "SD SPI device attached (handle=%d)", (int) this->sdspi_handle_);

  // Step 2: Build the host driver struct pointing at the existing device.
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = this->sdspi_handle_;

  // Step 3: Mount FAT32 filesystem (auto-format if needed for LFN + directory support).
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = true;
  mount_config.max_files = 3;
  mount_config.allocation_unit_size = 16 * 1024;

  ret = esp_vfs_fat_sdspi_mount(
      MOUNT_POINT, &host, &dev_config, &mount_config, &this->card_);
  if (ret != ESP_OK) {
    this->mount_error_ = std::string("esp_vfs_fat_sdspi_mount: ") + esp_err_to_name(ret);
    ESP_LOGW(TAG, "SD card mount failed: %s (0x%x)", esp_err_to_name(ret), ret);
    sdspi_host_remove_device(this->sdspi_handle_);
    this->mounted_ = false;
    return;
  }

  // Build diagnostic trail (readable via SD Card Status sensor)
  std::string diag;

  // Diagnostic: verify root is writable
  FILE *test = fopen("/sdcard/test.txt", "w");
  if (test == nullptr) {
    this->mount_error_ = std::string("root write failed e=") + std::to_string(errno);
    ESP_LOGE(TAG, "Cannot write to /sdcard/: errno=%d (%s)", errno, strerror(errno));
    this->mounted_ = false;
    return;
  }
  fprintf(test, "ok\n");
  fclose(test);
  remove("/sdcard/test.txt");
  diag += "write:ok";
  ESP_LOGI(TAG, "Root write test passed");

  // Step 4: Create directory tree using FatFS f_mkdir directly.
  // POSIX mkdir() returns ENOSYS on esp-idf FAT VFS, so use FatFS API.
  // Drive "0:" maps to the VFS mount at /sdcard.
  FRESULT fres;

  fres = f_mkdir("0:/ovoeasy");
  if (fres == FR_OK) {
    diag += " mkdir1:ok";
    ESP_LOGI(TAG, "Created %s", BASE_DIR);
  } else if (fres == FR_EXIST) {
    diag += " dir1:exists";
  } else {
    diag += " mkdir1:FAIL(fr=" + std::to_string(fres) + ")";
    ESP_LOGE(TAG, "f_mkdir ovoeasy failed: FRESULT=%d", fres);
  }

  fres = f_mkdir("0:/ovoeasy/logs");
  if (fres == FR_OK) {
    diag += " mkdir2:ok";
    ESP_LOGI(TAG, "Created %s", LOG_DIR);
  } else if (fres == FR_EXIST) {
    diag += " dir2:exists";
  } else {
    diag += " mkdir2:FAIL(fr=" + std::to_string(fres) + ")";
    ESP_LOGE(TAG, "f_mkdir ovoeasy/logs failed: FRESULT=%d", fres);
  }

  this->mounted_ = true;
  this->mount_error_ = diag;  // Store diagnostic trail even on success
  sdmmc_card_print_info(stdout, this->card_);
  ESP_LOGI(TAG, "SD card mounted: %s", diag.c_str());
}

void SdLogger::unmount_card_() {
  if (!this->mounted_) {
    return;
  }
  esp_vfs_fat_sdcard_unmount(MOUNT_POINT, this->card_);
  sdspi_host_remove_device(this->sdspi_handle_);
  this->card_ = nullptr;
  this->sdspi_handle_ = 0;
  this->mounted_ = false;
  this->current_file_date_.clear();
  this->current_file_path_.clear();
  ESP_LOGI(TAG, "SD card unmounted");
}

void SdLogger::mark_card_removed_() {
  // "Soft" unmount — card is physically gone, so don't call
  // esp_vfs_fat_sdcard_unmount() which may try to flush dirty
  // buffers to the missing card and crash/panic.
  // sdspi_host_remove_device is safe — it only frees the ESP32's
  // SPI device slot, no card communication.
  if (this->sdspi_handle_ != 0) {
    sdspi_host_remove_device(this->sdspi_handle_);
  }
  this->mounted_ = false;
  this->card_ = nullptr;
  this->sdspi_handle_ = 0;
  this->current_file_date_.clear();
  this->current_file_path_.clear();
  this->mount_error_ = "Card removed";
  this->stale_vfs_ = true;  // Clean up VFS on next successful mount
  ESP_LOGW(TAG, "SD card removed (soft unmount)");
}

bool SdLogger::check_card_present_() {
  if (this->card_ == nullptr) {
    return false;
  }
  // CMD13 and f_stat are unreliable for detecting card removal:
  // - CMD13: MISO floats high over SPI, driver sees "valid" response
  // - f_stat: FatFS caches directory metadata in RAM, never touches card
  // Read a raw sector to force actual SPI data transfer.
  uint8_t buf[512];
  esp_err_t ret = sdmmc_read_sectors(this->card_, buf, 0, 1);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "SD card not responding: %s", esp_err_to_name(ret));
    this->mark_card_removed_();
    return false;
  }
  return true;
}

void SdLogger::loop() {
  uint32_t now = millis();

  // If not mounted, attempt re-mount every 10 seconds
  if (!this->mounted_) {
    if ((now - this->last_remount_attempt_ms_) >= 10000) {
      this->last_remount_attempt_ms_ = now;
      this->mount_card_();
      if (this->mounted_) {
        this->cleanup_old_files_();
      }
    }
    return;
  }

  // Health check every 2s — updates status for UI, detects card removal
  if ((now - this->last_health_check_ms_) >= 2000) {
    this->last_health_check_ms_ = now;
    this->check_card_present_();
    if (!this->mounted_) {
      return;
    }
  }

  if ((now - this->last_flush_ms_) >= this->flush_interval_ms_ && !this->buffer_.empty()) {
    this->flush_buffer_();
    this->last_flush_ms_ = now;
  }
}

void SdLogger::log_row(const std::string &csv_line) {
  if (!this->mounted_) {
    ESP_LOGI(TAG, "log_row called but not mounted, discarding");
    return;
  }

  if (this->buffer_.size() >= MAX_BUFFER_ROWS) {
    ESP_LOGI(TAG, "Buffer full (%u rows), force flushing", (unsigned) MAX_BUFFER_ROWS);
    this->flush_buffer_();
    this->last_flush_ms_ = millis();
  }

  this->buffer_.push_back(csv_line);
  ESP_LOGI(TAG, "Buffered row %u: %.60s...", (unsigned) this->buffer_.size(), csv_line.c_str());
}

void SdLogger::flush_buffer_() {
  if (this->buffer_.empty() || !this->mounted_) {
    return;
  }

  // Verify card is still physically present before writing
  if (!this->check_card_present_()) {
    ESP_LOGW(TAG, "Flush aborted: card removed (%u rows buffered)", (unsigned) this->buffer_.size());
    return;
  }

  this->rotate_file_if_needed_();

  FILE *f = fopen(this->current_file_path_.c_str(), "a");
  if (f == nullptr) {
    ESP_LOGE(TAG, "Failed to open %s: errno=%d (%s)",
             this->current_file_path_.c_str(), errno, strerror(errno));
    // I/O error likely means card was removed — soft unmount, keep buffer for re-mount
    this->mark_card_removed_();
    return;
  }

  // Write header if file is new (empty)
  fseek(f, 0, SEEK_END);
  if (ftell(f) == 0) {
    fprintf(f, "%s\n", CSV_HEADER);
  }

  for (const auto &row : this->buffer_) {
    fprintf(f, "%s\n", row.c_str());
  }

  fclose(f);
  ESP_LOGI(TAG, "Flushed %u rows to %s", (unsigned) this->buffer_.size(),
           this->current_file_path_.c_str());
  this->buffer_.clear();
}

void SdLogger::rotate_file_if_needed_() {
  std::string today = this->current_date_string_();
  if (today == this->current_file_date_) {
    return;
  }

  this->current_file_date_ = today;
  this->current_file_path_ = std::string(LOG_DIR) + "/ovo-" + today + ".csv";
  ESP_LOGI(TAG, "Log file rotated to %s", this->current_file_path_.c_str());
}

std::string SdLogger::current_date_string_() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Fallback if RTC/NTP not synced (year < 2024)
  if (timeinfo.tm_year < 124) {
    return "nodate";
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return std::string(buf);
}

void SdLogger::cleanup_old_files_() {
  DIR *dir = opendir(LOG_DIR);
  if (dir == nullptr) {
    return;
  }

  time_t now = time(nullptr);
  struct tm now_tm;
  localtime_r(&now, &now_tm);

  // Skip cleanup if time not synced
  if (now_tm.tm_year < 124) {
    closedir(dir);
    return;
  }

  time_t cutoff = now - (this->retention_days_ * 86400);
  int removed = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Only process files matching ovo-YYYY-MM-DD.csv pattern (18 chars)
    std::string name(entry->d_name);
    if (name.length() != 18 || name.substr(0, 4) != "ovo-" || name.substr(14) != ".csv") {
      continue;
    }

    // Parse date from filename (ovo-YYYY-MM-DD.csv)
    int y, m, d;
    if (sscanf(name.c_str(), "ovo-%d-%d-%d.csv", &y, &m, &d) != 3) {
      continue;
    }
    struct tm file_tm = {};
    file_tm.tm_year = y - 1900;
    file_tm.tm_mon = m - 1;
    file_tm.tm_mday = d;
    time_t file_time = mktime(&file_tm);

    if (file_time < cutoff) {
      std::string path = std::string(LOG_DIR) + "/" + name;
      if (remove(path.c_str()) == 0) {
        removed++;
      }
    }
  }

  closedir(dir);
  if (removed > 0) {
    ESP_LOGI(TAG, "Cleaned up %d old log files (retention: %d days)", removed, this->retention_days_);
  }
}

const char *SdLogger::get_status() const {
  if (!this->mount_attempted_) {
    return "Initializing";
  }
  if (this->mounted_) {
    // mount_error_ holds diagnostic trail on success (e.g. "write:ok dir1:exists dir2:exists")
    return this->mount_error_.empty() ? "OK" : this->mount_error_.c_str();
  }
  // Not mounted — mount_error_ holds failure reason (e.g. "Card removed")
  return this->mount_error_.empty() ? "No SD Card" : this->mount_error_.c_str();
}

void SdLogger::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Logger:");
  ESP_LOGCONFIG(TAG, "  CS Pin: GPIO%d", this->cs_pin_);
  ESP_LOGCONFIG(TAG, "  Retention: %d days", this->retention_days_);
  ESP_LOGCONFIG(TAG, "  Flush Interval: %us", this->flush_interval_ms_ / 1000);
  ESP_LOGCONFIG(TAG, "  Status: %s", this->get_status());
}

}  // namespace sd_logger
}  // namespace esphome
