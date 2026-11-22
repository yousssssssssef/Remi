# Remi hardware: working microphone baseline

Updated 22 November 2026. The user confirmed that INMP441 recording works on the ESP32-CAM after this firmware was uploaded and flash hashes verified. Audio quality has not yet been measured against a speech-recognition test set.

This directory preserves the standalone microphone experiment. It does not replace the full Remi kitchen-device firmware or implement a voice backend.

- [Run, wire, build and upload the recorder](docs/recorder-setup.md)
- [Improve audio and measure recognition quality](docs/audio-quality.md)
- [Understand the complete voice-request system](docs/voice-architecture.md)
- [Learning path and primary resources](docs/learning-path.md)
- [Experiment history and remaining work](docs/bench-log.md)
- [Firmware](firmware/INMP441Recorder/)

Current behavior: live screen and browser waveforms, dBFS meter, clipping indicator, 16 kHz mono PCM WAV recording up to 10 seconds, early stop, browser playback and download. Wi-Fi remains on. Audio is kept in PSRAM and lost on reset. There is no speech-to-text, noise suppression model, wake word, speaker playback or integration with kitchen actions yet.

The source is the exact working bench version. Preserve it before tuning. Browser waveform height adapts for visibility; it is not a calibrated acoustic measurement. The error counter covers read failures, not every possible DMA overrun. Firmware currently uses a fixed prototype AP password; do not treat it as a production credential design.
