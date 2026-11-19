// The phone/Mac remote and setup pages, served from the device on port 80.
#include "remote.h"
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "app_data.h"
#include "backend.h"
#include "camera.h"

namespace {

WebServer server(80);
QueueHandle_t events;

// One self-contained page: a draggable knob (20 clicks per turn, tap = press, hold = back), keyboard
// shortcuts for the Mac, and two text boxes. Events are sent in order, one request at a time.
const char REMOTE_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Remi remote</title><style>
:root{--bg:#F2C230;--ink:#0E1440;--cobalt:#1734BE;--pink:#C2146C;--card:#fff}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--ink);font:16px/1.4 -apple-system,system-ui,sans-serif;
display:flex;flex-direction:column;align-items:center;padding:20px 16px 32px;gap:18px;user-select:none}
h1{margin:4px 0 0;font-size:22px;font-weight:800}p{margin:0}.hint{font-size:13px;opacity:.7;text-align:center}
#knob{width:min(72vw,300px);aspect-ratio:1;border-radius:50%;background:radial-gradient(circle at 50% 40%,#3b55d0,var(--cobalt));
box-shadow:0 10px 30px rgba(14,20,64,.35),inset 0 -6px 0 rgba(0,0,0,.18);position:relative;touch-action:none;cursor:grab}
#knob::after{content:"";position:absolute;inset:22%;border-radius:50%;background:var(--bg);box-shadow:inset 0 3px 8px rgba(0,0,0,.2)}
#dot{position:absolute;left:50%;top:6%;width:14px;height:14px;margin-left:-7px;border-radius:50%;background:#fff;z-index:1}
#knob.down{transform:scale(.97)}
.row{display:flex;gap:10px;width:min(100%,420px)}
button{flex:1;font:600 15px system-ui;border:0;border-radius:999px;min-height:48px;background:var(--cobalt);color:#fff}
button.alt{background:#fff;color:var(--ink)}button:active{transform:scale(.97)}
form{display:flex;gap:8px;width:min(100%,420px)}input{flex:1;min-width:0;font:16px system-ui;border:0;border-radius:14px;padding:12px 14px}
form button{flex:0 0 auto;padding:0 18px}#status{font-size:13px;min-height:18px}
a{color:var(--cobalt);font-weight:600}
#snap{width:min(100%,420px);display:flex;flex-direction:column;gap:8px}#snap .row{width:100%}#snap button{font-size:14px;padding:0 6px}
</style></head><body>
<h1>Remi remote</h1><p class="hint">Turn the knob, tap it to press, hold it to go back.<br>On a Mac: arrow keys, Space, Esc.</p>
<div id="knob" aria-label="Remi knob" role="slider"><div id="dot"></div></div>
<div class="row"><button data-k="l">Left</button><button data-k="p">Press</button><button data-k="r">Right</button><button class="alt" data-k="h">Hold</button></div>
<form id="add"><input name="q" placeholder="Add to shopping list, e.g. milk" autocomplete="off"><button>Add</button></form>
<form id="ask"><input name="q" placeholder="Ask Remi, e.g. what can I cook?" autocomplete="off"><button>Ask</button></form>
<section id="snap"><p><b>Scan with your phone</b></p>
<div class="row"><button class="alt" data-m="0">Barcode</button><button class="alt" data-m="1">Label</button><button class="alt" data-m="2">List</button><button class="alt" data-m="3">Receipt</button></div>
<input id="cam" type="file" accept="image/*" capture="environment" hidden></section>
<p id="status"></p><p class="hint"><a href="/setup">Wi-Fi and account setup</a></p>
<script>
const st=document.getElementById('status');let chain=Promise.resolve();
function send(path){chain=chain.then(()=>fetch(path,{method:'POST'})).then(r=>{st.textContent=r.ok?'':'Remi didn\'t answer'}).catch(()=>{st.textContent='Can\'t reach Remi'})}
function key(k){if(navigator.vibrate)navigator.vibrate(k=='h'?30:8);send('/input?k='+k)}
const knob=document.getElementById('knob');let drag=null,angle=0;
function ang(e){const r=knob.getBoundingClientRect();return Math.atan2(e.clientY-r.top-r.height/2,e.clientX-r.left-r.width/2)}
knob.onpointerdown=e=>{knob.setPointerCapture(e.pointerId);knob.classList.add('down');drag={last:ang(e),acc:0,moved:0,held:false};
drag.timer=setTimeout(()=>{if(drag&&drag.moved<.25){drag.held=true;key('h')}},600)};
knob.onpointermove=e=>{if(!drag)return;const a=ang(e);let d=a-drag.last;if(d>Math.PI)d-=2*Math.PI;if(d<-Math.PI)d+=2*Math.PI;
drag.last=a;drag.acc+=d;drag.moved+=Math.abs(d);const step=Math.PI/10;
while(drag.acc>step){drag.acc-=step;angle+=18;key('r')}while(drag.acc<-step){drag.acc+=step;angle-=18;key('l')}
knob.style.rotate=angle+'deg'};
knob.onpointerup=knob.onpointercancel=()=>{if(!drag)return;clearTimeout(drag.timer);knob.classList.remove('down');
if(!drag.held&&drag.moved<.25)key('p');drag=null};
document.querySelectorAll('[data-k]').forEach(b=>b.onclick=()=>key(b.dataset.k));
addEventListener('keydown',e=>{if(e.target.tagName=='INPUT')return;const k={ArrowLeft:'l',ArrowRight:'r',' ':'p',Enter:'p',Escape:'h',Backspace:'h'}[e.key];
if(k){e.preventDefault();if(k=='l')angle-=18;if(k=='r')angle+=18;knob.style.rotate=angle+'deg';key(k)}});
for(const id of['add','ask'])document.getElementById(id).onsubmit=e=>{e.preventDefault();const i=e.target.q;if(!i.value.trim())return;
send('/text?kind='+(id=='add'?'a':'q')+'&q='+encodeURIComponent(i.value.trim()));i.value='';i.blur()};
// Phone camera: shrink to 1024 px JPEG on the phone so the upload stays small, then Remi reads it.
let mode=0;const cam=document.getElementById('cam');
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>{mode=b.dataset.m;cam.click()});
cam.onchange=async()=>{const f=cam.files[0];cam.value='';if(!f)return;st.textContent='Sending to Remi...';
try{const img=await createImageBitmap(f);const s=Math.min(1,1024/Math.max(img.width,img.height));
const c=document.createElement('canvas');c.width=Math.round(img.width*s);c.height=Math.round(img.height*s);
c.getContext('2d').drawImage(img,0,0,c.width,c.height);const blob=await new Promise(r=>c.toBlob(r,'image/jpeg',.82));
const fd=new FormData();fd.append('photo',blob,'photo.jpg');
const r=await fetch('/photo?mode='+mode,{method:'POST',body:fd});st.textContent=r.ok?'Remi is reading it':'Remi couldn\'t take the photo'}
catch(e){st.textContent='Can\'t reach Remi'}};
</script></body></html>)HTML";

String esc(const String &s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else if (c == '&') o += "&amp;";
    else o += c;
  }
  return o;
}

// One photo from the phone waits here until the backend task takes it for reading.
SemaphoreHandle_t photoLock;
Photo pending{nullptr, 0, false};
uint8_t *upload = nullptr;
size_t uploadLen = 0, uploadCap = 0;
bool uploadPng = false;

void handlePhotoChunk() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    free(upload);
    uploadCap = psramFound() ? 3 * 1024 * 1024 : 200 * 1024;  // shortcut: no PSRAM caps photos at 200 KB; the page sends ~150 KB
    upload = (uint8_t *)(psramFound() ? ps_malloc(uploadCap) : malloc(uploadCap));
    uploadLen = 0;
    uploadPng = u.type == "image/png";
  } else if (u.status == UPLOAD_FILE_WRITE && upload) {
    if (uploadLen + u.currentSize > uploadCap) { free(upload); upload = nullptr; return; }  // too big: drop it
    memcpy(upload + uploadLen, u.buf, u.currentSize);
    uploadLen += u.currentSize;
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    free(upload); upload = nullptr;
  }
}

void push(char kind, const String &text);

void handlePhotoDone() {
  int mode = server.arg("mode").toInt();
  if (!upload || !uploadLen || mode < 0 || mode > 3) {
    free(upload); upload = nullptr;
    server.send(400, "text/plain", "Send one photo under 3 MB");
    return;
  }
  xSemaphoreTake(photoLock, portMAX_DELAY);
  free(pending.bytes);  // a newer photo replaces one that wasn't read yet
  pending = {upload, uploadLen, uploadPng};
  xSemaphoreGive(photoLock);
  upload = nullptr;
  push('s', String(mode));
  server.send(204);
}

void push(char kind, const String &text = "") {
  RemoteEvent e{kind, {}};
  snprintf(e.text, sizeof e.text, "%s", text.c_str());
  xQueueSend(events, &e, 0);
}

void handleSetupPage() {
  DeviceConfig c = loadConfig();
  String html = F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
                  "<title>Remi setup</title><style>body{font:16px system-ui;background:#F2C230;color:#0E1440;margin:0;padding:20px 16px}"
                  "form{max-width:420px;margin:auto;display:grid;gap:10px}label{font-weight:600;font-size:14px}"
                  "input{font:16px system-ui;border:0;border-radius:12px;padding:12px}button{font:600 16px system-ui;border:0;"
                  "border-radius:999px;padding:14px;background:#1734BE;color:#fff;margin-top:8px}p{font-size:13px;opacity:.75}"
                  "</style></head><body><form method=post action=/setup><h2>Remi setup</h2>");
  html += "<label>Wi-Fi name</label><input name=ssid value=\"" + esc(c.ssid) + "\" required>";
  html += F("<label>Wi-Fi password</label><input name=wpass type=password placeholder='unchanged if empty'>");
  html += "<label>Remi API address</label><input name=api value=\"" + esc(c.apiUrl) +
          "\" placeholder='http://192.168.1.7:3000/graphql'>";
  html += "<label>Remi account email</label><input name=email type=email value=\"" + esc(c.email) + "\">";
  html += F("<label>Remi password</label><input name=pass type=password placeholder='unchanged if empty'>");
  html += "<label>Time zone (POSIX)</label><input name=tz value=\"" + esc(c.tz) + "\">";
  html += F("<p>Remi signs in once and keeps only a refresh token; the password is deleted from the device after the "
            "first sign-in.</p><button>Save and restart</button><p><a href=/>Back to the remote</a></p></form></body></html>");
  server.send(200, "text/html", html);
}

void handleSetupSave() {
  DeviceConfig c = loadConfig();
  c.ssid = server.arg("ssid");
  c.wifiPass = server.arg("wpass");
  c.apiUrl = server.arg("api");
  c.email = server.arg("email");
  c.password = server.arg("pass");
  c.tz = server.arg("tz").length() ? server.arg("tz") : c.tz;
  saveConfig(c);
  server.send(200, "text/html",
              F("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><body style='font:18px "
                "system-ui;background:#F2C230;padding:24px'>Saved. Remi is restarting and joining your Wi-Fi. Reconnect "
                "your phone to your home Wi-Fi, then scan the code on Remi's Connect page.</body>"));
  delay(800);
  ESP.restart();
}

void task(void *) {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", REMOTE_HTML); });
  server.on("/input", HTTP_POST, [] {
    String k = server.arg("k");
    if (k.length() == 1 && strchr("lrph", k[0])) { push(k[0]); server.send(204); }
    else server.send(400, "text/plain", "k must be l, r, p or h");
  });
  server.on("/text", HTTP_POST, [] {
    String kind = server.arg("kind"), q = server.arg("q");
    q.trim();
    if ((kind == "a" || kind == "q") && q.length() && q.length() < 48) { push(kind[0], q); server.send(204); }
    else server.send(400, "text/plain", "kind must be a or q, and q 1-47 characters");
  });
  server.on("/photo", HTTP_POST, handlePhotoDone, handlePhotoChunk);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/setup", HTTP_POST, handleSetupSave);
  server.onNotFound([] {  // phones probing for a captive portal land on the remote
    server.sendHeader("Location", "/");
    server.send(302);
  });
  bool started = false;
  for (;;) {
    if (!started && (WiFi.status() == WL_CONNECTED || backendInSetupMode())) { server.begin(); started = true; }
    if (started) server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(started ? 2 : 200));
  }
}

}  // namespace

void remoteBegin() {
  events = xQueueCreate(16, sizeof(RemoteEvent));
  photoLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "remi-remote", 6144, nullptr, 1, nullptr, 0);
}

bool remoteNext(RemoteEvent &event) { return events && xQueueReceive(events, &event, 0) == pdTRUE; }

bool takePhoto(Photo &photo) {
  if (!photoLock) return false;
  xSemaphoreTake(photoLock, portMAX_DELAY);
  photo = pending;
  pending = {nullptr, 0, false};
  xSemaphoreGive(photoLock);
  return photo.bytes != nullptr;
}
