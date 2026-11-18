#pragma once
// Pick the board Remi runs on. DevKit is the full build: screen, rotary encoder and speaker.
// The ESP32-CAM build keeps today's TFT wiring and has no free pins for the encoder or speaker,
// so it is navigated from the Serial Monitor instead (a = left, d = right, s = press, b = hold, c = colour test).
#define REMI_DEVKIT 1
#define REMI_ESP32_CAM 2
#ifndef REMI_BOARD
#define REMI_BOARD REMI_DEVKIT
#endif

#if REMI_BOARD == REMI_DEVKIT
// ESP32-WROOM-32 DevKit (30/38 pin) on a screw-terminal expansion board.
constexpr int8_t PIN_TFT_SCK = 18, PIN_TFT_MOSI = 23, PIN_TFT_CS = 5, PIN_TFT_DC = 4, PIN_TFT_RST = -1;  // RESET -> 3V3
constexpr int8_t PIN_ENC_A = 32, PIN_ENC_B = 33, PIN_ENC_SW = 25;   // KY-040 CLK, DT, SW
constexpr int8_t PIN_I2S_BCLK = 26, PIN_I2S_LRC = 27, PIN_I2S_DOUT = 22;  // MAX98357A BCLK, LRC, DIN
// Reserved for the microphone (INMP441, shares BCLK/LRC with the amp): SD -> GPIO35. Not used yet.
constexpr float SPEAKER_VOLUME = 0.6f;       // 0..1; MAX98357A GAIN pin floating = 9 dB
#define REMI_SPI_HOST VSPI
#elif REMI_BOARD == REMI_ESP32_CAM
constexpr int8_t PIN_TFT_SCK = 14, PIN_TFT_MOSI = 13, PIN_TFT_CS = 15, PIN_TFT_DC = 2, PIN_TFT_RST = -1;
constexpr int8_t PIN_ENC_A = -1, PIN_ENC_B = -1, PIN_ENC_SW = -1;
// Speaker on the CAM (MAX98357A): every other header pin is boot, PSRAM or upload, so audio takes
// U0T (serial TX: logs stop, keyboard input on U0R still works), IO4 (also drives the flash LED, so it
// flickers while sound plays; tape over it) and IO12 (must be low at boot; the amp input doesn't pull it up).
constexpr int8_t PIN_I2S_BCLK = 1, PIN_I2S_LRC = 4, PIN_I2S_DOUT = 12;
constexpr float SPEAKER_VOLUME = 0.4f;       // USB power through the MB: keep peaks low to avoid brownouts
#define REMI_SPI_HOST HSPI
#else
#error "Unknown REMI_BOARD"
#endif

constexpr uint32_t TFT_SPI_HZ = 16000000;   // lower to 8 MHz if long jumpers give coloured noise
constexpr int ENC_STEPS_PER_DETENT = 4;      // KY-040: 4 quarter-steps per click (see HardwareExp rotary guide)
constexpr uint32_t LONG_PRESS_MS = 600;
constexpr uint32_t REFRESH_EVERY_MS = 60000; // Home/list/ideas background sync
