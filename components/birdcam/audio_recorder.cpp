#include "audio_recorder.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>

namespace esphome::birdcam {
static void le16(uint8_t *p, uint16_t value) { p[0] = value; p[1] = value >> 8; }
static void le32(uint8_t *p, uint32_t value) {
  for (int i = 0; i < 4; i++) p[i] = value >> (8 * i);
}
void AudioRecorder::setup() {
  mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) { mark_failed(); return; }
  source_->add_data_callback([this](const std::vector<uint8_t> &data) { on_data_(data); });
}
float AudioRecorder::remaining() const {
  if (!have_attempt_) return 0;
  uint32_t elapsed = millis() - last_attempt_;
  return elapsed >= interval_ms_ ? 0 : (interval_ms_ - elapsed) / 1000.0f;
}
bool AudioRecorder::trigger(const std::string &source) {
  if (is_failed() || mutex_ == nullptr || mode_ == 0 || !mic_owned_) return false;
  bool allowed = source == "Manuale" || (source == "PIR" && (mode_ == 1 || mode_ == 3)) ||
                 (source == "Audio" && wants_audio());
  if (!allowed || recording_ || completed_ || remaining() > 0) return false;
  // Apply the limit to attempted starts as well: allocation failures must not spin.
  have_attempt_ = true;
  last_attempt_ = millis();
  if (!source_->is_running()) { errors_++; last_error_ = "Microfono non pronto"; return false; }
  const size_t length = 44 + 48000 * 2 * duration_s_;
  auto next = std::make_shared<Photo>();
  next->data = static_cast<uint8_t *>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (next->data == nullptr) { errors_++; last_error_ = "PSRAM insufficiente per WAV"; return false; }
  next->length = length;
  uint8_t *p = next->data;
  std::memset(p, 0, 44);
  std::memcpy(p, "RIFF", 4); le32(p + 4, length - 8);
  std::memcpy(p + 8, "WAVEfmt ", 8); le32(p + 16, 16);
  le16(p + 20, 1); le16(p + 22, 1); le32(p + 24, 48000);
  le32(p + 28, 96000); le16(p + 32, 2); le16(p + 34, 16);
  std::memcpy(p + 36, "data", 4); le32(p + 40, length - 44);
  xSemaphoreTake(mutex_, portMAX_DELAY);
  building_ = next; written_ = 44; skipped_ = 0;
  pending_source_ = source; active_duration_s_ = duration_s_; started_ = millis(); recording_ = true;
  xSemaphoreGive(mutex_);
  ESP_LOGI("birdcam_audio", "Registrazione %us da %s", duration_s_, source.c_str());
  return true;
}
void AudioRecorder::on_data_(const std::vector<uint8_t> &data) {
  if (!recording_) return;
  if (xSemaphoreTake(mutex_, 0) != pdTRUE) { skipped_++; return; }
  if (recording_ && building_) {
    size_t length = std::min(data.size(), building_->length - written_);
    length &= ~size_t(1);
    std::memcpy(building_->data + written_, data.data(), length);
    written_ += length;
    if (written_ == building_->length) { recording_ = false; completed_ = true; }
  }
  xSemaphoreGive(mutex_);
}
void AudioRecorder::cancel_(const char *reason) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  recording_ = false; completed_ = false; building_.reset();
  xSemaphoreGive(mutex_);
  errors_++; last_error_ = reason;
  ESP_LOGW("birdcam_audio", "%s", reason);
}
void AudioRecorder::loop() {
  // Start/stop only here, after every component has completed setup.
  bool wanted = mode_ != 0;
  if (wanted != mic_owned_) {
    if (!wanted && (recording_ || completed_)) cancel_("Registrazione annullata: modalita spenta");
    if (wanted) source_->start(); else source_->stop();
    mic_owned_ = wanted;
  }
  if (completed_) {
    if (skipped_ != 0) { cancel_("Blocchi audio persi; WAV scartato"); return; }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    clip_ = building_; building_.reset(); completed_ = false;
    bytes_ = clip_->length; count_++; last_source_ = pending_source_; last_error_ = "nessuno";
    xSemaphoreGive(mutex_);
    ESP_LOGI("birdcam_audio", "WAV pronto: %u byte", unsigned(bytes_));
  }
  if (recording_ && millis() - started_ > active_duration_s_ * 1000 + 5000)
    cancel_("Timeout audio; WAV scartato");
}
esp_err_t AudioRecorder::wav_handler(httpd_req_t *req) {
  auto *self = static_cast<AudioRecorder *>(req->user_ctx);
  std::shared_ptr<Photo> clip;
  xSemaphoreTake(self->mutex_, portMAX_DELAY);
  clip = self->clip_;
  xSemaphoreGive(self->mutex_);
  if (!clip) {
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "Nessuna registrazione dopo questo avvio", HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "audio/wav");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, max-age=0");
  httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=birdcam-last.wav");
  for (size_t offset = 0; offset < clip->length; offset += 4096) {
    size_t length = std::min(size_t(4096), clip->length - offset);
    if (httpd_resp_send_chunk(req, reinterpret_cast<const char *>(clip->data + offset), length) != ESP_OK)
      return ESP_FAIL;
  }
  return httpd_resp_send_chunk(req, nullptr, 0);
}
}
