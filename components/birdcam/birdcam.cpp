#include "birdcam.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <esp_heap_caps.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <esp_wifi.h>

namespace esphome::birdcam {
static const char *const TAG = "birdcam";
Photo::~Photo() { if (data != nullptr) heap_caps_free(data); }

bool Birdcam::bits_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t old;
  return read_byte(reg, &old) && write_byte(reg, (old & ~mask) | (value & mask));
}

void Birdcam::setup() {
  // I2C bus is ready; this runs before camera/display/microphone setup.
  uint8_t chip;
  if (!read_byte(0x03, &chip) || chip != 0x4A) {
    ESP_LOGE(TAG, "AXP2101 missing or unexpected chip ID");
    mark_failed();
    return;
  }
  // Board voltages from LILYGO's MinimalPowersExample. NEVER alter DCDC1.
  bool ok = bits_(0x92, 0x1F, 13);  // ALDO1 1.8V camera
  ok &= bits_(0x93, 0x1F, 23);      // ALDO2 2.8V camera
  ok &= bits_(0x95, 0x1F, 25);      // ALDO4 3.0V camera
  ok &= bits_(0x94, 0x1F, 28);      // ALDO3 3.3V PIR
  ok &= bits_(0x96, 0x1F, 28);      // BLDO1 3.3V microphone
  ok &= bits_(0x90, 0x1F, 0x1F);
  ok &= bits_(0x68, 0x01, 0x01);    // battery presence detection
  ok &= bits_(0x30, 0x2D, 0x2D);    // general ADC + battery/VBUS/VSYS
  // Do not change charger current, target voltage or TS protection: battery unknown.
  if (!ok) { mark_failed(); return; }
  delay(20);  // rail settling before peripheral initialization
  photo_mutex_ = xSemaphoreCreateMutex();
  if (photo_mutex_ == nullptr) { mark_failed(); return; }
  camera_->add_listener(this);
  poll_power_();
}

float Birdcam::voltage_(uint8_t reg, uint8_t mask) {
  uint8_t bytes[2];
  if (!read_bytes(reg, bytes, 2)) return NAN;
  return (((bytes[0] & mask) << 8) | bytes[1]) / 1000.0f;
}

void Birdcam::poll_power_() {
  uint8_t status[2];
  power_valid_ = read_bytes(0x00, status, 2);
  if (!power_valid_) {
    battery_voltage_ = usb_voltage_ = system_voltage_ = battery_percent_ = NAN;
    status_set_warning();
    return;
  }
  status_clear_warning();
  usb_ = (status[0] & 0x20) && !(status[1] & 0x08);
  battery_ = status[0] & 0x08;
  charging_ = (status[1] >> 5) == 1;
  battery_voltage_ = battery_ ? voltage_(0x34, 0x1F) : NAN;
  usb_voltage_ = usb_ ? voltage_(0x38, 0x3F) : NAN;
  system_voltage_ = voltage_(0x3A, 0x3F);
  uint8_t percent;
  battery_percent_ = battery_ && read_byte(0xA4, &percent) && percent <= 100 ? percent : NAN;
}

void Birdcam::loop() {
  const uint32_t now = millis();
  if (!camera_started_ && esp_camera_sensor_get() != nullptr && !camera_->is_failed()) {
    camera_started_ = true;
    camera_start_ = now;
    set_resolution(resolution_);
    start_http_();
  }
  if (now - power_poll_ >= 1000) {
    power_poll_ = now;
    poll_power_();
    // USB overrides all energy saving, including Wi-Fi modem power saving.
    esp_wifi_set_ps(saving() ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
    if (camera_started_) {
      const float fps = saving() ? battery_fps_ : usb_fps_;
      camera_->set_max_update_interval(static_cast<uint32_t>(1000.0f / fps));
      camera_->set_idle_update_interval(saving() ? 0 : 10000);
    }
  }
  if (pending_) {
    if (now - capture_start_ > 10000) fail_capture_("Timeout acquisizione");
    else camera_->request_image(camera::WEB_REQUESTER);
  }
}

bool Birdcam::trigger(const std::string &source) {
  if ((source == "PIR" && !pir_enabled_) || (source == "Audio" && !audio_enabled_)) return false;
  const uint32_t now = millis();
  if (!camera_ready() || now - camera_start_ < 2000 || pending_) return false;
  if (have_event_ && now - last_event_ < cooldown_ms_) return false;
  have_event_ = true;
  last_event_ = capture_start_ = now;
  pending_source_ = source;
  // Discard pre-trigger queued frames (one framebuffer), then allow one fresh settling frame.
  discard_frames_ = 2;
  pending_ = true;
  camera_->request_image(camera::WEB_REQUESTER);
  return true;
}

void Birdcam::audio_level(float level) {
  if (!audio_enabled_ || !std::isfinite(level) || level < audio_threshold_) {
    audio_above_ = false;
    return;
  }
  if (!audio_above_) { audio_above_ = true; audio_since_ = millis(); }
  if (millis() - audio_since_ >= audio_duration_ms_) {
    trigger("Audio");
    audio_since_ = millis();
  }
}

void Birdcam::fail_capture_(const char *reason) {
  pending_ = false;
  errors_++;
  last_error_ = reason;
  ESP_LOGW(TAG, "%s; previous photo retained", reason);
}

void Birdcam::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (!pending_) return;
  if (discard_frames_ != 0) { --discard_frames_; return; }
  const size_t length = image->get_data_length();
  const uint8_t *data = image->get_data_buffer();
  if (length < 4 || length > 1024 * 1024 || data[0] != 0xFF || data[1] != 0xD8) {
    fail_capture_("JPEG non valido o oltre 1 MiB"); return;
  }
  auto next = std::make_shared<Photo>();
  next->data = static_cast<uint8_t *>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!next->data) { fail_capture_("PSRAM insufficiente"); return; }
  next->length = length;
  memcpy(next->data, data, length);
  // Readers retain a shared_ptr: replacing photo never invalidates an HTTP transfer.
  xSemaphoreTake(photo_mutex_, portMAX_DELAY);
  photo_.swap(next);
  xSemaphoreGive(photo_mutex_);
  pending_ = false;
  last_photo_ = millis();
  capture_ms_ = last_photo_ - capture_start_;
  photo_bytes_ = length;
  photo_count_++;
  last_source_ = pending_source_;
  last_error_ = "nessuno";
  ESP_LOGI(TAG, "Photo %u: %s, %u bytes", photo_count_, last_source_.c_str(), static_cast<unsigned>(length));
}

float Birdcam::photo_age() const { return photo_count_ ? (millis() - last_photo_) / 1000.0f : NAN; }

bool Birdcam::set_resolution(const std::string &value) {
  if (!camera_ready() || pending_) return false;
  framesize_t size;
  if (value == "320x240") size = FRAMESIZE_QVGA;
  else if (value == "640x480") size = FRAMESIZE_VGA;
  else if (value == "800x600") size = FRAMESIZE_SVGA;
  else if (value == "1024x768") size = FRAMESIZE_XGA;
  else if (value == "1280x1024") size = FRAMESIZE_SXGA;
  else if (value == "1600x1200") size = FRAMESIZE_UXGA;
  else return false;
  // Driver allocated at UXGA during startup; lower sizes fit that allocation.
  sensor_t *s = esp_camera_sensor_get();
  if (s == nullptr || s->set_framesize(s, size) != 0) return false;
  resolution_ = value;
  return true;
}

bool Birdcam::set_control(const std::string &control, int value) {
  if (!camera_ready() || pending_) return false;
  sensor_t *s = esp_camera_sensor_get();
  if (s == nullptr) return false;
  int result = -1;
  if (control == "brightness" && value >= -2 && value <= 2) result = s->set_brightness(s, value);
  else if (control == "contrast" && value >= -2 && value <= 2) result = s->set_contrast(s, value);
  else if (control == "saturation" && value >= -2 && value <= 2) result = s->set_saturation(s, value);
  else if (control == "quality" && value >= 10 && value <= 63) result = s->set_quality(s, value);
  else if (control == "ae_level" && value >= -2 && value <= 2) result = s->set_ae_level(s, value);
  else if (control == "exposure" && value >= 0 && value <= 1200) result = s->set_aec_value(s, value);
  else if (control == "wb" && value >= 0 && value <= 4) result = s->set_wb_mode(s, value);
  else if (control == "mirror") result = s->set_hmirror(s, !!value);
  else if (control == "flip") result = s->set_vflip(s, !!value);
  else if (control == "aec") result = s->set_exposure_ctrl(s, !!value);
  else if (control == "agc") result = s->set_gain_ctrl(s, !!value);
  return result == 0;
}

void Birdcam::start_http_() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = photo_port_;
  config.ctrl_port = photo_port_ + 10000;
  config.max_uri_handlers = 2;
  config.max_open_sockets = 2;
  config.lru_purge_enable = true;
  config.send_wait_timeout = 3;
  if (httpd_start(&http_, &config) != ESP_OK) {
    last_error_ = "Avvio HTTP fallito"; errors_++; return;
  }
  httpd_uri_t uri{};
  uri.uri = "/last.jpg";
  uri.method = HTTP_GET;
  uri.handler = photo_handler_;
  uri.user_ctx = this;
  if (httpd_register_uri_handler(http_, &uri) != ESP_OK) {
    httpd_stop(http_); http_ = nullptr;
    last_error_ = "Registrazione HTTP fallita"; errors_++;
  }
}

esp_err_t Birdcam::photo_handler_(httpd_req_t *req) {
  auto *self = static_cast<Birdcam *>(req->user_ctx);
  std::shared_ptr<Photo> photo;
  xSemaphoreTake(self->photo_mutex_, portMAX_DELAY);
  photo = self->photo_;
  xSemaphoreGive(self->photo_mutex_);
  if (!photo) {
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "Nessuna foto dopo questo avvio", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, max-age=0");
  for (size_t offset = 0; offset < photo->length; offset += 4096) {
    size_t count = std::min(size_t(4096), photo->length - offset);
    if (httpd_resp_send_chunk(req, reinterpret_cast<const char *>(photo->data + offset), count) != ESP_OK)
      return ESP_FAIL;
  }
  return httpd_resp_send_chunk(req, nullptr, 0);
}

void Birdcam::dump_config() {
  ESP_LOGCONFIG(TAG, "LILYGO T-Camera S3 V1.6; PSRAM only; photo http://IP:%u/last.jpg", photo_port_);
  ESP_LOGCONFIG(TAG, "No deep sleep, no flash photo writes, charger configuration preserved");
}
}  // namespace esphome::birdcam
