# Stream Video SDK for ESP32

The official ESP32 SDK for [Stream Video](https://getstream.io/video/), enabling real-time video and audio calling on ESP32-S3 and ESP32-P4 devices.

## Features

- **Publish video and audio** — H.264 video and Opus audio to a Stream call over WebRTC.
- **Stream SFU** — Connect to Stream's Selective Forwarding Unit for signaling and media.
- **Built-in capture** — Camera and microphone capture, encoding, and publishing are handled automatically.

## Requirements

- ESP-IDF v5.4+
- ESP32-S3 or ESP32-P4

## Quick start

1. Add the dependency to your project's `idf_component.yml`:
   ```yaml
   dependencies:
     stream-video:
       git: "https://github.com/GetStream/stream-video-esp32.git"
       path: "components/stream-video"
       version: "v0.1.0"
   ```

2. In your main component's `CMakeLists.txt`, add `stream-video` to `REQUIRES`.

3. Configure WiFi and board selection in `idf.py menuconfig`, then build and flash.

## Dependencies

All fetched automatically via the IDF Component Manager:

- `espressif/esp_peer` — WebRTC
- `espressif/esp_capture` — camera/mic capture
- `espressif/esp_video_codec` — H.264 encoder
- `espressif/esp_audio_codec` — Opus encoder
- `espressif/esp_websocket_client` — WebSocket signaling
- `espressif/cjson` — JSON parsing
- `livekit/nanopb` — protobuf serialization

## Documentation

- [Full README and examples](https://github.com/GetStream/stream-video-esp32)
- [Stream Video docs](https://getstream.io/video/docs/)

## License

See [LICENSE](https://github.com/GetStream/stream-video-esp32/blob/main/LICENSE) in the repository.
