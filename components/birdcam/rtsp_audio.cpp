#include "rtsp_audio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <lwip/sockets.h>
#include <lwip/tcp.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/time.h>

namespace esphome::birdcam {
static const char *const TAG_AUDIO="birdcam_rtsp";
void RtspAudio::setup() {
  mutex_=xSemaphoreCreateMutex();
  ring_=static_cast<uint8_t *>(heap_caps_malloc(CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!mutex_ || !ring_) { ESP_LOGE(TAG_AUDIO,"Buffer RTSP non disponibile"); mark_failed(); return; }
  source_->add_data_callback([this](const std::vector<uint8_t> &data) { on_data_(data); });
  if (xTaskCreate(task_entry_, "birdcam_rtsp", 8192, this, 3, &task_handle_) != pdPASS) {
    ESP_LOGE(TAG_AUDIO,"Avvio task RTSP fallito"); mark_failed();
  }
}
void RtspAudio::loop() {
  bool allowed=mode_==2 || (mode_==1 && controller_->power_valid() && controller_->usb_present());
  enabled_=allowed;
  bool wanted=allowed && playing_;
  // Microphone refcount changes happen only after all components have completed setup.
  if (wanted!=mic_owned_) {
    if (wanted) source_->start(); else source_->stop();
    mic_owned_=wanted;
  }
}
void RtspAudio::clear_() {
  xSemaphoreTake(mutex_,portMAX_DELAY); read_=write_=used_=0; buffered_=0; xSemaphoreGive(mutex_);
}
void RtspAudio::on_data_(const std::vector<uint8_t> &data) {
  if (!enabled_ || !playing_ || broken_) return;
  // Only a bounded wait for a memory copy; the network never holds this mutex.
  if (xSemaphoreTake(mutex_,1)!=pdTRUE) { dropped_++; broken_=true; return; }
  size_t n=data.size() & ~size_t(1);
  if (n>CAPACITY-used_) { dropped_++; broken_=true; xSemaphoreGive(mutex_); return; }
  uint32_t clips=0;
  for (size_t i=0;i<n;i+=2) {
    int16_t sample=int16_t(uint16_t(data[i]) | (uint16_t(data[i+1])<<8));
    if (sample==32767 || sample==-32768) ++clips;
  }
  clipped_.fetch_add(clips);
  size_t first=std::min(n,CAPACITY-write_);
  std::memcpy(ring_+write_,data.data(),first);
  std::memcpy(ring_,data.data()+first,n-first);
  write_=(write_+n)%CAPACITY; used_+=n; buffered_=used_;
  xSemaphoreGive(mutex_);
}
bool RtspAudio::send_(int fd,const void *data,size_t n) {
  const auto *p=static_cast<const uint8_t *>(data);
  uint32_t start=millis();
  while (n && enabled_ && millis()-start<300) {
    int result=::send(fd,p,n,0);
    if (result>0) { p+=result; n-=result; continue; }
    if (result<0 && errno==EINTR) continue;
    if (result<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) {
      fd_set out; FD_ZERO(&out); FD_SET(fd,&out); timeval wait{0,20000};
      if (select(fd+1,nullptr,&out,nullptr,&wait)<0 && errno!=EINTR) return false;
      continue;
    }
    return false;
  }
  return n==0;
}
void RtspAudio::task_entry_(void *arg) { static_cast<RtspAudio *>(arg)->task_(); }
void RtspAudio::task_() {
  int listener=-1;
  for (;;) {
    if (!enabled_) {
      if (listener>=0) { close(listener); listener=-1; }
      vTaskDelay(pdMS_TO_TICKS(100)); continue;
    }
    if (listener<0) {
      listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
      int yes=1;
      if (listener>=0) setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
      sockaddr_in address{}; address.sin_family=AF_INET; address.sin_port=htons(port_);
      address.sin_addr.s_addr=htonl(INADDR_ANY);
      if (listener<0 || bind(listener,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0 ||
          listen(listener,1)<0) {
        if (listener>=0) close(listener); listener=-1; errors_++;
        ESP_LOGW(TAG_AUDIO,"Avvio socket RTSP fallito; nuovo tentativo fra 2 s");
        vTaskDelay(pdMS_TO_TICKS(2000)); continue;
      }
      fcntl(listener,F_SETFL,O_NONBLOCK);
      ESP_LOGI(TAG_AUDIO,"RTSP audio su porta %u",port_);
    }
    fd_set input; FD_ZERO(&input); FD_SET(listener,&input); timeval wait{0,100000};
    if (select(listener+1,&input,nullptr,nullptr,&wait)<=0) continue;
    int fd=accept(listener,nullptr,nullptr);
    if (fd<0) continue;
    // Close the listener during the session: a second client cannot queue stale requests.
    close(listener); listener=-1;
    fcntl(fd,F_SETFL,O_NONBLOCK);
    int yes=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(yes));
    connected_=true; connections_++;
    ESP_LOGI(TAG_AUDIO,"Client RTSP connesso");
    client_(fd);
    playing_=false; connected_=false;
    shutdown(fd,SHUT_RDWR); close(fd); clear_();
    ESP_LOGI(TAG_AUDIO,"Client RTSP disconnesso");
  }
}
void RtspAudio::client_(int fd) {
  std::string input; input.reserve(4096);
  bool setup=false, marker=true;
  uint8_t rtp_channel=0, rtcp_channel=1;
  uint16_t sequence=esp_random(); uint32_t timestamp=esp_random(),ssrc=esp_random();
  const std::string session=std::to_string(esp_random());
  uint32_t last_control=millis(), last_audio=millis(), last_report=millis();
  uint32_t packet_count=0,octet_count=0;
  uint8_t pcm[rtsp::PAYLOAD], frame[rtsp::FRAME];
  broken_=false; clear_();
  auto response=[&](const std::string &seq,int code,const char *reason,
                    const std::string &headers=std::string{},const std::string &body=std::string{}) {
    std::string out="RTSP/1.0 "+std::to_string(code)+" "+reason+"\r\nCSeq: "+seq+
      "\r\nServer: Birdcam/0.3.0\r\n"+headers;
    if (!body.empty()) out+="Content-Length: "+std::to_string(body.size())+"\r\n";
    out+="\r\n"+body; return send_(fd,out.data(),out.size());
  };
  while (enabled_ && !broken_) {
    if (millis()-last_control>65000) return;
    fd_set readable; FD_ZERO(&readable); FD_SET(fd,&readable); timeval wait{0,5000};
    int result=select(fd+1,&readable,nullptr,nullptr,&wait);
    if (result<0 && errno!=EINTR) { errors_++; return; }
    if (result>0) {
      char buf[1024]; int n=recv(fd,buf,sizeof(buf),0);
      if (n==0) return;
      if (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) { errors_++; return; }
      if (n>0) input.append(buf,n);
      if (input.size()>8192) { errors_++; return; }
    }
    while (!input.empty()) {
      // Clients may interleave RTCP receiver reports with RTSP control messages.
      if (input[0]=='$') {
        if (input.size()<4) break;
        size_t n=(uint8_t(input[2])<<8)|uint8_t(input[3]);
        if (n>4096) { errors_++; return; }
        if (input.size()<n+4) break;
        input.erase(0,n+4); continue;
      }
      auto end=input.find("\r\n\r\n");
      if (end==std::string::npos) { if (input.size()>4096) return; break; }
      std::string request=input.substr(0,end+4);
      auto len=rtsp::header(request,"Content-Length");
      size_t body=0;
      if (!len.empty()) {
        if (len.size()>4 || len.find_first_not_of("0123456789")!=std::string::npos) return;
        body=std::strtoul(len.c_str(),nullptr,10); if (body>1024) return;
      }
      if (input.size()<end+4+body) break;
      input.erase(0,end+4+body); last_control=millis();
      auto seq=rtsp::header(request,"CSeq");
      if (seq.empty() || seq.size()>10 || seq.find_first_not_of("0123456789")!=std::string::npos) return;
      auto a=request.find(' '), b=request.find(' ',a+1);
      if (a==std::string::npos || b==std::string::npos || request.substr(b+1,8)!="RTSP/1.0") return;
      auto method=request.substr(0,a), uri=request.substr(a+1,b-a-1), path=rtsp::path(uri);
      auto client_session=rtsp::header(request,"Session");
      auto semicolon=client_session.find(';');
      if (semicolon!=std::string::npos) client_session.resize(semicolon);
      std::string session_header="Session: "+session+";timeout=60\r\n";
      if (method=="OPTIONS") {
        if (!response(seq,200,"OK","Public: OPTIONS, DESCRIBE, SETUP, PLAY, GET_PARAMETER, TEARDOWN\r\n")) return;
      } else if (path!="/audio" && path!="/audio/" && path!="/audio/trackID=0") {
        if (!response(seq,404,"Not Found")) return;
      } else if (method=="DESCRIBE") {
        std::string base=uri;
        if (base.back()!='/') base+='/';
        if (!response(seq,200,"OK","Content-Type: application/sdp\r\nContent-Base: "+base+"\r\n",rtsp::sdp())) return;
      } else if (method=="SETUP") {
        if (playing_) { if (!response(seq,455,"Method Not Valid in This State")) return; continue; }
        if (!rtsp::channels(rtsp::header(request,"Transport"),rtp_channel,rtcp_channel)) {
          if (!response(seq,461,"Unsupported Transport")) return;
        } else {
          setup=true;
          std::string transport="Transport: RTP/AVP/TCP;unicast;interleaved="+
            std::to_string(rtp_channel)+"-"+std::to_string(rtcp_channel)+"\r\n";
          if (!response(seq,200,"OK",transport+session_header)) return;
        }
      } else if (method=="PLAY" || method=="GET_PARAMETER" || method=="TEARDOWN") {
        if (!setup || client_session!=session) { if (!response(seq,454,"Session Not Found")) return; continue; }
        if (method=="PLAY") {
          if (!playing_) {
            clear_(); broken_=false; marker=true;
            last_audio=last_report=millis(); playing_=true;
          }
          std::string base=uri; if (base.back()!='/') base+='/';
          if (!response(seq,200,"OK",session_header+"Range: npt=0.000-\r\nRTP-Info: url="+base+
              "trackID=0;seq="+std::to_string(sequence)+";rtptime="+std::to_string(timestamp)+"\r\n")) return;
        } else {
          if (!response(seq,200,"OK",session_header)) return;
          if (method=="TEARDOWN") return;
        }
      } else if (!response(seq,405,"Method Not Allowed")) return;
    }
    if (!playing_) continue;
    bool ready=false;
    xSemaphoreTake(mutex_,portMAX_DELAY);
    if (used_>=rtsp::PAYLOAD) {
      size_t first=std::min(rtsp::PAYLOAD,CAPACITY-read_);
      std::memcpy(pcm,ring_+read_,first); std::memcpy(pcm+first,ring_,rtsp::PAYLOAD-first);
      read_=(read_+rtsp::PAYLOAD)%CAPACITY; used_-=rtsp::PAYLOAD; buffered_=used_; ready=true;
    }
    xSemaphoreGive(mutex_);
    if (ready) {
      rtsp::packet(frame,pcm,rtp_channel,sequence,timestamp,ssrc,marker);
      if (!send_(fd,frame,sizeof(frame))) { errors_++; return; }
      ++sequence; timestamp+=rtsp::SAMPLES; marker=false;
      packets_++; ++packet_count; octet_count+=rtsp::PAYLOAD; last_audio=millis();
    } else if (millis()-last_audio>8000) {
      ESP_LOGW(TAG_AUDIO,"Nessun audio dal microfono; chiusura RTSP"); errors_++; return;
    }
    if (packet_count && millis()-last_report>=5000) {
      // Compound RTCP sender report + SDES CNAME, interleaved on the negotiated channel.
      uint8_t report[56]{}; report[0]='$'; report[1]=rtcp_channel; rtsp::be16(report+2,52);
      report[4]=0x80; report[5]=200; rtsp::be16(report+6,6); rtsp::be32(report+8,ssrc);
      timeval now{}; gettimeofday(&now,nullptr);
      rtsp::be32(report+12,uint32_t(now.tv_sec)+2208988800UL);
      rtsp::be32(report+16,uint32_t((uint64_t(now.tv_usec)<<32)/1000000));
      rtsp::be32(report+20,timestamp); rtsp::be32(report+24,packet_count); rtsp::be32(report+28,octet_count);
      report[32]=0x81; report[33]=202; rtsp::be16(report+34,5); rtsp::be32(report+36,ssrc);
      report[40]=1; report[41]=12; std::memcpy(report+42,"birdcam-mic",11); report[53]='0';
      if (!send_(fd,report,sizeof(report))) { errors_++; return; }
      last_report=millis();
    }
  }
  if (broken_) { errors_++; ESP_LOGW(TAG_AUDIO,"Audio perso o rete lenta; sessione RTSP chiusa"); }
}
void RtspAudio::dump_config() {
  ESP_LOGCONFIG(TAG_AUDIO,"RTSP audio: rtsp://IP:%u/audio; L16 mono 48000 Hz, solo TCP, un client",port_);
  ESP_LOGCONFIG(TAG_AUDIO,"Buffer PSRAM %u byte; nessuna registrazione audio",unsigned(CAPACITY));
}
}  // namespace esphome::birdcam
