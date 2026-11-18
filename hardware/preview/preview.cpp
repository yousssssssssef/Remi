// Host preview: runs the real device UI + synth with sample data and a scripted knob session.
// Writes raw frames (RGB24, 128x160 @ 30 fps), a WAV, and PPM stills for key screens.
//   c++ -std=c++17 -O2 -I../firmware/Remi_Device preview.cpp ../firmware/Remi_Device/{gfx,ui,synth}.cpp -o preview
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
void lockData() {}
void unlockData() {}
void playSfx(Sfx s) { synth::play(s); }

#include "sample_backend.h"

struct Ev { uint32_t at; ui::Input in; const char *shot; };

int main(int argc, char **argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  sampleData();
  synth::setVolume(1.0f);  // louder than the device default so the preview is easy to hear
  using I = ui::Input;
  const I R = I::Right, L = I::Left, P = I::Press, LP = I::LongPress;
  // Script (ms after boot). The intro runs ~9.2 s on its own.
  std::vector<Ev> script = {
      {9900, P, nullptr},  {11200, P, "01-home"},
      {11400, R, nullptr}, {12600, P, "02-just-bought"}, {13300, R, nullptr}, {13700, P, nullptr},
      {14100, R, nullptr}, {14400, P, nullptr}, {14800, R, nullptr}, {15000, R, nullptr}, {15300, P, nullptr},
      {15700, R, nullptr}, {16000, R, nullptr}, {16200, R, "03-shopping-list"}, {16700, P, nullptr},
      {20500, R, "04-put-away-done"}, {21700, P, "05-cook"}, {22600, R, nullptr}, {23000, L, "06-ideas"}, {23400, P, nullptr},
      {24600, P, "07-recipe"}, {25800, P, "08-step"}, {26700, P, "09-step-timer"}, {27600, R, nullptr},
      {28200, R, nullptr}, {28800, R, nullptr}, {29400, R, nullptr}, {30000, R, nullptr}, {30700, R, "10-long-step"},
      {31200, R, nullptr}, {31700, R, nullptr}, {32300, R, nullptr}, {33600, P, "11-finished"}, {34300, R, nullptr},
      {35300, P, "12-talk"}, {36400, P, "13-talk-speaking"}, {39000, L, nullptr}, {39400, L, nullptr},
      {39800, L, nullptr}, {41200, LP, "14-home-again"}};
  const uint32_t END = 42000;
  FILE *video = fopen((out + "/frames.rgb").c_str(), "wb");
  std::vector<int16_t> audio;
  std::vector<int16_t> chunk;
  size_t next = 0;
  double audioDue = 0;
  ui::begin();
  for (uint32_t frame = 0; simMs < END; ++frame) {
    simMs = uint32_t(frame * 1000.0 / 30.0 + 0.5);
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
      ui::input(script[next].in);
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
