#include <Arduino.h>
#include <ESP_I2S.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <atomic>
#include <math.h>
constexpr uint32_t RATE=16000, SECONDS=10, TOTAL=RATE*SECONDS;
I2SClass mic;
SPIClass spi(HSPI);
Adafruit_ST7735 tft(&spi,15,2,-1);
WebServer server(80);
GFXcanvas16 plot(120,66);
int16_t *audio=nullptr;
std::atomic<int> state{0}; // 0 live, 1 start request, 2 recording, 3 saved, 4 input error
std::atomic<uint32_t> saved{0}, failures{0};
portMUX_TYPE waveMux=portMUX_INITIALIZER_UNLOCKED;
int16_t wave[120]={};
float levelDb=-90;
int peakValue=0;
uint32_t clipped=0;
void capture(void*){
 int32_t in[256];float dc=0;int bad=0;
 for(;;){
  size_t bytes=0;
  esp_err_t err=i2s_channel_read(mic.rxChan(),in,sizeof(in),&bytes,1000);
  if(err!=ESP_OK || bytes==0){failures++;if(++bad>=3)state=4;vTaskDelay(1);continue;}
  bad=0;
  if(state==1){saved=0;state=2;}
  int n=bytes/4,pk=0;double sum=0;int16_t preview[120]={};uint32_t clips=0;
  for(int i=0;i<n;i++){
   // INMP441 supplies signed 24-bit I2S audio in the upper bits of a 32-bit slot.
   float value=float(in[i]>>8)/256.0f;
   dc+=0.003f*(value-dc);
   int v=lroundf((value-dc)*4.0f); // fixed 12 dB gain, no noise pumping
   if(v>32767 || v< -32768)clips++;
   int16_t sample=constrain(v,-32768,32767);
   pk=max(pk,abs(int(sample)));sum+=double(sample)*sample;
   if(i<120)preview[i]=sample;
   if(state==2){uint32_t pos=saved.load();if(pos<TOTAL){audio[pos]=sample;saved=pos+1;}if(saved>=TOTAL)state=3;}
  }
  float db=20*log10f(max(1.0,sqrt(sum/n))/32768.0);
  portENTER_CRITICAL(&waveMux);memcpy(wave,preview,sizeof(wave));levelDb=db;peakValue=pk;clipped=clips;portEXIT_CRITICAL(&waveMux);
 }
}
void draw(){
 int16_t w[120];float db;int pk;uint32_t clips;
 portENTER_CRITICAL(&waveMux);memcpy(w,wave,sizeof(w));db=levelDb;pk=peakValue;clips=clipped;portEXIT_CRITICAL(&waveMux);
 int st=state.load();uint32_t count=saved.load();
 tft.fillRect(0,0,128,49,0x0842);tft.setTextSize(1);tft.setCursor(5,6);tft.setTextColor(0x7BEF);tft.print("REMI / AUDIO");
 tft.setCursor(5,23);tft.setTextColor(st==2?ST77XX_RED:ST77XX_WHITE);tft.print(st==2?"RECORDING":st==3?"SAVED":st==4?"MIC ERROR":"LIVE INPUT");
 tft.setCursor(5,38);tft.setTextColor(ST77XX_WHITE);tft.printf("%02lu.%01lu / 10s",(unsigned long)(count/RATE),(unsigned long)((count%RATE)*10/RATE));
 plot.fillScreen(0x0842);for(int y=10;y<66;y+=22)plot.drawFastHLine(0,y,120,0x18E3);
 int scale=max(pk,1024);for(int x=1;x<120;x++){int a=33-constrain(int(w[x-1])*28/scale,-29,29);int b=33-constrain(int(w[x])*28/scale,-29,29);plot.drawLine(x-1,a,x,b,st==2?0xFCB0:0x4FDC);}
 tft.drawRGBBitmap(4,52,plot.getBuffer(),120,66);
 tft.fillRect(4,123,120,8,0x18E3);tft.fillRect(4,123,constrain(int((db+60)*2),0,120),8,clips?ST77XX_RED:0x4FDC);
 tft.fillRect(0,136,128,24,0x0842);tft.setCursor(5,137);tft.setTextColor(ST77XX_WHITE);tft.printf("%4.0f dBFS %s",db,clips?"CLIP":"");
 tft.setCursor(5,150);tft.setTextColor(0x7BEF);tft.print("192.168.4.1");
}
void sendWav(){
 if(state!=3){server.send(409,"text/plain","Finish a recording first.");return;}
 uint32_t length=saved*2;uint8_t h[44]={};
 auto u16=[&](int p,uint16_t v){h[p]=v;h[p+1]=v>>8;};auto u32=[&](int p,uint32_t v){for(int i=0;i<4;i++)h[p+i]=v>>(i*8);};
 memcpy(h,"RIFF",4);u32(4,36+length);memcpy(h+8,"WAVEfmt ",8);u32(16,16);u16(20,1);u16(22,1);u32(24,RATE);u32(28,RATE*2);u16(32,2);u16(34,16);memcpy(h+36,"data",4);u32(40,length);
 server.sendHeader("Cache-Control","no-store");server.setContentLength(44+length);server.send(200,"audio/wav","");auto c=server.client();c.write(h,44);auto p=(uint8_t*)audio;
 while(length && c.connected()){size_t n=c.write(p,min(size_t(2048),size_t(length)));if(!n)break;p+=n;length-=n;delay(1);}
}
const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="en"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Remi recorder</title><style>
*{box-sizing:border-box}body{margin:0;background:#111b20;color:#f0f5f2;font:17px/1.5 system-ui,sans-serif}main{max-width:680px;margin:auto;padding:32px 22px}h1{font-size:36px;letter-spacing:-1.5px;margin:0}p{color:#b5c4c9}#status{color:#8ce5d1;font-size:22px;margin-top:25px}canvas{width:100%;height:160px;background:#17272d;display:block}button{font:inherit;padding:14px 20px;border:0;border-radius:6px;cursor:pointer;background:#a3eedb;color:#102825;margin:16px 8px 8px 0}button:disabled{opacity:.45;cursor:default}#stop{background:#f5b7a3}audio{width:100%;margin:16px 0}a{color:#a3eedb}small{color:#b5c4c9}#error{color:#ffc2b0}</style>
<main><h1>Remi recorder</h1><p>INMP441 · 16 kHz mono · up to 10 seconds</p><div id="status" role="status">Connecting…</div><canvas id="wave" width="600" height="160" aria-label="Live microphone waveform"></canvas><p id="meter"></p><button id="record" disabled>Record 10 seconds</button><button id="stop" disabled>Stop &amp; save</button><p id="error" role="alert"></p><section id="result" hidden><audio id="player" controls></audio><a id="download" href="/audio.wav" download="remi-inmp441.wav">Download WAV</a></section><p><small>Speak toward the small sound hole on the microphone board. Waveform height adapts for visibility; dBFS shows the actual recording level. One recording is kept in memory and is lost on reset.</small></p></main>
<script>
const $=id=>document.getElementById(id);let previous=-1;async function action(url){try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(await r.text());$('error').textContent='';}catch(e){$('error').textContent=e.message;}}
$('record').onclick=()=>action('/record');$('stop').onclick=()=>action('/stop');
async function poll(){try{let r=await fetch('/status',{cache:'no-store'});let s=await r.json();$('status').textContent=['Live input','Starting…','Recording','Recording saved','Microphone read error'][s.state]+' · '+s.seconds.toFixed(1)+'s';$('record').disabled=[1,2,4].includes(s.state);$('stop').disabled=s.state!==2;$('meter').textContent=s.db.toFixed(0)+' dBFS · peak '+s.peak+' · read errors '+s.errors+(s.clip?' · CLIPPING':'');if(s.state===3&&previous!==3){$('result').hidden=false;$('player').src='/audio.wav?v='+Date.now();}if(s.state===1||s.state===2){$('result').hidden=true;$('player').pause();}previous=s.state;let c=$('wave').getContext('2d');c.fillStyle='#17272d';c.fillRect(0,0,600,160);c.strokeStyle='#31454d';c.beginPath();c.moveTo(0,80);c.lineTo(600,80);c.stroke();let scale=Math.max(1024,...s.wave.map(Math.abs));c.strokeStyle=s.state===2?'#f5b7a3':'#a3eedb';c.lineWidth=2;c.beginPath();s.wave.forEach((v,i)=>{let x=i*600/119,y=80-v*65/scale;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke();}catch(e){$('error').textContent='Connection lost. Rejoin Remi-Mic-Test and reload.';}setTimeout(poll,150);}poll();</script></html>)HTML";
void setup(){
 // UART is deliberately unused: GPIO1 and GPIO3 belong to the microphone.
 spi.begin(14,-1,13,-1);tft.initR(INITR_BLACKTAB);tft.setRotation(0);tft.setSPISpeed(16000000);tft.fillScreen(0x0842);
 audio=(int16_t*)ps_malloc(TOTAL*2);
 if(!audio){tft.setCursor(5,20);tft.setTextColor(ST77XX_RED);tft.print("PSRAM ERROR");while(true)delay(1000);}
 mic.setPins(12,1,-1,3);
 if(!mic.begin(I2S_MODE_STD,RATE,I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT)){tft.setCursor(5,20);tft.setTextColor(ST77XX_RED);tft.print("I2S INIT ERROR");while(true)delay(1000);}
 if(xTaskCreatePinnedToCore(capture,"mic",6144,nullptr,2,nullptr,0)!=pdPASS){tft.setCursor(5,20);tft.setTextColor(ST77XX_RED);tft.print("TASK ERROR");while(true)delay(1000);}
 WiFi.mode(WIFI_AP);WiFi.softAP("Remi-Mic-Test","remimic123");
 server.on("/",[]{server.send_P(200,"text/html",PAGE);});
 server.on("/record",HTTP_POST,[]{if(state==1||state==2||state==4){server.send(409,"text/plain","Recorder busy or input fault; reset if input failed.");return;}state=1;server.send(202,"text/plain","Recording");});
 server.on("/stop",HTTP_POST,[]{if(state==2 && saved>0)state=3;server.send(200,"text/plain","Stopped");});
 server.on("/audio.wav",sendWav);
 server.on("/status",[]{int16_t w[120];float db;int pk;uint32_t cl;portENTER_CRITICAL(&waveMux);memcpy(w,wave,sizeof(w));db=levelDb;pk=peakValue;cl=clipped;portEXIT_CRITICAL(&waveMux);String j="{\"state\":"+String(state.load())+",\"seconds\":"+String(float(saved)/RATE,2)+",\"db\":"+String(db,1)+",\"peak\":"+String(pk)+",\"clip\":"+String(cl)+",\"errors\":"+String(failures.load())+",\"wave\":[";for(int i=0;i<120;i++){if(i)j+=',';j+=String(w[i]);}j+="]}";server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",j);});server.begin();
}
void loop(){server.handleClient();static uint32_t last=0;if(millis()-last>=60){last=millis();draw();}delay(1);}
