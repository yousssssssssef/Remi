#pragma once
// Tiny procedural synth for Remi's voice and UI sounds: 22.05 kHz mono, 16-bit.
// play() may be called from the UI core while render() runs on the audio core (single-producer queue).
#include <stdint.h>
#include "app_data.h"

namespace synth {

constexpr int RATE = 22050;
void play(Sfx s);
void render(int16_t *out, int n);  // mixes all active sounds into out (overwrites)
void setVolume(float v);           // 0..1, default 0.6
bool busy();                       // anything playing or queued (lets the device stop I2S clocks when silent)

}  // namespace synth
