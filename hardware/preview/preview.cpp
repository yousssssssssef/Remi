// Host preview: runs the real device UI + synth with sample data and a scripted knob session.
// Writes raw frames (RGB24, 128x160 @ 30 fps), a WAV, and PPM stills for key screens.
//   see render.sh (C++17 for the UI, plus the vendored qrcode.c)
//   ./preview out_dir
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "app_data.h"
#include "gfx.h"
#include "synth.h"
#include "ui.h"

AppData data;
static uint32_t simMs = 0;
uint32_t nowMs() { return simMs; }
bool localTime(struct tm *out) {
  time_t base = 1795023720;  // 2026-11-18 19:42 UTC, shown as local time
  time_t t = base + simMs / 1000;
  gmtime_r(&t, out);
  return true;
}
bool cameraPreview(uint16_t *, int, int, int, int) { return false; }
void lockData() {}
void unlockData() {}
void playSfx(Sfx s) { synth::play(s); }

#include "sample_backend.h"

struct Ev { uint32_t at; ui::Input in; const char *shot; const char *add = nullptr; const char *ask = nullptr; };

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  sampleData();
  synth::setVolume(1.0f);  // louder than the device default so the preview is easy to hear
  using I = ui::Input;
  const I R = I::Right, P = I::Press, LP = I::LongPress;
  // Script (ms after boot). The boot runs about 6.2 s on its own. Pages, left to right:
  // Home, Today, Kitchen, Just bought, Cook, Ask Remi, Connect.
  std::vector<Ev> script = {
      {7600, R, "01-home"},                                   // -> Today
      {8800, P, "02-today"}, {9800, LP, "03-today-planned"},  // planned list, back
      {10400, R, nullptr},                                    // -> Kitchen
      {11600, P, "04-kitchen"}, {12400, R, nullptr}, {13000, P, "05-kitchen-list"},  // list, 2nd item, action sheet
      {13700, R, "06-kitchen-sheet"}, {14300, P, nullptr},     // "Threw it out"
      {15600, LP, "07-kitchen-updated"},                       // back to top
      {16200, R, nullptr},                                     // -> Just bought
      {17400, P, "08-just-bought"}, {18000, P, nullptr}, {18400, R, nullptr}, {18700, P, nullptr},
      {19100, R, nullptr}, {19400, R, nullptr}, {19700, R, nullptr}, {20000, R, nullptr}, {20300, R, nullptr},
      {20800, P, "09-shopping-list"}, {22500, I::Left, "10-put-away-done"}, {24400, R, nullptr},  // -> Cook
      {25000, P, "11-cook"}, {25800, P, "12-ideas"}, {27000, P, "13-recipe"},
      {28000, R, "14-step"}, {28600, R, nullptr}, {29200, R, nullptr}, {29800, R, nullptr}, {30400, R, nullptr},
      {31000, R, nullptr}, {31600, R, nullptr}, {32200, R, nullptr}, {32800, R, nullptr},
      {34000, P, "15-cooked-it"}, {35600, R, "16-kitchen-updated-toast"},  // -> Ask
      {36800, P, "17-ask"}, {37400, R, nullptr}, {38000, P, "18-ask-list"},  // "What should I use first?"
      {40400, R, "19-ask-answer"},                                            // -> Connect (from answer: back first)
      {40600, LP, nullptr}, {41000, R, nullptr},
      {42600, LP, "20-connect"},
      {43400, LP, nullptr, "milk"},                                           // phone remote: add milk
      {45000, R, "21-add-to-list"}, {45600, P, nullptr},
      {47000, LP, "22-added"},
      {47800, LP, nullptr, nullptr, "what can I cook tonight?"},              // phone remote: a question
      {50600, LP, "23-typed-question"},
      {51400, R, nullptr}, {52600, P, "24-scan"}, {53400, R, nullptr}, {54000, I::Left, "25-scan-modes"},
      {54600, P, nullptr}, {55800, P, "26-viewfinder"}, {57200, P, "27-scan-result"},
      {58600, R, "28-scan-saved"},                                            // -> Connect, then leave it alone
      {105500, I::Left, "29-ambient"}, {106400, I::Left, "30-woke-up"},
      {107000, R, nullptr}, {107150, R, nullptr}, {107300, R, nullptr}, {107450, R, nullptr},
      {107600, R, nullptr}, {107750, R, nullptr}, {107900, R, nullptr},
      {109000, LP, "31-pairing-code"}};  // first run with no account: the code to type in the app
  const uint32_t END = 109500;
  FILE *video = fopen((out + "/frames.rgb").c_str(), "wb");
  std::vector<int16_t> audio;
  std::vector<int16_t> chunk;
  size_t next = 0;
  double audioDue = 0;
  ui::begin();
  for (uint32_t frame = 0; simMs < END; ++frame) {
    simMs = uint32_t(frame * 1000.0 / 30.0 + 0.5);
    if (simMs >= 107000 && simMs < 107040) { lockData(); snprintf(data.pairCode, sizeof data.pairCode, "482913"); unlockData(); }
    while (next < script.size() && script[next].at <= simMs) {
      if (script[next].shot) {  // save the screen as it looks right before this input
        ui::render();
        FILE *s = fopen((out + "/" + script[next].shot + ".ppm").c_str(), "wb");
        fprintf(s, "P6 128 160 255\n");
        for (int i = 0; i < 128 * 160; ++i) {
          uint16_t c = gfx::fb[i];
          unsigned char px[3] = {uint8_t(((c >> 11) & 31) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31)};
          fwrite(px, 1, 3, s);
        }
        fclose(s);
      }
      if (script[next].add) ui::textInput(ui::TextKind::Add, script[next].add);
      else if (script[next].ask) ui::textInput(ui::TextKind::Ask, script[next].ask);
      else ui::input(script[next].in);
      ++next;
    }
    finishPending();
    ui::render();
    for (int i = 0; i < 128 * 160; ++i) {
      uint16_t c = gfx::fb[i];
      unsigned char px[3] = {uint8_t(((c >> 11) & 31) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31)};
      fwrite(px, 1, 3, video);
    }
    audioDue += synth::RATE / 30.0;
    int n = int(audioDue) - int(audio.size());
    chunk.resize(n);
    synth::render(chunk.data(), n);
    audio.insert(audio.end(), chunk.begin(), chunk.end());
  }
  fclose(video);
  FILE *w = fopen((out + "/audio.wav").c_str(), "wb");
  uint32_t bytes = uint32_t(audio.size() * 2), rate = synth::RATE, brate = rate * 2, riff = 36 + bytes, fmtLen = 16;
  uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, w); fwrite(&riff, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w); fwrite(&fmtLen, 4, 1, w);
  fwrite(&pcm, 2, 1, w); fwrite(&ch, 2, 1, w); fwrite(&rate, 4, 1, w); fwrite(&brate, 4, 1, w);
  fwrite(&align, 2, 1, w); fwrite(&bits, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&bytes, 4, 1, w);
  fwrite(audio.data(), 2, audio.size(), w);
  fclose(w);
  printf("frames written for %u ms, %zu audio samples\n", END, audio.size());
}
