from pathlib import Path
import shutil
import subprocess
import sys
import wave
import os

base = Path('tests/.build')
base.mkdir(parents=True, exist_ok=True)
component = Path('components/birdcam')
for name in ('audio_recorder.h', 'audio_recorder.cpp'):
    shutil.copy2(component / name, base / name)
stubs = {
    'esphome/core/component.h': r'''
#pragma once
namespace esphome { namespace setup_priority { constexpr float AFTER_CONNECTION=200; }
class Component { public: virtual ~Component() = default; virtual void setup(){}; virtual void loop(){};
virtual float get_setup_priority() const {return 0;} bool is_failed() const{return failed_;}
void mark_failed(){failed_=true;} bool failed_=false; }; }
''',
    'esphome/core/hal.h': '#pragma once\n#include "birdcam.h"\n',
    'esphome/core/log.h': '#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n',
    'esphome/components/microphone/microphone_source.h': r'''
#pragma once
#include <vector>
#include <functional>
#include <cstdint>
namespace esphome::microphone {
class MicrophoneSource { public:
  template<class F> void add_data_callback(F f){callback=f;}
  bool is_running() const{return running;} void start(){running=true;} void stop(){running=false;}
  void feed(const std::vector<uint8_t>&data){if(running) callback(data);}
  bool running=false; std::function<void(const std::vector<uint8_t>&)> callback;
}; }
''',
    'birdcam.h': r'''
#pragma once
#include "esphome/core/component.h"
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <functional>
inline uint32_t fake_now=0; inline uint32_t millis(){return fake_now;}
inline bool fail_allocation=false;
constexpr int MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2, pdTRUE=1, portMAX_DELAY=-1;
struct Mutex {bool busy=false;}; using SemaphoreHandle_t=Mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new Mutex;}
inline int xSemaphoreTake(Mutex*m,int){if(m->busy)return 0;m->busy=true;return 1;}
inline void xSemaphoreGive(Mutex*m){m->busy=false;}
inline void *heap_caps_malloc(size_t n,int){return fail_allocation ? nullptr : std::malloc(n);}
using esp_err_t=int; constexpr int ESP_OK=0, ESP_FAIL=-1, HTTPD_RESP_USE_STRLEN=-1;
struct httpd_req_t {void*user_ctx; std::string payload,status,type; std::function<void()> hook;};
inline void httpd_resp_set_status(httpd_req_t*r,const char*s){r->status=s;}
inline void httpd_resp_set_type(httpd_req_t*r,const char*s){r->type=s;}
inline void httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){}
inline int httpd_resp_send(httpd_req_t*r,const char*s,int){r->payload=s;return ESP_OK;}
inline int httpd_resp_send_chunk(httpd_req_t*r,const char*s,size_t n){if(s)r->payload.append(s,n);if(r->hook){auto f=r->hook;r->hook=nullptr;f();}return ESP_OK;}
namespace esphome::birdcam {struct Photo {uint8_t*data=nullptr;size_t length=0;~Photo(){std::free(data);}};}
''',
    'tests.cpp': r'''
#include "audio_recorder.h"
#include <cassert>
#include <fstream>
using namespace esphome::birdcam;
class TestRecorder:public AudioRecorder {public:
  void force_busy(bool v){mutex_->busy=v;}
};
static std::vector<uint8_t> pcm(4096,0x12);
static void complete(TestRecorder&r,esphome::microphone::MicrophoneSource&m){while(r.recording())m.feed(pcm);r.loop();}
static std::string download(TestRecorder&r){httpd_req_t request{&r};assert(r.wav_handler(&request)==ESP_OK);assert(request.type=="audio/wav");return request.payload;}
int main(){
  TestRecorder r; esphome::microphone::MicrophoneSource m; r.set_microphone(&m); r.setup(); r.loop();
  assert(!m.running); assert(!r.trigger("Manuale"));
  httpd_req_t empty{&r};r.wav_handler(&empty);assert(empty.status=="404 Not Found");
  r.set_mode(4);r.loop(); assert(m.running); assert(!r.trigger("PIR"));
  assert(r.trigger("Manuale")); assert(!r.trigger("Manuale"));
  complete(r,m); assert(r.count()==1); assert(r.bytes()==576044);
  std::string first=download(r);std::ofstream("tests/.build/result.wav",std::ios::binary).write(first.data(),first.size());
  fake_now=59999;assert(!r.trigger("Manuale"));fake_now=60000;assert(r.trigger("Manuale"));
  r.set_mode(0);r.loop();assert(!r.recording());assert(!m.running);assert(r.errors()==1);assert(download(r)==first);
  r.set_mode(1);r.loop();fake_now=120000;assert(!r.trigger("Audio"));assert(r.trigger("PIR"));
  // Changing the duration applies to the next clip, not this recording's timeout.
  r.set_duration(3);fake_now+=9000;r.loop();assert(r.recording());complete(r,m);assert(r.bytes()==576044);
  fake_now=180000;assert(r.trigger("PIR"));assert(r.recording());
  r.force_busy(true);m.feed(pcm);r.force_busy(false);complete(r,m);
  assert(r.errors()==2);assert(r.count()==2);assert(download(r)==first);
  fake_now=240000;fail_allocation=true;assert(!r.trigger("PIR"));fail_allocation=false;
  assert(r.errors()==3);assert(!r.trigger("PIR"));assert(download(r)==first);
  fake_now=300000;assert(r.trigger("PIR"));fake_now+=8001;r.loop();assert(!r.recording());assert(r.errors()==4);
  fake_now=360000;assert(r.trigger("PIR"));
  // Hold an HTTP reader to the previous clip while a new clip replaces it.
  httpd_req_t held{&r}; held.hook=[&](){complete(r,m);};r.wav_handler(&held);
  assert(held.payload==first);assert(r.bytes()==288044);assert(download(r).size()==288044);
  r.set_mode(2);r.loop();fake_now=420000;assert(!r.trigger("PIR"));assert(r.trigger("Audio"));complete(r,m);
  assert(r.count()==4);assert(r.errors()==4);
  return 0;
}
'''
}
for name, contents in stubs.items():
    path=base/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(contents,encoding='utf-8')
command=[os.environ.get('CXX', 'c++'),'-std=c++17','-O1','-I'+str(base.resolve()),str(base/'audio_recorder.cpp'),str(base/'tests.cpp'),'-o',str(base/'tests.exe')]
result=subprocess.run(command,capture_output=True,text=True)
print(result.stdout+result.stderr)
if result.returncode:raise SystemExit(result.returncode)
subprocess.run([str((base/'tests.exe').resolve())],check=True)
with wave.open(str(base/'result.wav')) as f:
    assert (f.getnchannels(),f.getsampwidth(),f.getframerate(),f.getnframes())==(1,2,48000,288000)
    assert f.readframes(4)==b'\x12\x12'*4
print('PASS: WAV real header/PCM, disabled default, mode filters, shared cooldown, no queue, cancel, immutable duration, memory failure, timeout, dropped blocks, download during replacement')
