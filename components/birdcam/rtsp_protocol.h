#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace esphome::birdcam::rtsp {
constexpr size_t SAMPLES = 480;  // 10 ms, mono 48 kHz
constexpr size_t PAYLOAD = SAMPLES * 2;
constexpr size_t FRAME = 4 + 12 + PAYLOAD;
inline void be16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
inline void be32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = v >> (24 - 8 * i); }
inline std::string lower(std::string s) {
  for (char &c : s) c = std::tolower(static_cast<unsigned char>(c));
  return s;
}
inline std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
inline std::string header(const std::string &request, const std::string &name) {
  size_t p = request.find("\r\n");
  while (p != std::string::npos) {
    p += 2;
    size_t end = request.find("\r\n", p);
    if (end == std::string::npos || end == p) break;
    size_t colon = request.find(':', p);
    if (colon < end && lower(trim(request.substr(p, colon-p))) == lower(name))
      return trim(request.substr(colon+1, end-colon-1));
    p = end;
  }
  return {};
}
inline std::string path(const std::string &uri) {
  auto p = uri.find("://");
  if (p != std::string::npos) { p = uri.find('/', p+3); if (p == std::string::npos) return "/"; }
  else p = 0;
  return uri.substr(p);
}
inline bool channels(const std::string &transport, uint8_t &rtp, uint8_t &rtcp) {
  auto t = lower(transport);
  auto end = t.find(',');
  if (end != std::string::npos) t.resize(end);  // First advertised alternative only
  if (t.find("rtp/avp/tcp") != 0 || (t.size()>11 && t[11]!=';' && t[11]!=' ') || t.find("multicast") != std::string::npos) return false;
  auto p = t.find("interleaved=");
  unsigned a=0, b=1; int used=0;
  if (p != std::string::npos) {
    auto value = t.substr(p+12);
    if (std::sscanf(value.c_str(), "%u-%u%n", &a, &b, &used) != 2 || a>254 || b!=a+1 ||
        (size_t(used)<value.size() && value[used]!=';' && value[used]!=' ')) return false;
  }
  rtp=a; rtcp=b; return true;
}
inline std::string sdp() {
  return "v=0\r\no=- 1 1 IN IP4 0.0.0.0\r\ns=Birdcam microphone\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\nm=audio 0 RTP/AVP 96\r\na=rtpmap:96 L16/48000/1\r\na=ptime:10\r\na=control:trackID=0\r\na=sendonly\r\n";
}
inline void packet(uint8_t *out, const uint8_t *pcm_le, uint8_t channel, uint16_t seq,
                   uint32_t timestamp, uint32_t ssrc, bool marker) {
  out[0]='$'; out[1]=channel; be16(out+2, 12+PAYLOAD);
  out[4]=0x80; out[5]=96 | (marker ? 0x80 : 0); be16(out+6, seq);
  be32(out+8, timestamp); be32(out+12, ssrc);
  // RTP L16 is signed network byte order, unlike the microphone PCM buffer.
  for (size_t i=0; i<PAYLOAD; i+=2) { out[16+i]=pcm_le[i+1]; out[17+i]=pcm_le[i]; }
}
}  // namespace esphome::birdcam::rtsp
