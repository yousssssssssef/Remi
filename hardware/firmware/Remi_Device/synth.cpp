#include "synth.h"
#include <math.h>

namespace synth {
namespace {

enum Wave : uint8_t { SINE, TRI, SQUARE, NOISE };

struct Note {
  uint16_t at, dur;          // ms after the sound starts
  float f0, f1;              // pitch glide (Hz); for NOISE: low-pass cutoff glide
  Wave wave;
  float vol;
  uint8_t attack = 4;        // ms
  float vibrato = 0;         // depth in semitones
};

struct Voice {
  bool on = false;
  uint32_t delay, len, pos;
  float f0, f1, phase, vol, vib, lp;
  uint32_t attack;
  Wave wave;
};

constexpr int VOICES = 12;
Voice voices[VOICES];
volatile uint8_t qHead = 0, qTail = 0;
Sfx queue[16];
float master = 0.6f;
uint32_t noiseState = 0x12345u, babbleSeed = 7;

void add(const Note &n) {
  int slot = 0;
  uint32_t best = 0;
  for (int i = 0; i < VOICES; ++i) {
    if (!voices[i].on) { slot = i; best = UINT32_MAX; break; }
    if (voices[i].pos > best) { best = voices[i].pos; slot = i; }  // steal the oldest
  }
  Voice &v = voices[slot];
  v.on = true;
  v.delay = uint32_t(n.at) * RATE / 1000;
  v.len = uint32_t(n.dur) * RATE / 1000;
  v.pos = 0;
  v.f0 = n.f0; v.f1 = n.f1;
  v.phase = 0; v.vol = n.vol; v.vib = n.vibrato; v.lp = 0;
  v.attack = uint32_t(n.attack) * RATE / 1000 + 1;
  v.wave = n.wave;
}

template <int N>
void addAll(const Note (&notes)[N]) {
  for (const Note &n : notes) add(n);
}

float rnd() {  // 0..1, deterministic
  babbleSeed = babbleSeed * 1664525u + 1013904223u;
  return (babbleSeed >> 8) / 16777216.f;
}

// Remi's voice: chirpy robot syllables, gliding pitch with a soft square + sine body.
void babble(int syllables, float base, uint16_t start) {
  uint16_t t = start;
  for (int i = 0; i < syllables; ++i) {
    uint16_t d = uint16_t(70 + rnd() * 90);
    float p = base * (0.85f + rnd() * 0.4f);
    float q = p * (rnd() < 0.5f ? 1.25f : 0.8f);
    if (i == syllables - 1) q = p * 1.35f;  // ends on an upturn, like a friendly question
    add({t, d, p, q, SQUARE, 0.10f, 6, 0.35f});
    add({t, d, p * 2, q * 2, SINE, 0.16f, 6, 0.35f});
    t += d + uint16_t(18 + rnd() * 40);
  }
}

void spawn(Sfx s) {
  switch (s) {
    case Sfx::Boot: {  // warm rising chime
      const Note n[] = {{0, 420, 1046.5f, 1046.5f, SINE, .22f}, {110, 420, 1318.5f, 1318.5f, SINE, .2f},
                        {220, 460, 1568, 1568, SINE, .2f}, {330, 700, 2093, 2093, SINE, .16f},
                        {0, 160, 523.25f, 523.25f, TRI, .12f}};
      addAll(n);
      break;
    }
    case Sfx::Whoosh: {
      const Note n[] = {{0, 820, 300, 5200, NOISE, .5f, 220}, {80, 700, 220, 980, SINE, .1f, 120}};
      addAll(n);
      break;
    }
    case Sfx::Land: {
      const Note n[] = {{0, 170, 820, 240, SINE, .4f, 2}, {0, 60, 2400, 1800, TRI, .08f, 1}};
      addAll(n);
      break;
    }
    case Sfx::Hello:
      babbleSeed = 42;
      babble(2, 640, 0);
      break;
    case Sfx::Voice:
      babble(9 + int(rnd() * 4), 560 + rnd() * 120, 0);
      break;
    case Sfx::Tick: {
      const Note n[] = {{0, 14, 2600, 2200, SINE, .16f, 1}};
      addAll(n);
      break;
    }
    case Sfx::Page: {
      const Note n[] = {{0, 110, 900, 2600, NOISE, .22f, 20}, {0, 60, 620, 880, TRI, .08f, 4}};
      addAll(n);
      break;
    }
    case Sfx::Select: {
      const Note n[] = {{0, 70, 880, 1320, TRI, .22f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Back: {
      const Note n[] = {{0, 80, 1100, 660, TRI, .2f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Check: {
      const Note n[] = {{0, 60, 1046.5f, 1046.5f, TRI, .2f, 2}, {55, 90, 1568, 1568, TRI, .2f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Uncheck: {
      const Note n[] = {{0, 70, 784, 700, TRI, .17f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Success: {
      const Note n[] = {{0, 140, 784, 784, SINE, .2f}, {80, 140, 1046.5f, 1046.5f, SINE, .2f},
                        {160, 160, 1318.5f, 1318.5f, SINE, .2f}, {240, 420, 1568, 1568, SINE, .2f},
                        {240, 380, 3136, 3136, SINE, .05f}};
      addAll(n);
      babbleSeed = 99;
      babble(2, 760, 380);
      break;
    }
    case Sfx::Error: {
      const Note n[] = {{0, 130, 330, 300, SQUARE, .1f}, {150, 200, 262, 220, SQUARE, .1f}};
      addAll(n);
      break;
    }
    case Sfx::TimerStart: {
      const Note n[] = {{0, 70, 1318.5f, 1318.5f, SINE, .2f, 2}, {80, 120, 1760, 1760, SINE, .2f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Alarm: {
      const Note n[] = {{0, 110, 1760, 1760, SQUARE, .1f, 2}, {0, 110, 3520, 3520, SINE, .12f, 2},
                        {190, 110, 1760, 1760, SQUARE, .1f, 2}, {190, 110, 3520, 3520, SINE, .12f, 2},
                        {380, 200, 2093, 2093, SQUARE, .1f, 2}, {380, 200, 4186, 4186, SINE, .1f, 2}};
      addAll(n);
      break;
    }
    case Sfx::Edge: {
      const Note n[] = {{0, 70, 190, 120, SINE, .3f, 1}};
      addAll(n);
      break;
    }
  }
}

}  // namespace

void setVolume(float v) { master = v; }

void play(Sfx s) {
  uint8_t next = (qHead + 1) & 15;
  if (next == qTail) return;  // queue full: drop
  queue[qHead] = s;
  qHead = next;
}

void render(int16_t *out, int n) {
  while (qTail != qHead) {
    spawn(queue[qTail]);
    qTail = (qTail + 1) & 15;
  }
  for (int i = 0; i < n; ++i) {
    float mix = 0;
    for (Voice &v : voices) {
      if (!v.on) continue;
      if (v.delay) { --v.delay; continue; }
      if (v.pos >= v.len) { v.on = false; continue; }
      float t = float(v.pos) / v.len;
      float f = v.f0 * powf(v.f1 / v.f0, t);
      if (v.vib > 0) f *= 1 + 0.06f * v.vib * sinf(v.pos * 0.0019f);
      float env = v.pos < v.attack ? float(v.pos) / v.attack : 1.f;
      float tail = 1 - t;
      env *= tail * tail * (3 - 2 * tail);  // smooth release
      float s;
      if (v.wave == NOISE) {
        noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
        float white = int32_t(noiseState) / 2147483648.f;
        float a = 1 - expf(-6.2831853f * f / RATE);
        v.lp += a * (white - v.lp);
        s = v.lp * 1.6f;
      } else {
        v.phase += f / RATE;
        if (v.phase >= 1) v.phase -= 1;
        if (v.wave == SINE) s = sinf(6.2831853f * v.phase);
        else if (v.wave == TRI) s = 4 * fabsf(v.phase - 0.5f) - 1;
        else s = v.phase < 0.5f ? 0.8f : -0.8f;
      }
      mix += s * env * v.vol;
      ++v.pos;
    }
    mix *= master;
    mix = mix / (1 + fabsf(mix));  // soft clip, never wraps
    out[i] = int16_t(mix * 32000);
  }
}

}  // namespace synth
