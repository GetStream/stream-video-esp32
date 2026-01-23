# Getting Started with Stream Video ESP32 SDK

This guide will help you get started with the Stream Video ESP32 SDK.

## Prerequisites

1. **ESP-IDF v5.4 or higher** - Install from [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

2. **ESP32-S3 or ESP32-P4 device** - This SDK is designed for these chips

3. **Stream account** - Sign up at [getstream.io](https://getstream.io)

## Installation

### Option 1: Using Component Manager (Recommended)

Add the SDK to your project:

```bash
idf.py add-dependency "GetStream/stream-video-esp32=^0.1.0"
```

### Option 2: Manual Installation

1. Clone this repository
2. Copy the `components/stream-video` directory to your project's `components/` directory
3. Add dependencies to your `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_peer: "^1.2.7"
  espressif/esp_capture: "*"
  espressif/av_render: "*"
  nanopb/nanopb: "*"
  espressif/media_lib_os: "*"
```

## Basic Example

See `examples/minimal/` for a complete working example.

## Next Steps

- Read the [API Reference](api_reference.md)
- Check out the examples in the `examples/` directory
- Review the [Stream Video Documentation](https://getstream.io/video/docs/)


