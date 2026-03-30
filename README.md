# Stream Video ESP32 SDK

This is the official ESP32 SDK for Stream Video, a platform for building apps with video and audio calling support. The repository includes an SDK and examples on how to use Stream Video on ESP32 devices. The supported devices are **ESP32-S3** (example provided) and **ESP32-P4**.

## What is Stream?

Stream allows developers to rapidly deploy scalable feeds, chat messaging and video with an industry leading 99.999% uptime SLA guarantee.

With Stream's video components, you can use our SDK to build in-app video calling, audio rooms, audio calls, or live streaming. The best place to get started is with our [docs](https://getstream.io/video/docs/).

All calls run on Stream's network of edge servers around the world, ensuring optimal latency and reliability.

## Features

- **Publish video and audio** — Send H.264 video and Opus audio to a Stream call. The SDK currently supports *publishing* only; subscribing to remote participants' audio/video is not yet supported.
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
     stream-video:
       git: "https://github.com/GetStream/stream-video-esp32.git"
       path: "components/stream-video"
       version: "v0.1.0"
   ```
   Then in your main component's `CMakeLists.txt` add `stream-video` to `REQUIRES`.  
   **Full steps for a new app:** [docs/using_sdk_in_your_app.md](docs/using_sdk_in_your_app.md).

3. **Run the minimal example**:
   ```bash
   cd examples/minimal
   idf.py set-target esp32s3
   idf.py menuconfig
   idf.py build flash monitor
   ```

   **Before building, configure two things in `idf.py menuconfig`:**
   - **WiFi** — Go to **Stream Video Example** and set your WiFi SSID and password.
   - **Board selection** — Choose the correct board for your hardware (this sets the right camera pin map and audio codec). See the [supported boards list](examples/minimal/README.md#audio-capture) in the example README.

   Configure Stream credentials (environment, user, call) in `main/main.c`. For a **menuconfig snapshot and options reference**, see [BUILD_INSTRUCTIONS.md](examples/minimal/BUILD_INSTRUCTIONS.md#menuconfig-reference). Details: [examples/minimal/README.md](examples/minimal/README.md).

   The minimal example automatically joins this [call](https://pronto.getstream.io/join/79cYh3J5JgGk). You can open it from a web browser to see the camera feed of the ESP32 device.

   **Important**:
   ESP32 devices have weak WiFi receivers. For smooth testing, we recommend being close to the router.
   
## Docs

- [API reference](docs/api_reference.md)
- [Build & flash](examples/minimal/BUILD_INSTRUCTIONS.md)
- [Stream Video docs](https://getstream.io/video/docs/)

## Security

WiFi and tokens: use menuconfig or `sdkconfig.defaults`; do not commit `sdkconfig` or secrets.

## 👩‍💻 Free for Makers 👨‍💻

Stream is free for most side and hobby projects. To qualify, your project/company needs to have < 5 team members and < $10k in monthly revenue. Makers get $100 in monthly credit for video for free.

## We are hiring

We've closed a [\$38 million Series B funding round](https://techcrunch.com/2021/03/04/stream-raises-38m-as-its-chat-and-activity-feed-apis-power-communications-for-1b-users/) in 2021 and we keep actively growing.
Our APIs are used by more than a billion end-users, and you'll have a chance to make a huge impact on the product within a team of the strongest engineers all over the world.
Check out our current openings and apply via [Stream's website](https://getstream.io/team/#jobs).