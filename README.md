# Stream Video ESP32 SDK

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE.txt)

Stream Video SDK for ESP32-S3 - Real-time video, audio, and data for embedded projects.

This SDK provides Stream Video signaling and SFU connectivity on ESP32-S3 with integrated WebRTC and media components for embedded use.

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

This SDK uses the following components :

- `esp_peer` (~1.2.7): Espressif WebRTC component
- `esp_capture`: Media capture component
- `av_render`: Media rendering component
- `nanopb`: Protocol Buffer implementation for embedded systems
- `media_lib_os`: Media library OS abstraction

## Installation

## ESP-IDF Setup (macOS)

Step-by-step install for macOS (Apple Silicon or Intel):

1. Install prerequisites:
   ```bash
   brew install cmake ninja dfu-util ccache git
   ```
2. Clone ESP-IDF:
   ```bash
   mkdir -p ~/esp
   cd ~/esp
   git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git
   ```
3. Install ESP-IDF tools:
   ```bash
   cd ~/esp/esp-idf
   ./install.sh esp32s3
   ```
   
4. Export the environment (each new terminal):
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```
5. Optional: auto-export for zsh by adding this line to `~/.zshrc`:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

## Getting Started

### Basic Usage

Add this component to your ESP-IDF project:

```bash
idf.py add-dependency "GetStream/stream-video-esp32=^0.1.0"
```

Or manually add it to your `idf_component.yml`:

```yaml
dependencies:
  GetStream/stream-video-esp32: "^0.1.0"
```

## Examples

See the `examples/` directory for complete examples:

- **minimal**: Video publishing example

### Run the minimal example

1. Open the example:
   ```bash
   cd examples/minimal
   ```
2. Set the target:
   ```bash
   idf.py set-target esp32s3
   ```
3. Configure WiFi and video quality in menuconfig:
   ```bash
   idf.py menuconfig
   ```
   Navigate to `Stream Video Example` and update WiFi SSID/password and the video quality settings (width/height/fps/bitrate).
4. Build, flash, and monitor:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

### Where to change example values

- `examples/minimal/main/main.c`: Stream environment, user, and call settings.
- `examples/minimal/main/Kconfig`: Menuconfig options for WiFi, STUN, and video quality.
- `examples/minimal/sdkconfig.defaults`: Default project values used when you first configure the project.

### Default values and menuconfig overrides

The minimal example ships with defaults in `examples/minimal/sdkconfig.defaults`. These are the starting values before you run `menuconfig`:

- WiFi SSID: `"MyWiFi"` (placeholder — set your SSID in `sdkconfig.defaults` or via menuconfig)
- WiFi password: `""` (empty)
- Video width/height: `320x240` (lower resolution for memory; can increase in menuconfig)
- Video FPS: `20`
- Video bitrate: `900000`
- Camera format: `YUV422`

To change WiFi credentials or video quality after the first configure, run `idf.py menuconfig` and edit the settings under `Stream Video Example` which is under `Component config`. Changes are saved to `sdkconfig` in the example directory and override the defaults.

```c
#include "esp_log.h"
#include "stream_video.h"

static const char *TAG = "app";

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



## API Reference

See [docs/api_reference.md](docs/api_reference.md) for complete API documentation.

## Project Status

⚠️ **Developer Preview** - This SDK is currently in development and not ready for production use.

**Known limitations:** WiFi uses exponential backoff in the example; coordinator and SFU have configurable reconnect. The API reference in [docs/api_reference.md](docs/api_reference.md) is incomplete.

## Security / credentials

WiFi credentials and Stream tokens are not stored in the repository. Set WiFi via `idf.py menuconfig` or by editing `sdkconfig.defaults`; `sdkconfig` is gitignored. Tokens are obtained at runtime from your backend. Do not commit `sdkconfig` or any file containing secrets.

## License

[Apache-2.0](LICENSE.txt)

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md).

## Links

- [Stream Video Documentation](https://getstream.io/video/docs/)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
