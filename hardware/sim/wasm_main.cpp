// Browser build of the real Remi device UI and synth (WebAssembly, wasi-sdk).
// The page calls remi_frame() every animation frame, blits the 128x160 RGB565 framebuffer onto the 3D screen,
// feeds knob events through remi_input(), and pulls audio from remi_audio() into Web Audio.
// Kitchen data comes from the same sample backend as the Mac preview (no network in the simulator).
#include <time.h>
#include "app_data.h"
#include "gfx.h"
#include "synth.h"
#include "ui.h"

AppData data;
static uint32_t simMs = 0;
static int64_t localEpoch = 0;  // seconds, already shifted to local time by the page
static bool soundOn = true;

uint32_t nowMs() { return simMs; }
bool localTime(struct tm *out) {
  time_t t = time_t(localEpoch);
  gmtime_r(&t, out);
  return true;
}
void lockData() {}
void unlockData() {}
void playSfx(Sfx s) {
  if (soundOn) synth::play(s);
}

#include "../preview/sample_backend.h"

#define EXPORT(name) extern "C" __attribute__((export_name(name)))

static int16_t audio[4096];

EXPORT("remi_init") void remi_init(uint32_t ms) {
  simMs = ms;
  sampleData();
  synth::setVolume(0.9f);
  ui::begin();
}

EXPORT("remi_input") void remi_input(int in) { ui::input(ui::Input(in)); }  // 0 left, 1 right, 2 press, 3 hold

EXPORT("remi_frame") uint16_t *remi_frame(uint32_t ms, double localSeconds) {
  simMs = ms;
  localEpoch = int64_t(localSeconds);
  finishPending();
  ui::render();
  return gfx::fb;
}

EXPORT("remi_audio") int16_t *remi_audio(int n) {
  if (n > 4096) n = 4096;
  synth::render(audio, n);
  return audio;
}

EXPORT("remi_sound") void remi_sound(int on) { soundOn = on; }

// Typed text from the page (the phone remote's boxes): write UTF-8 into remi_text_buf(), then call remi_text.
static char textBuf[64];
EXPORT("remi_text_buf") char *remi_text_buf() { return textBuf; }
EXPORT("remi_text") void remi_text(int ask) {  // 0 = add to shopping list, 1 = ask Remi
  textBuf[sizeof textBuf - 1] = 0;
  ui::textInput(ask ? ui::TextKind::Ask : ui::TextKind::Add, textBuf);
}
EXPORT("remi_in_intro") int remi_in_intro() { return ui::inIntro(); }
