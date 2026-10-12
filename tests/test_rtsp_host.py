from pathlib import Path
import shutil, subprocess, os, socket, struct, time, threading, queue

base=Path('tests/.build')
base.mkdir(parents=True,exist_ok=True)
for name in ('rtsp_audio.cpp','rtsp_audio.h','rtsp_protocol.h'):
    shutil.copy2(Path('components/birdcam')/name,base/name)
stubs={
'esphome/core/component.h': r'''
#pragma once
namespace esphome { namespace setup_priority { constexpr float AFTER_CONNECTION=200; }
class Component {public: virtual ~Component()=default;virtual void setup(){};virtual void loop(){};
virtual void dump_config(){};virtual float get_setup_priority() const{return 0;}bool is_failed()const{return failed_;}
void mark_failed(){failed_=true;}bool failed_=false;};}
''',
'esphome/core/hal.h':'#pragma once\n#include "birdcam.h"\n',
'esphome/core/log.h':r'''
#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGCONFIG(...) ((void)0)
''',
'lwip/sockets.h':'#pragma once\n#include <sys/socket.h>\n#include <sys/select.h>\n#include <netinet/in.h>\n#include <arpa/inet.h>\n#include <unistd.h>\n',
'lwip/tcp.h':'#pragma once\n#include <netinet/tcp.h>\n',
'freertos/task.h':r'''
#pragma once
#include "birdcam.h"
using TaskHandle_t=void*;
constexpr int pdPASS=1;
inline int xTaskCreate(void(*fn)(void*),const char*,int,void*arg,int,TaskHandle_t*out){*out=(void*)1;std::thread(fn,arg).detach();return pdPASS;}
inline void vTaskDelay(unsigned ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
#define pdMS_TO_TICKS(ms) (ms)
''',
'esphome/components/microphone/microphone_source.h':r'''
#pragma once
#include <functional>
#include <vector>
#include <atomic>
#include <cstdint>
namespace esphome::microphone {class MicrophoneSource {public:
template<class F>void add_data_callback(F fn){callback=fn;}
void start(){running=true;}void stop(){running=false;}
void feed(const std::vector<uint8_t>&v){if(running)callback(v);}
std::atomic<bool> running{false};std::function<void(const std::vector<uint8_t>&)>callback;};}
''',
'birdcam.h':r'''
#pragma once
#include "esphome/core/component.h"
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
inline auto epoch=std::chrono::steady_clock::now();
inline uint32_t millis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-epoch).count();}
inline uint32_t esp_random(){static std::atomic<uint32_t>n{0x12345678};return n.fetch_add(0xabcdef);}
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,pdTRUE=1,portMAX_DELAY=-1;
using SemaphoreHandle_t=std::mutex*;
inline auto xSemaphoreCreateMutex(){return new std::mutex;}
inline int xSemaphoreTake(std::mutex*m,int t){if(t==0)return m->try_lock();m->lock();return 1;}
inline void xSemaphoreGive(std::mutex*m){m->unlock();}
inline void*heap_caps_malloc(size_t n,int){return std::malloc(n);}
namespace esphome::birdcam {class Birdcam {public:bool power_valid()const{return valid;}bool usb_present()const{return usb;}
std::atomic<bool>valid{true},usb{true};};}
''',
'server.cpp':r'''
#include "rtsp_audio.h"
#include <iostream>
#include <cassert>
using namespace esphome::birdcam;
int main(int argc,char**argv){
  uint8_t a,b;
  assert(rtsp::channels("RTP/AVP/TCP;unicast;interleaved=2-3",a,b)&&a==2&&b==3);
  assert(!rtsp::channels("RTP/AVP;unicast;client_port=2-3",a,b));
  assert(!rtsp::channels("RTP/AVP/TCP;interleaved=255-256",a,b));
  assert(!rtsp::channels("RTP/AVP/TCP;interleaved=2-2",a,b));
  assert(!rtsp::channels("RTP/AVP/TCP;interleaved=2-3junk",a,b));
  assert(!rtsp::channels("RTP/AVP/TCPevil",a,b));
  Birdcam controller;esphome::microphone::MicrophoneSource mic;RtspAudio rtsp;
  rtsp.set_controller(&controller);rtsp.set_microphone(&mic);rtsp.set_port(std::atoi(argv[1]));rtsp.setup();
  assert(!rtsp.is_failed());rtsp.loop();assert(rtsp.enabled());
  std::atomic<int>mode{2};std::atomic<bool>burst{false};
  std::thread([&](){std::string line;while(std::getline(std::cin,line)){
    if(line=="mode0")mode=0;if(line=="mode1")mode=1;if(line=="mode2")mode=2;
    if(line=="usb0")controller.usb=false;if(line=="usb1")controller.usb=true;
    if(line=="burst")burst=true;
    if(line=="stats")std::cout<<"STATS "<<rtsp.enabled()<<" "<<mic.running<<" "<<rtsp.playing()<<" "<<rtsp.dropped()<<" "<<rtsp.errors()<<std::endl;
  }}).detach();
  std::vector<uint8_t>pcm(rtsp::PAYLOAD);
  for(size_t i=0;i<pcm.size();i+=2){pcm[i]=0x34;pcm[i+1]=0x12;}
  std::cout<<"READY"<<std::endl;
  uint32_t last=0;
  for(;;){rtsp.set_mode(mode);rtsp.loop();if(millis()-last>=10){last=millis();mic.feed(pcm);}
    if(burst.exchange(false))mic.feed(std::vector<uint8_t>(70000));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
'''}
for name,text in stubs.items():
    p=base/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-pthread','-I'+str(base.resolve()),str(base/'rtsp_audio.cpp'),str(base/'server.cpp'),'-o',str(base/'server')],check=True)
with socket.socket() as temp:
    temp.bind(('127.0.0.1',0));port=temp.getsockname()[1]
process=subprocess.Popen([str((base/'server').resolve()),str(port)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
lines=queue.Queue()
threading.Thread(target=lambda:[lines.put(line.strip()) for line in process.stdout],daemon=True).start()
def command(s):process.stdin.write(s+'\n');process.stdin.flush()
def stats():
    command('stats');line=lines.get(timeout=3);assert line.startswith('STATS '),line;return list(map(int,line.split()[1:]))
def connect():
    deadline=time.monotonic()+3
    while True:
        try:s=socket.create_connection(('127.0.0.1',port),timeout=1);s.settimeout(3);return s
        except OSError:
            if time.monotonic()>deadline:raise
            time.sleep(.05)
class Client:
    def __init__(self,s):self.s=s;self.buf=b'';self.seq=0;self.frames=[];self.session=''
    def next(self):
        while True:
            if self.buf.startswith(b'$') and len(self.buf)>=4:
                n=int.from_bytes(self.buf[2:4],'big')+4
                if len(self.buf)>=n:
                    p=self.buf[:n];self.buf=self.buf[n:];return p
            elif b'\r\n\r\n' in self.buf:
                end=self.buf.index(b'\r\n\r\n')+4;head=self.buf[:end].decode();headers={}
                for line in head.split('\r\n')[1:]:
                    if ':' in line:k,v=line.split(':',1);headers[k.lower()]=v.strip()
                n=end+int(headers.get('content-length','0'))
                if len(self.buf)>=n:
                    body=self.buf[end:n];self.buf=self.buf[n:];return head,headers,body
            block=self.s.recv(8192)
            if not block:raise EOFError()
            self.buf+=block
    def request(self,method,path='/audio',headers='',fragment=False):
        self.seq+=1
        text=f'{method} rtsp://127.0.0.1:{port}{path} RTSP/1.0\r\nCSeq: {self.seq}\r\n{headers}\r\n'.encode()
        if fragment:self.s.sendall(text[:17]);time.sleep(.01);self.s.sendall(text[17:])
        else:self.s.sendall(text)
        while True:
            r=self.next()
            if isinstance(r,bytes):self.frames.append(r);continue
            head,h,body=r;assert int(h['cseq'])==self.seq
            return int(head.split()[1]),h,body
    def play(self):
        code,h,_=self.request('SETUP','/audio/trackID=0','Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n')
        assert code==200;self.session=h['session'].split(';')[0]
        code,h,_=self.request('PLAY',headers='Session: '+self.session+'\r\n');assert code==200
        return h
try:
    assert lines.get(timeout=3)=='READY'
    assert stats()[:3]==[1,0,0]
    command('mode0');time.sleep(.15)
    assert stats()[:3]==[0,0,0]
    try:socket.create_connection(('127.0.0.1',port),timeout=.2);raise AssertionError('off listener open')
    except OSError:pass
    command('mode1');time.sleep(.15)
    client=Client(connect());assert stats()[1]==0
    assert client.request('OPTIONS',fragment=True)[0]==200
    code,h,body=client.request('DESCRIBE');assert code==200 and b'L16/48000/1' in body and b'm=video' not in body
    assert client.request('DESCRIBE','/wrong')[0]==404
    assert client.request('SETUP','/audio/trackID=0','Transport: RTP/AVP;unicast;client_port=1234-1235\r\n')[0]==461
    assert client.request('PLAY')[0]==454
    info=client.play();time.sleep(.05);assert stats()[:3]==[1,1,1]
    frames=[];deadline=time.monotonic()+6
    while time.monotonic()<deadline:
        item=client.next()
        if isinstance(item,bytes):
            if item[1]==2:frames.append(item)
            else:
                assert item[1]==3 and len(item)==56 and item[5]==200 and item[33]==202
                break
    else:raise AssertionError('RTCP sender report absent')
    assert len(frames)>100
    first_seq=int.from_bytes(frames[0][6:8],'big');first_ts=int.from_bytes(frames[0][8:12],'big')
    for i,f in enumerate(frames):
        assert len(f)==976 and f[4]==0x80 and f[5]&127==96
        assert int.from_bytes(f[6:8],'big')==(first_seq+i)&65535
        assert int.from_bytes(f[8:12],'big')==(first_ts+i*480)&0xffffffff
        assert f[16:]==b'\x12\x34'*480
    assert frames[0][5]&128 and not frames[1][5]&128
    assert f'seq={first_seq}' in info['rtp-info']
    # A second client is refused while the first is active.
    try:socket.create_connection(('127.0.0.1',port),timeout=.2);raise AssertionError('second client accepted')
    except OSError:pass
    client.s.sendall(b'$\x03\x00\x04\x80\xc9\x00\x00')
    assert client.request('GET_PARAMETER',headers='Session: '+client.session+'\r\n')[0]==200
    assert client.request('TEARDOWN',headers='Session: '+client.session+'\r\n')[0]==200
    client.s.close();time.sleep(.1);assert stats()[1:3]==[0,0]
    client=Client(connect());client.play();command('usb0');time.sleep(.2);assert stats()[:3]==[0,0,0];client.s.close()
    command('mode2');time.sleep(.2);client=Client(connect());client.play();time.sleep(.1);assert stats()[:3]==[1,1,1]
    command('burst');time.sleep(.2);s=stats();assert s[2]==0 and s[3]>=1 and s[4]>=1;client.s.close()
    # Actual FFmpeg RTSP client: the ingestion path used by BirdNET-Go 20261010.
    time.sleep(.15)
    assert shutil.which('ffmpeg'), 'FFmpeg required for interoperability test'
    decoded=subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-rtsp_transport','tcp',
        '-analyzeduration','100000','-probesize','4096','-i',f'rtsp://127.0.0.1:{port}/audio',
        '-t','0.2','-acodec','pcm_s16le','-f','s16le','pipe:1'],capture_output=True,timeout=20)
    assert decoded.returncode==0, decoded.stderr.decode()
    assert len(decoded.stdout)>=4800 and len(decoded.stdout)%2==0
    assert decoded.stdout==b'\x34\x12'*(len(decoded.stdout)//2), decoded.stdout[:32]
    time.sleep(.15)
    client=Client(connect());client.play();command('mode0');time.sleep(.2);assert stats()[:3]==[0,0,0];client.s.close()
    print('PASS: actual FFmpeg decoding, RTSP server, always enabled default, explicit off and USB policy, mic only on PLAY, TCP handshake, fragmented control, SDP, UDP rejection, session validation, RTP L16 byte order/sequence/timestamps, RTCP, keepalive, teardown, disconnect, overflow, reconnect, single client')
finally:
    process.kill();process.wait()
