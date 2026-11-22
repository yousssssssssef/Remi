# How Remi will understand spoken requests

The ESP records sound. Speech recognition turns sound into text. A separate intent layer decides what the text means. Remi's application validates and executes an allowed operation. These are distinct steps; a transcript is not permission to run an arbitrary action.

## Recommended first architecture

Use this ESP32-CAM as a microphone and display, with a Mac or backend doing speech recognition. Start with a Record button, not always-listening or wake-word behavior. Keep the working recorder separate until a complete voice turn succeeds.

```mermaid
flowchart LR
 U[User presses Record] --> M[INMP441]
 M --> E[ESP32: I2S DMA, PCM, WAV]
 E --> W[Wi-Fi: phone or Mac bridge]
 W --> B[Backend: validate audio and detect speech]
 B --> S[Local ASR or cloud transcription]
 S --> T[Transcript]
 T --> I[Intent and parameter extraction]
 I --> V[Validate identity, household and allowed action]
 V --> C[Confirm changes when needed]
 C --> A[Existing Remi application operation]
 A --> R[Reply text to screen]
```

Today only the microphone → ESP → browser WAV portion is implemented. The bridge, transcription service, intent handling and voice actions are proposed. The checked-out fork's backend does not currently contain `/voice/turn` or `/device/ask`; old full-device notes describe those names in another local project version. Inspect the target backend schema and handlers before adapting those names.

## Where each part can run

| Option | Runs on | Internet | Fit for Remi |
|---|---|---|---|
| Current ESP alone | PCM capture, meter, buffering; small DSP is possible | No | Working front end; do not plan unrestricted conversational transcription on this board |
| Limited embedded commands | Supported ESP-SR model/target | No | Possible future experiment; vocabulary, language, model and board limits apply |
| Local transcription | Mac, PC or another adequately sized host | Not after model download | Recommended learning prototype; audio can stay on your network |
| Cloud transcription | Your backend calls an ASR provider | Yes | Alternative for broader model access; measure dialect accuracy, latency and cost |
| Hybrid | ESP captures; local server transcribes; optional cloud reasoning | Depends on selected stages | Choose separately for transcription and reasoning |

Offline recognition on the Mac is not the same as offline recognition on the ESP. whisper.cpp supports local Whisper inference; choose a multilingual model for Arabic rather than an English-only `.en` variant. Benchmark on your actual host before deciding model size. Do not assume ESP-SR is S3-only: original ESP32 documentation exists, but supported features depend on release and model. MultiNet's documented command languages are Chinese/English, so it is not a substitute for open-ended Egyptian Arabic transcription.

## Network connection choices

The recorder currently creates `Remi-Mic-Test`. That access point is not itself an Internet connection and does not route audio to a cloud service.

For the first offline experiment, connect the Mac to that AP and download a WAV; run local transcription on the downloaded file. Download models beforehand. The Mac may temporarily lose its usual Wi-Fi Internet connection.

For an integrated device, use station mode: ESP and the backend join the same home Wi-Fi. A backend on the Mac must listen on its LAN interface and accept the selected port through the firewall. On the ESP, `localhost` means the ESP, not your Mac. Configure a LAN hostname/IP and handle reconnects and timeouts. AP+station is possible for provisioning, but not needed for the first end-to-end prototype.

If cloud ASR is chosen, the ESP sends audio to your backend; the backend holds the provider API key and uses HTTPS to the provider. Do not place a permanent provider key in firmware, browser JavaScript or this public repository. Production device authentication and provisioning must replace the shared test AP password.

## One spoken request, step by step

Example: “Add two cartons of milk to my shopping list.”

1. User presses Record. Show LISTENING immediately.
2. Capture 16 kHz signed mono PCM, up to the configured ten-second limit. Longer requests need an explicit buffer/streaming design.
3. Stop on the button. Later add VAD with about 200–300 ms pre-roll and 600–1000 ms trailing silence as starting parameters to tune. Keep a hard duration limit and a cancel action.
4. Send a WAV plus a unique request ID. Validate duration, size, WAV headers and sample rate on the backend. Authenticate the device and derive its household server-side.
5. Reject silence/no speech rather than asking a model to invent a request. Test VAD on target languages and background noise.
6. Transcribe locally or in the cloud. Provide relevant ingredient vocabulary or language hints only where the selected model supports them. Test Egyptian Arabic and mixed-language speech explicitly.
7. Extract a structured intent such as `add_to_shopping_list`, item `milk`, quantity `2`, unit `carton`. These example names are a proposed interface, not existing backend operations.
8. Match milk to the actual catalogue and validate quantity and unit. If ambiguous, ask “Which milk?” instead of guessing.
9. Show a confirmation for a mutation during the prototype. Bind that confirmation to the exact proposed action and expire it. Read-only questions can return directly after authorization.
10. Execute an existing authorized application command. Store an idempotency result under the request ID so reconnect/retry cannot add the same item twice.
11. Return the transcript, reply text, action result and request ID. Show DONE or a specific recoverable error. Speaker/TTS is a separate later integration.

Proposed API: `POST /voice/turn` accepts bounded multipart WAV plus request ID and device authentication, returns transcript, status (`needs_confirmation`, `completed`, `no_speech`, or `error`), reply and a validated proposed action. A separate confirmation request performs mutations. Start with one request at a time. Set upload and ASR timeouts; on disconnect, retain a short recording for an explicit retry rather than pretending it succeeded.

## Privacy and speaker interaction

Live waveform sampling currently continues in RAM, but only a user-triggered recording is retained. Clearly distinguish LIVE INPUT from a saved or transmitted recording. Add a visible listening/upload indicator, cancel action and documented retention policy. Avoid storing raw household speech or transcripts in public commits or routine logs. Keep provider keys server-side.

Start half-duplex when the speaker returns: stop playback before listening, then stop recording before speaking. Full duplex needs a separate I²S/pin/resource plan and echo cancellation using playback PCM as a reference. The old amplifier pin assignments overlap this microphone experiment; do not reconnect it without revising the plan. Camera, encoder and speaker coexistence have not been validated in this firmware.

## Concrete implementation sequence

1. Preserve this working capture baseline and collect a short labelled voice test set.
2. Manually transcribe a downloaded WAV on the Mac using whisper.cpp. Compare a multilingual model and a selected cloud model if desired.
3. Create a local bridge and backend voice endpoint with duration/size checks, no-speech behavior and transcript-only results.
4. Add one read-only question using existing kitchen data.
5. Add one confirmed shopping-list action with authorization and retry deduplication.
6. Integrate this turn into the main device UI, then test DSP and VAD improvements against the saved baseline.
7. Consider wake word, speaker replies and a board migration only after measured resource and pin requirements justify them.

Sources: [whisper.cpp](https://github.com/ggml-org/whisper.cpp), [OpenAI transcription guide](https://developers.openai.com/api/docs/guides/speech-to-text), [ESP-SR command recognition](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/speech_command_recognition/README.html), [original ESP32 benchmarks](https://docs.espressif.com/projects/esp-sr/en/latest/esp32/benchmark/README.html), [Silero VAD](https://github.com/snakers4/silero-vad). Research checked 22 November 2026; verify provider model names and target support again at implementation time.
