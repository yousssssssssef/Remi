# Bench record: 22 November 2026

## Confirmed

- ESP32-D0WD-V3 ESP32-CAM with PSRAM, existing ST7735 display, MB supply, amplifier removed.
- INMP441 wiring: clock IO12, WS GPIO1/U0T, SD GPIO3/U0R, left selected with L/R grounded, 3.3V supply.
- Firmware compiled with Arduino ESP32 core 3.3.13. Binary uses 1,020,669 program bytes (32%) and 50,000 static RAM bytes (15%); PSRAM allocation is additional.
- Uploaded to the bench board and flash hash verified. User then reported: “Great, the microphone is working now.” This is functional confirmation, not a measured quality or ASR benchmark.
- Separate I²S capture task, live TFT/browser waveform, level display, ten-second PCM recording, early stop and WAV playback implemented.

## Earlier failed path

The analog KY-038/KY-037-style module produced static. ADC samples were zero 93.8% of the time with screen updates, versus 99.9% with the screen frozen. The display affected the interference, but this did not prove that the microphone itself was defective. The switch to digital INMP441 produced working audio. The analog firmware is not the current baseline.

The AI-generated realistic wiring illustration was rejected as incorrect and must not be used. The deterministic breadboard hole map supersedes it. Older full-device notes that say the microphone requires a DevKit, that all wake-word support requires an S3, or that GPIO35 is the current mic input do not describe this working bench test.

## Not yet verified or implemented

No controlled noise/ASR evaluation, no dedicated noise suppression, no DMA-overrun instrumentation, no speech endpoint, no wake word, no speaker/camera/encoder coexistence test. The recorder's fixed DC removal and gain are implemented; proposed filters remain proposals. Preserve this firmware before experiments. Audio files and personal transcripts are not included in the repository.
