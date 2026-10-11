#pragma once
#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone_source.h"
#include "birdcam.h"
#include <atomic>
#include <algorithm>

namespace esphome::birdcam {
class AudioRecorder : public Component {
 public:
  void set_microphone(microphone::MicrophoneSource *source) { source_ = source; }
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION - 1; }
  void setup() override;
  void loop() override;
  void set_mode(int mode) { mode_ = mode; }
  bool wants_audio() const { return mode_ == 2 || mode_ == 3; }
  bool trigger(const std::string &source);
  void set_interval(float seconds) { interval_ms_ = uint32_t(std::max(15.0f, std::min(3600.0f, seconds)) * 1000); }
  void set_duration(float seconds) { duration_s_ = uint32_t(std::max(3.0f, std::min(10.0f, seconds))); }
  bool recording() const { return recording_; }
  uint32_t count() const { return count_; }
  uint32_t errors() const { return errors_; }
  size_t bytes() const { return bytes_; }
  float remaining() const;
  const std::string &last_source() const { return last_source_; }
  const std::string &last_error() const { return last_error_; }
  static esp_err_t wav_handler(httpd_req_t *req);
 protected:
  void on_data_(const std::vector<uint8_t> &data);
  void cancel_(const char *reason);
  microphone::MicrophoneSource *source_{nullptr};
  SemaphoreHandle_t mutex_{nullptr};
  std::shared_ptr<Photo> building_, clip_;
  std::atomic<bool> recording_{false}, completed_{false};
  std::atomic<uint32_t> skipped_{0};
  bool mic_owned_{false}, have_attempt_{false};
  int mode_{0};
  uint32_t interval_ms_{60000}, duration_s_{6}, started_{0}, last_attempt_{0};
  uint32_t active_duration_s_{6};
  uint32_t count_{0}, errors_{0};
  size_t written_{0}, bytes_{0};
  std::string pending_source_, last_source_{"nessuno"}, last_error_{"nessuno"};
};
}
