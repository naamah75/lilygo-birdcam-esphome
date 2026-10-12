#pragma once
#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone_source.h"
#include "birdcam.h"
#include "rtsp_protocol.h"
#include <atomic>
#include <freertos/task.h>

namespace esphome::birdcam {
class RtspAudio : public Component {
 public:
  void set_microphone(microphone::MicrophoneSource *s) { source_=s; }
  void set_controller(Birdcam *b) { controller_=b; }
  void set_port(uint16_t p) { port_=p; }
  void set_mode(int m) { mode_=m; }
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION - 1; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  bool enabled() const { return enabled_; }
  bool connected() const { return connected_; }
  bool playing() const { return playing_; }
  uint32_t packets() const { return packets_; }
  uint32_t dropped() const { return dropped_; }
  uint32_t errors() const { return errors_; }
  uint32_t connections() const { return connections_; }
  uint32_t clipped() const { return clipped_; }
  size_t buffered() const { return buffered_; }
 protected:
  static void task_entry_(void *arg);
  void task_();
  void client_(int fd);
  bool send_(int fd, const void *data, size_t n);
  void on_data_(const std::vector<uint8_t> &data);
  void clear_();
  microphone::MicrophoneSource *source_{nullptr};
  Birdcam *controller_{nullptr};
  SemaphoreHandle_t mutex_{nullptr};
  TaskHandle_t task_handle_{nullptr};
  static constexpr size_t CAPACITY=48000;  // 500 ms, not a recording
  uint8_t *ring_{nullptr};
  size_t read_{0}, write_{0}, used_{0};
  uint16_t port_{8554};
  int mode_{2};  // Always available on USB and battery, independently of photo triggers.
  bool mic_owned_{false};
  std::atomic<bool> enabled_{false}, connected_{false}, playing_{false}, broken_{false};
  std::atomic<uint32_t> packets_{0}, dropped_{0}, errors_{0}, connections_{0}, clipped_{0};
  std::atomic<size_t> buffered_{0};
};
}
