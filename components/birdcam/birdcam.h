#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include <esp_http_server.h>
#include <freertos/semphr.h>
#include <memory>
#include <string>
#include <cmath>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace esphome::birdcam {
struct Photo {
  uint8_t *data{nullptr};
  size_t length{0};
  ~Photo();
};

class Birdcam : public Component, public i2c::I2CDevice, public camera::CameraListener {
 public:
  void set_camera(esp32_camera::ESP32Camera *camera) { camera_ = camera; }
  void set_photo_port(uint16_t port) { photo_port_ = port; }
  float get_setup_priority() const override { return setup_priority::IO + 1.0f; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  bool trigger(const std::string &source);
  void audio_level(float dbfs);
  void set_audio_threshold(float value) { audio_threshold_ = value; }
  void set_audio_duration(float seconds) { audio_duration_ms_ = seconds * 1000; }
  void set_cooldown(float seconds) { cooldown_ms_ = seconds * 1000; }
  void set_audio_enabled(bool enabled) { audio_enabled_ = enabled; audio_above_ = false; }
  void set_pir_enabled(bool enabled) { pir_enabled_ = enabled; }
  void set_battery_saving(bool enabled) { battery_saving_ = enabled; }
  void set_usb_fps(float fps) { usb_fps_ = fps; }
  void set_battery_fps(float fps) { battery_fps_ = fps; }
  bool set_resolution(const std::string &value);
  bool set_control(const std::string &control, int value);
  bool usb_present() const { return usb_; }
  bool power_valid() const { return power_valid_; }
  bool battery_present() const { return battery_; }
  bool charging() const { return charging_; }
  bool saving() const { return power_valid_ && !usb_ && battery_saving_; }
  bool pending() const { return pending_; }
  bool camera_ready() const { return camera_started_ && !camera_->is_failed(); }
  float battery_voltage() const { return battery_voltage_; }
  float usb_voltage() const { return usb_voltage_; }
  float system_voltage() const { return system_voltage_; }
  float battery_percent() const { return battery_percent_; }
  uint32_t photo_count() const { return photo_count_; }
  uint32_t error_count() const { return errors_; }
  size_t photo_bytes() const { return photo_bytes_; }
  float capture_ms() const { return capture_ms_; }
  float photo_age() const;
  const std::string &last_source() const { return last_source_; }
  const std::string &resolution() const { return resolution_; }
  const std::string &last_error() const { return last_error_; }
 protected:
  bool bits_(uint8_t reg, uint8_t mask, uint8_t value);
  float voltage_(uint8_t reg, uint8_t high_mask);
  void poll_power_();
  void fail_capture_(const char *reason);
  void start_http_();
  static esp_err_t photo_handler_(httpd_req_t *req);
  esp32_camera::ESP32Camera *camera_{nullptr};
  SemaphoreHandle_t photo_mutex_{nullptr};
  std::shared_ptr<Photo> photo_;
  httpd_handle_t http_{nullptr};
  uint16_t photo_port_{8082};
  bool camera_started_{false}, power_valid_{false}, usb_{false}, battery_{false}, charging_{false};
  bool battery_saving_{true}, audio_enabled_{false}, pir_enabled_{true}, pending_{false};
  bool audio_above_{false}, have_event_{false};
  uint8_t discard_frames_{0};
  uint32_t power_poll_{0}, capture_start_{0}, last_event_{0}, last_photo_{0}, audio_since_{0};
  uint32_t audio_duration_ms_{200}, cooldown_ms_{5000}, photo_count_{0}, errors_{0};
  uint32_t camera_start_{0};
  float battery_voltage_{NAN}, usb_voltage_{NAN}, system_voltage_{NAN}, battery_percent_{NAN};
  float audio_threshold_{-25}, usb_fps_{10}, battery_fps_{2}, capture_ms_{NAN};
  size_t photo_bytes_{0};
  std::string pending_source_, last_source_{"nessuno"}, resolution_{"640x480"}, last_error_{"nessuno"};
};
}  // namespace esphome::birdcam
