# Stream Video ESP32 SDK

Stream Video SDK for ESP32-S3 - Real-time video, audio, and data for embedded projects.

This SDK ports the Stream Video Android SDK's signaling and SFU connection logic to ESP32-S3, while using the same WebRTC and media libraries as LiveKit's ESP32 SDK.

## Features

* **Supported chips**: ESP32-S3 and ESP32-P4
* **Bidirectional audio**: Opus encoding, acoustic echo cancellation (AEC)
* **Video publishing**: H.264 encoding
* **Real-time data**: Data packets, remote method calls (RPC)
* **Stream SFU integration**: Connect to Stream's Selective Forwarding Unit

## Requirements

- ESP-IDF v5.4 or higher
- ESP32-S3 or ESP32-P4 device

## Dependencies

This SDK uses the following components (same as LiveKit ESP32 SDK):

- `esp_peer` (~1.2.7): Espressif WebRTC component
- `esp_capture`: Media capture component
- `av_render`: Media rendering component
- `nanopb`: Protocol Buffer implementation for embedded systems
- `media_lib_os`: Media library OS abstraction

## Installation

Add this component to your ESP-IDF project:

```bash
idf.py add-dependency "GetStream/stream-video-esp32=^0.1.0"
```

Or manually add it to your `idf_component.yml`:

```yaml
dependencies:
  GetStream/stream-video-esp32: "^0.1.0"
```

## Getting Started

### Basic Usage

```c
#include "stream_video.h"

void app_main(void)
{
    // Initialize Stream Video SDK
    stream_video_error_t err = stream_video_init();
    if (err != STREAM_VIDEO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDK");
        return;
    }

    // TODO: Connect to room and publish/subscribe tracks
}
```

## Examples

See the `examples/` directory for complete examples:

- **minimal**: Basic connection with audio
- **minimal_video**: Audio + video publishing
- **voice_agent**: AI agent interaction

## API Reference

See [docs/api_reference.md](docs/api_reference.md) for complete API documentation.

## Project Status

⚠️ **Developer Preview** - This SDK is currently in development and not ready for production use.

## License

Apache-2.0

## Contributing

Contributions are welcome! Please see our contributing guidelines.

## Links

- [Stream Video Documentation](https://getstream.io/video/docs/)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
