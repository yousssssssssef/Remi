# Learn the voice pipeline

Follow this sequence using your working recorder. The exercises are proposed; they have not been run for you.

| Step | Learn | Exercise | Primary resource |
|---|---|---|---|
| 1 | Microphone acoustic port and I²S clocks | Identify SCK, WS, SD, left-slot selection and the bottom port on your board | [INMP441 datasheet](https://product.tdk.com/system/files/dam/doc/product/sw_piezo/mic/mems-mic/data_sheet/inmp441.pdf) |
| 2 | Samples, signed PCM and WAV | Download a ten-second WAV. Check 16 kHz, one channel, 16 bits and 320,000 audio bytes | [Python wave module](https://docs.python.org/3/library/wave.html) |
| 3 | DMA and pin ownership | Trace capture task → PCM buffer → waveform snapshot → WAV download in `recorder.cpp` | [ESP-IDF I²S driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html) |
| 4 | Level, noise and clipping | Record quiet, close speech and distant speech. Compare numbers without trusting auto-scaled wave height | [Audio-quality experiment guide](audio-quality.md) |
| 5 | Local speech recognition | Build whisper.cpp per its current README, download a multilingual model, transcribe your own WAV | [whisper.cpp quick start](https://github.com/ggml-org/whisper.cpp#quick-start) |
| 6 | Voice activity detection | Mark speech versus silence, inspect whether word beginnings/endings survive trimming | [Silero VAD examples](https://github.com/snakers4/silero-vad) |
| 7 | Denoising versus recognition | Process a copy, then compare its transcript with the unfiltered reference | [RNNoise code and example format](https://github.com/xiph/rnnoise) |
| 8 | Embedded audio front ends | Identify what AEC, NS, AGC and VAD each do; check board/version support before porting | [Espressif AFE](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/audio_front_end/README.html) |
| 9 | Cloud ASR as an alternative | From the backend, submit one non-sensitive test file and inspect the returned transcript | [OpenAI file transcription](https://developers.openai.com/api/docs/guides/speech-to-text) |
| 10 | Intent, validation and action | Turn one transcript into a proposed shopping action, then confirm before executing | [Remi architecture plan](voice-architecture.md) |

Glossary: ASR is speech-to-text. DSP changes the audio signal. VAD finds speech regions. NS suppresses noise. AGC adjusts gain. AEC removes the device's own speaker echo using a reference. A wake word starts an interaction; it does not understand the full request. An LLM can interpret text, but the application still controls which actions are allowed.

Start with file transcription. Streaming adds partial transcripts, transport buffering, end-of-turn decisions and cancellation. Learn those after one complete file-based turn works.
