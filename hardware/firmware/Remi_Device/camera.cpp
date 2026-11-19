// ESP32-CAM camera (OV2640): a small live preview for the Scan viewfinder, and JPEG capture for the AI reader.
// On boards without a camera every call reports "no camera" and the Scan page uses photos from the phone.
#include <Arduino.h>
#include "app_data.h"
#include "camera.h"
#include "config.h"

#if REMI_BOARD == REMI_ESP32_CAM
#include "esp_camera.h"

namespace {
SemaphoreHandle_t lock;
enum class CamMode { Off, Preview, Jpeg } cam = CamMode::Off;
uint32_t lastPreviewMs = 0;

bool start(CamMode want) {  // AI Thinker pin map; XCLK is IO0, shared with the speaker's DIN
  if (cam == want) return true;
  if (cam != CamMode::Off) { esp_camera_deinit(); cam = CamMode::Off; }
  audioPause(true);
  camera_config_t c = {};
  c.pin_pwdn = 32; c.pin_reset = -1; c.pin_xclk = 0; c.pin_sccb_sda = 26; c.pin_sccb_scl = 27;
  c.pin_d7 = 35; c.pin_d6 = 34; c.pin_d5 = 39; c.pin_d4 = 36; c.pin_d3 = 21; c.pin_d2 = 19; c.pin_d1 = 18; c.pin_d0 = 5;
  c.pin_vsync = 25; c.pin_href = 23; c.pin_pclk = 22;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_1; c.ledc_channel = LEDC_CHANNEL_1;  // keep clear of anything else using LEDC 0
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  if (want == CamMode::Preview) { c.pixel_format = PIXFORMAT_RGB565; c.frame_size = FRAMESIZE_QQVGA; c.fb_count = 2; }
  else { c.pixel_format = PIXFORMAT_JPEG; c.frame_size = FRAMESIZE_SVGA; c.jpeg_quality = 12; c.fb_count = 1; }
  if (esp_camera_init(&c) != ESP_OK) { audioPause(false); return false; }
  cam = want;
  return true;
}

void stop() {
  if (cam == CamMode::Off) return;
  esp_camera_deinit();
  cam = CamMode::Off;
  audioPause(false);
}
}  // namespace

void cameraBegin() { lock = xSemaphoreCreateMutex(); data.hasCamera = true; }

// Called by the UI each frame while the viewfinder shows: center-crops the 160x120 frame into the region.
bool cameraPreview(uint16_t *fb, int x, int y, int w, int h) {
  if (!lock || xSemaphoreTake(lock, 0) != pdTRUE) return false;  // capture in progress: keep the last frame
  bool ok = start(CamMode::Preview);
  camera_fb_t *f = ok ? esp_camera_fb_get() : nullptr;
  if (f) {
    const uint8_t *src = f->buf;
    int ox = (int(f->width) - w) / 2, oy = (int(f->height) - h) / 2;
    for (int j = 0; j < h; ++j) {
      int sy = j + oy;
      if (sy < 0 || sy >= int(f->height)) continue;
      for (int i = 0; i < w; ++i) {
        int sx = i + ox;
        if (sx < 0 || sx >= int(f->width)) continue;
        const uint8_t *p = src + (sy * f->width + sx) * 2;
        fb[(y + j) * 128 + x + i] = uint16_t(p[0] << 8 | p[1]);  // sensor sends big-endian RGB565
      }
    }
    esp_camera_fb_return(f);
    lastPreviewMs = millis();
  }
  xSemaphoreGive(lock);
  return f != nullptr;
}

// Called from the backend task. Returns a heap (PSRAM) copy the caller frees with free().
bool cameraCaptureJpeg(uint8_t **out, size_t *len) {
  if (!lock) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  bool ok = start(CamMode::Jpeg);
  camera_fb_t *f = nullptr;
  if (ok) {
    for (int i = 0; i < 2; ++i) { f = esp_camera_fb_get(); if (i == 0 && f) { esp_camera_fb_return(f); f = nullptr; } }  // drop a stale frame
  }
  if (f) {
    *out = (uint8_t *)ps_malloc(f->len);
    if (*out) { memcpy(*out, f->buf, f->len); *len = f->len; }
    esp_camera_fb_return(f);
  }
  stop();
  xSemaphoreGive(lock);
  return f && *out;
}

// Turns the camera off once the viewfinder has been gone for a second, giving IO0 back to the speaker.
void cameraIdle() {
  if (!lock || cam != CamMode::Preview || millis() - lastPreviewMs < 1000) return;
  if (xSemaphoreTake(lock, 0) != pdTRUE) return;
  stop();
  xSemaphoreGive(lock);
}

#else
void cameraBegin() { data.hasCamera = false; }
bool cameraPreview(uint16_t *, int, int, int, int) { return false; }
bool cameraCaptureJpeg(uint8_t **, size_t *) { return false; }
void cameraIdle() {}
#endif
