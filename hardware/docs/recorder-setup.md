# Run the INMP441 recorder

## Working configuration

ESP32-CAM with PSRAM, ESP32-CAM-MB USB board, INMP441 six-pin breakout, and the existing ST7735 128×160 screen. Remove the analog microphone and amplifier. Leave camera capture disabled and both SD slots empty. Match printed pin labels, not board orientation in generic pictures.

| INMP441 | ESP32-CAM | Direction |
|---|---|---|
| VDD | 3.3V | Power to microphone |
| GND | GND | Common ground |
| L/R | GND | Select left I²S slot |
| SCK | IO12 | Clock from ESP |
| WS | U0T / GPIO1 | Word clock from ESP |
| SD | U0R / GPIO3 | Audio into ESP |

| ST7735 | ESP32-CAM |
|---|---|
| SCK | IO14 |
| SDA / MOSI | IO13 |
| CS | IO15 |
| A0 / DC | IO2 |
| RESET | 3.3V |
| VCC | 3.3V, retain this bench setup |
| LED | 3.3V, retain existing matching module wiring |
| GND | GND |

Screen power is specific to this previously identified module, not a claim that every ST7735 breakout has the same backlight circuit. Earlier full-device notes mention 5V screen VCC; do not copy that into this bench configuration.

IO16 stays reserved for PSRAM. IO12 is a boot strapping pin; here it drives the microphone's clock input, rather than receiving the analog board's biased output. Do not add a pull-up to IO12. UART logs are disabled because GPIO1 and GPIO3 now belong to I²S.

## Breadboard junctions

The ESP sits beside the MB, not in the MB socket. In run mode, connect only MB 5V socket → ESP 5V and MB GND socket → ESP GND. The USB bridge must not drive ESP U0R while the microphone drives it.

[Exact hole map](breadboard-map.svg). A–E on each numbered row are connected; F–J is a separate group. Rails are unused.

| Row | A | B | C | D | E |
|---|---|---|---|---|---|
| 3 | MB 5V | ESP 5V | — | — | — |
| 5 | MB GND | ESP GND | TFT GND | MIC GND | MIC L/R |
| 7 | ESP 3V3 | TFT VCC | TFT RESET | TFT LED | MIC VDD |
| 13 | ESP IO14 | TFT SCK | — | — | — |
| 16 | ESP IO13 | TFT SDA | — | — | — |
| 19 | ESP IO15 | TFT CS | — | — | — |
| 22 | ESP IO2 | TFT A0/DC | — | — | — |
| 25 | ESP IO12 | MIC SCK | — | — | — |
| 27 | ESP U0T | MIC WS | — | — | — |
| 29 | ESP U0R | MIC SD | — | — | — |

## Build

Verified toolchain: Arduino ESP32 core 3.3.13, Adafruit GFX 1.13.0, Adafruit ST7735/ST7789 1.12.0, Adafruit BusIO 1.17.4. ESP_I2S, WiFi, SPI and WebServer come with the core. Select ESP32-CAM with PSRAM.

```sh
arduino-cli compile --fqbn 'esp32:esp32:esp32cam:UploadSpeed=115200,FlashFreq=40,FlashMode=dio' hardware/firmware/INMP441Recorder
```

On the author's Mac, arduino-cli is bundled inside Arduino IDE at `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`.

## Upload and reconnect

1. Unplug USB.
2. Disconnect microphone WS and SD from the ESP.
3. Connect MB U0T socket → ESP U0T and MB U0R socket → ESP U0R. These names describe matching CAM socket labels, not a separate USB adapter's TX/RX labels.
4. Connect ESP IO0 to GND.
5. Reconnect USB and press reset.
6. Upload the sketch. Confirm flash verification completes.
7. Unplug USB again.
8. Remove both MB serial jumpers and the IO0–GND jumper.
9. Reconnect microphone WS → U0T and SD → U0R.
10. Keep MB power jumpers in place and reconnect USB.

## Record

Join `Remi-Mic-Test`, password `remimic123`. Open `http://192.168.4.1`. The screen should show LIVE INPUT. Speak toward the small acoustic hole on the microphone breakout. Press Record, then Stop & save or wait ten seconds. Play or download the WAV. A new recording replaces the previous one.

PSRAM ERROR means allocation failed. I2S INIT ERROR means the interface failed to start. MIC ERROR means repeated read failures. Check wiring and restart; zero samples without a driver error can still indicate a disconnected microphone.

Audio transport: 24-bit I²S in 32-bit receive slots, left channel, 16 kHz. Firmware converts to signed 16-bit PCM, removes a slowly tracked DC component and applies fixed ×4 digital gain. That gain amplifies noise too. The WAV contains a standard 44-byte header; ten seconds contains 320,000 PCM bytes, plus header.

This is a one-user diagnostic server, not a deployed voice API. Do not expose it on the Internet.
