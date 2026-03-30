# Stream Video ESP32 SDK

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE.txt)

Real-time video, audio, and signaling for ESP32 with Stream's SFU. Supports **ESP32-S3** (example provided) and **ESP32-P4**.

## Features

- **Publish video and audio** — Send H.264 video and Opus audio (with AEC) to a Stream call. The SDK currently supports *publishing* only; subscribing to remote participants' audio/video is not yet supported.
- **Stream SFU** — Connect to Stream's Selective Forwarding Unit over WebRTC for signaling and media.
- **Built-in capture** — Camera and microphone capture, encoding, and publishing are handled automatically by the SDK.

## Requirements

- ESP-IDF v5.4+
- ESP32-S3 or ESP32-P4

## Dependencies

Fetched automatically via Component Manager:

- `espressif/esp_peer` — WebRTC
- `espressif/esp_capture` — camera/mic
- `espressif/esp_video_codec` — H.264 encoder
- `espressif/esp_audio_codec` — Opus encoder
- `espressif/esp_websocket_client` — signaling
- `espressif/cjson` — JSON
- `livekit/nanopb` — protobuf

## Quick start

1. **Install ESP-IDF** v5.4+ and set target (e.g. `./install.sh esp32s3`). See [examples/minimal/BUILD_INSTRUCTIONS.md](examples/minimal/BUILD_INSTRUCTIONS.md) for full setup (macOS, Linux, Windows).

2. **Add the SDK** to your project — in your project root `idf_component.yml`:
   ```yaml
   dependencies:
     GetStream/stream-video: "^0.1.0"
   ```
   Or: `idf.py add-dependency "GetStream/stream-video=^0.1.0"`  
   Then in your main component's `CMakeLists.txt` add `stream-video` to `REQUIRES`.  
   **Full steps for a new app:** [docs/using_sdk_in_your_app.md](docs/using_sdk_in_your_app.md).

3. **Run the minimal example**:
   ```bash
   cd examples/minimal
   idf.py set-target esp32s3
   idf.py menuconfig   # WiFi + Stream Video Example
   idf.py build flash monitor
   ```
   Configure Stream (env, user, call) in `main/main.c`; WiFi and video settings in menuconfig or `sdkconfig.defaults`. For a **menuconfig snapshot and options reference**, see [examples/minimal/BUILD_INSTRUCTIONS.md#menuconfig-reference-stream-video-example](examples/minimal/BUILD_INSTRUCTIONS.md#menuconfig-reference-stream-video-example). Details: [examples/minimal/README.md](examples/minimal/README.md).

## Docs

- [API reference](docs/api_reference.md)
- [Build & flash](examples/minimal/BUILD_INSTRUCTIONS.md)
- [Stream Video docs](https://getstream.io/video/docs/)

## Security

WiFi and tokens: use menuconfig or `sdkconfig.defaults`; do not commit `sdkconfig` or secrets.

## License

[Apache-2.0](LICENSE.txt). [Contributing](CONTRIBUTING.md).
