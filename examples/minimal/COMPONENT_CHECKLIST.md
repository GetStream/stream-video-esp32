# Component Dependencies Checklist

This document lists all components used in the project and their sources.

## ESP-IDF Built-in Components (No Component Manager needed)

These are part of ESP-IDF and don't need to be in `idf_component.yml`:

- `nvs_flash` - Non-volatile storage
- `esp_wifi` - WiFi
- `esp_netif` - Network interface
- `esp_event` - Event loop
- `esp_http_client` - HTTP client
- `mbedtls` - TLS/SSL
- `json` - cJSON library (ESP-IDF v5.x built-in name)
  - Note: In ESP-IDF v6+, this becomes `cjson` from Component Manager

## Component Manager Components (Must be in idf_component.yml)

These need to be declared in `idf_component.yml`:

- `espressif/esp_peer: "^1.2.7"` - WebRTC component
- `espressif/esp_capture: "*"` - Media capture
- `espressif/esp_websocket_client: "^1.6.1"` - WebSocket client
- `livekit/nanopb: "^0.4.9"` - Protobuf implementation

## Components Not Yet Available

These are commented out until available:

- `av_render` - Media rendering (may be part of ESP-IDF or need manual install)
- `media_lib_os` - Media library OS abstraction (may be part of ESP-IDF)

## Component Name Mapping

| Code Reference | ESP-IDF v5.x | ESP-IDF v6+ | Component Manager |
|---------------|--------------|-------------|-------------------|
| `cJSON.h` | `json` | `cjson` | `espressif/cjson` |
| `esp_websocket_client.h` | N/A | N/A | `espressif/esp_websocket_client` |
| `nanopb.h` | N/A | N/A | `livekit/nanopb` or `nikas-belogolov/nanopb` |

## Current Configuration (ESP-IDF v5.4)

For ESP-IDF v5.4, we use:
- `json` (built-in) for cJSON
- Component Manager components for external dependencies

If upgrading to ESP-IDF v6+, update:
- Change `json` to `cjson` in CMakeLists.txt
- Add `espressif/cjson` to idf_component.yml
