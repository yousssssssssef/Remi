// Remi kitchen device: boot intro, then Home / Just bought / Cook / Talk pages on a 1.8" ST7735,
// driven by a KY-040 rotary encoder, with Remi's voice and UI sounds on a MAX98357A speaker amp.
// Board and pins: config.h. Wi-Fi, API address and account: secrets.h (copy secrets.example.h).
// Build: ESP32 Dev Module (or AI Thinker ESP32-CAM), Partition Scheme "Huge APP (3MB No OTA)".
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ESP_I2S.h>
#include <SPI.h>
#include "app_data.h"
#include "backend.h"
#include "config.h"
#include "gfx.h"
#include "synth.h"
#include "ui.h"

AppData data;
SPIClass displaySPI(REMI_SPI_HOST);
Adafruit_ST7735 tft(&displaySPI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
SemaphoreHandle_t dataMutex;
I2SClass i2s;
bool audioOn = false;

// ---------- platform hooks used by the portable UI ----------
uint32_t nowMs() { return millis(); }
bool localTime(struct tm *out) { return getLocalTime(out, 0) && out->tm_year > 120; }
void lockData() { xSemaphoreTake(dataMutex, portMAX_DELAY); }
void unlockData() { xSemaphoreGive(dataMutex); }
void playSfx(Sfx s) {
  if (audioOn) synth::play(s);
}

// ---------- rotary encoder (interrupt-driven transition table, see HardwareExp rotary guide) ----------
static const int8_t QUAD_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t encPrev = 3;
volatile int8_t encAcc = 0;
volatile int32_t encSteps = 0;

void ARDUINO_ISR_ATTR onEncoder() {
  uint8_t cur = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  portENTER_CRITICAL_ISR(&encMux);
  encAcc = encAcc + QUAD_TABLE[(encPrev << 2) | cur];
  encPrev = cur;
  if (cur == 3) {  // detent rest state; accept one missed edge
    if (encAcc >= ENC_STEPS_PER_DETENT - 1) encSteps = encSteps + 1;
    else if (encAcc <= -(ENC_STEPS_PER_DETENT - 1)) encSteps = encSteps - 1;
    encAcc = 0;
  }
  portEXIT_CRITICAL_ISR(&encMux);
}

void pollInputs() {
  if (PIN_ENC_A >= 0) {
    portENTER_CRITICAL(&encMux);
    int32_t steps = encSteps;
    encSteps = 0;
    portEXIT_CRITICAL(&encMux);
    for (; steps > 0; --steps) ui::input(ui::Input::Right);
    for (; steps < 0; ++steps) ui::input(ui::Input::Left);
  }
  if (PIN_ENC_SW >= 0) {  // debounced: short press on release, long press once at the threshold
    static bool stable = HIGH, lastRaw = HIGH, longSent = false;
    static uint32_t changedAt = 0, pressedAt = 0;
    bool raw = digitalRead(PIN_ENC_SW);
    uint32_t now = millis();
    if (raw != lastRaw) { lastRaw = raw; changedAt = now; }
    if (now - changedAt >= 25 && raw != stable) {
      stable = raw;
      if (stable == LOW) { pressedAt = now; longSent = false; }
      else if (!longSent) ui::input(ui::Input::Press);
    }
    if (stable == LOW && !longSent && now - pressedAt >= LONG_PRESS_MS) { longSent = true; ui::input(ui::Input::LongPress); }
  }
  while (Serial.available()) {  // keyboard stand-in for the knob (Serial Monitor, 115200)
    char c = Serial.read();
    if (c == 'a') ui::input(ui::Input::Left);
    if (c == 'd') ui::input(ui::Input::Right);
    if (c == 's') ui::input(ui::Input::Press);
    if (c == 'b') ui::input(ui::Input::LongPress);
    if (c == 'c') ui::toggleColorTest();
    if (c == 't') playSfx(Sfx::TestTone);  // 10 s tone for checking the speaker wiring
  }
}

// ---------- audio ----------
// I2S runs only while something is playing: the amp mutes without clocks, and on the CAM the LRC pin
// (IO4) also drives the flash LED, which then stays dark between sounds.
void audioTask(void *) {
  static int16_t buf[256];
  bool running = false;
  uint32_t quietSince = 0;
  for (;;) {
    if (synth::busy()) {
      if (!running) running = i2s.begin(I2S_MODE_STD, synth::RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
      synth::render(buf, 256);
      if (running) i2s.write((const uint8_t *)buf, sizeof buf);  // blocks at the sample rate
      quietSince = millis();
    } else if (running && millis() - quietSince > 250) {
      i2s.end();
      running = false;
      for (int8_t pin : {PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT}) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    } else {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

void setup() {
#if REMI_BOARD == REMI_ESP32_CAM
  // U0T (GPIO1) carries the speaker's BCLK, so serial TX moves to GPIO33, the CAM's onboard red LED
  // (logs show up as blinks). RX stays on U0R, so the keyboard controls still work.
  Serial.begin(115200, SERIAL_8N1, 3, 33);
#else
  Serial.begin(115200);
#endif
  Serial.println("Remi ready. Serial keys: a = left, d = right, s = press, b = hold, c = colour test, t = test tone");
  dataMutex = xSemaphoreCreateMutex();
  if (!displaySPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, -1)) {  // no MISO: keeps GPIO12 free on the CAM
    Serial.println("ERROR: display SPI could not start");
    while (true) delay(1000);
  }
  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(0);
  tft.fillScreen(0);

  if (PIN_ENC_A >= 0) {
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);  // KY-040 boards often leave the SW pull-up unfitted
    encPrev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), onEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), onEncoder, CHANGE);
  }
  if (PIN_I2S_DOUT >= 0) {
    i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
    audioOn = i2s.begin(I2S_MODE_STD, synth::RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    if (audioOn) {
      i2s.end();  // the audio task starts it again whenever there's something to play
      synth::setVolume(SPEAKER_VOLUME);
      xTaskCreatePinnedToCore(audioTask, "remi-audio", 4096, nullptr, 3, nullptr, 0);
    } else {
      Serial.println("WARNING: I2S speaker could not start; continuing silently");
    }
  }
  backendBegin();
  ui::begin();
}

void loop() {
  static uint32_t nextFrame = 0;
  pollInputs();
  ui::render();
  tft.startWrite();
  tft.setAddrWindow(0, 0, gfx::W, gfx::H);
  tft.writePixels(gfx::fb, gfx::W * gfx::H);
  tft.endWrite();
  uint32_t now = millis();
  if (int32_t(nextFrame - now) > 0) delay(nextFrame - now);  // ~30 fps
  nextFrame = max(now, nextFrame) + 33;
}
