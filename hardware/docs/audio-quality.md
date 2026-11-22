# Improve audio for speech recognition

Recommendation: keep the working firmware as the baseline and improve one thing at a time. A pleasant-sounding recording is not necessarily the recording that produces the most accurate transcript.

## Start with the physical signal

Expose the INMP441 bottom acoustic port through the breakout hole. Do not press it against the breadboard or cover it with glue. Start speaking 15–30 cm away, then test 0.5 m and 1 m. Keep microphone and clock wires short, use a common ground, and move the microphone away from the screen, USB bridge, fans and the future speaker. The distances are test conditions, not promised pickup ranges.

Keep the existing working 3.3V supply. Inspect the breakout for its local decoupling capacitor; if absent, follow the datasheet supply-decoupling recommendation with a 100 nF ceramic close to VDD/GND. Do not add parts blindly or change the module to 5V. A better enclosure and microphone position usually help more than aggressive digital gain.

## Understand the current processing

I²S is a digital bus, not a noise filter. It avoids the ESP analog ADC path, but still captures room noise and power or clock faults. DMA buffers samples while the CPU updates the display. Keep capture timing stable and add DMA-overrun instrumentation before combining recording with camera, speaker or large network transfers.

The current DC tracker is a first-order high-pass in effect. At 16 kHz, coefficient 0.003 corresponds to roughly 7.7 Hz. It removes DC, not fan or speech-band noise. ×4 gain is about +12 dB; it cannot improve signal-to-noise ratio. The waveform is visually auto-scaled, whereas the dBFS number reflects digital amplitude after this processing, not sound-pressure level.

## Candidate changes, in order

| Change | Where | Purpose and tradeoff |
|---|---|---|
| Improve placement and supply | Hardware | Prevent noise rather than hiding it |
| Tune fixed gain | ESP | Keep ordinary speech clear with headroom; target peaks around −12 to −6 dBFS as an initial test setting, not an acceptance standard |
| High-pass around 70–100 Hz | ESP or backend | Reduce rumble; compare voices with and without it, because low voice fundamentals can be affected |
| VAD with padding | Backend first | Detect speech for trimming and end-of-turn; VAD does not remove noise inside speech |
| Neural denoising | Mac/backend first | Compare RNNoise or an AFE candidate against untouched audio; avoid speech damage |
| Slow gain control and limiter | Later | Handle changing distance; fast AGC can pump background noise and a limiter cannot repair clipping |
| Acoustic echo cancellation | When speaker returns | Needs the actual playback reference and delay alignment; noise suppression alone is not echo cancellation |

Do not hard-gate low-level audio: that can cut word beginnings and quiet consonants. Do not apply a 50 Hz notch unless a spectral measurement shows mains hum. Avoid a narrow telephone band by default. Keep 16 kHz mono as the baseline; upsampling cannot recreate missing information.

RNNoise's repository example expects 48 kHz 16-bit PCM. For the current 16 kHz recording, use a deliberate resample → denoise → resample chain on the host, or test a different filter suited to 16 kHz. Do not pass this WAV straight into a raw-PCM example. Espressif AFE offers NS, VAD, AGC and AEC; target/version compatibility and memory use must be checked on this original ESP32, not assumed from ESP32-S3 demonstrations.

## Measure with a repeatable set

Record 20–30 short requests with written reference transcripts. Include Egyptian Arabic, English and code-switching if these are your target languages. Include ingredient names, numbers, units, negation, and quiet speech. Use the same phrases in quiet, fan noise, screen active, and at the chosen distances.

Preserve the baseline WAV, then process copies. Compare no filter, high-pass only, and high-pass plus denoising using the same recognizer settings. Track clipped-sample percentage, speech RMS and peak dBFS, silence RMS, missing or repeated segments, transcript latency, word error rate, and correct intent/quantity/unit. WER = (substitutions + deletions + insertions) / reference words. Define Arabic normalization consistently, and also check the exact critical numbers and negations.

For a rough SNR estimate, use separated speech and noise portions from the same setup. RMS ratio is not a calibrated acoustic SNR measurement. Never claim a quality improvement from waveform appearance alone. Reject a filter if it removes a negation or makes numbers less accurate, even if the audio sounds cleaner.

Next experiment: compare matched raw and high-pass-only WAVs before choosing neural denoising. No new filter has been implemented or benchmarked in this documentation update.

Sources: [INMP441 datasheet](https://product.tdk.com/system/files/dam/doc/product/sw_piezo/mic/mems-mic/data_sheet/inmp441.pdf), [Espressif AFE](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/audio_front_end/README.html), [RNNoise](https://github.com/xiph/rnnoise), [Silero VAD](https://github.com/snakers4/silero-vad).
