# Stream Video ESP32-S3 - Complete Example

This example demonstrates the complete flow:
1. **App** fetches auth data (token) from a token service (e.g. Stream's pronto or your backend)
2. App passes auth data to the SDK; **SDK** connects to coordinator, joins the call, and connects to the SFU
3. SDK publishes video and audio over WebRTC

## Configuration

**WiFi:** Set SSID and password in `sdkconfig.defaults` (e.g. `CONFIG_STREAM_VIDEO_WIFI_SSID="MyWiFi"`) or run `idf.py menuconfig` and go to **Stream Video Example**. The example reads these from Kconfig; do not hardcode credentials in source.

**Stream (token service, user, call):** Edit `main/main.c` and set:

- `STREAM_AUTH_BASE_URL` — base URL of your token service (e.g. `"https://pronto.getstream.io/"`)
- `STREAM_ENVIRONMENT` — environment name passed to token service (e.g. `"pronto"`)
- `STREAM_USER_ID` — user ID (or `NULL` for auto-generated)
- `STREAM_CALL_TYPE` — e.g. `"default"`, `"livestream"`
- `STREAM_CALL_ID` — call ID to join, or `NULL` to create a new call

### Audio Capture
Audio publishing uses the microphone when a supported codec/mic board is present.
It uses Espressif's `codec_board` component to initialize the audio codec and provide a record handle.
Use `idf.py menuconfig` and configure under **Stream Video SDK**:
- Enable audio capture/publish, audio sample rate / channel count / bitrate / I2S mode

The SDK should work on any ESP32-S3 board with a camera and PSRAM. The following boards have been tested:
- **ESP32-S3 WROOM** (generic S3 wiring — default)
- **XIAO ESP32-S3 Sense** (OV2640/OV3660, PDM mic)

Select your board in `idf.py menuconfig` under **Stream Video SDK → Camera Board**. The board selection sets the correct camera pin map and audio codec type automatically. For `XIAO_ESP32S3_Sense`, audio input uses the PDM mic path.

**Using a different board?** The SDK relies on Espressif's `codec_board` component for audio codec initialization, which supports many additional boards. If your board is not listed in menuconfig:
1. Set `CONFIG_STREAM_CODEC_BOARD_TYPE` in menuconfig or `sdkconfig.defaults` to a board name supported by `codec_board`.
2. If needed, vendor the component into `examples/minimal/components/codec_board` and add your board definition. See the [codec_board source](https://github.com/espressif/esp-webrtc-solution/tree/main/components/codec_board) for details.

## Prerequisites

1. **ESP-IDF v5.4 or higher** - Install from [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)

2. **Token service** - The **app** fetches the token. Use your own backend or Stream's pronto. The example includes `app_token.c` which calls:
   ```
   GET <STREAM_AUTH_BASE_URL>api/auth/create-token?environment=xxx&user_id=xxx&exp=xxx
   ```
   Response must be JSON: `{ "userId", "apiKey", "token" }`. See [auth_flow.md](../../docs/auth_flow.md).

## Building and Flashing

For a detailed step-by-step (including Windows and partition tables), see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md). Summary:

### 1. Set up ESP-IDF environment
```bash
# On Linux/macOS
. $HOME/esp/esp-idf/export.sh

# On Windows
%userprofile%\esp\esp-idf\export.bat
```

### 2. Navigate to example directory
```bash
cd examples/minimal
```

### 3. Configure target (ESP32-S3)
```bash
idf.py set-target esp32s3
```

### 4. Configure menuconfig
```bash
idf.py menuconfig
```

**Two required settings:**
1. **WiFi** — Under **Stream Video Example**, set your WiFi SSID and password.
2. **Board selection** — Under **Stream Video SDK → Camera Board**, choose the board that matches your hardware. This sets the correct camera pin map and audio codec. Using the wrong board will cause capture failures at runtime.

**Optional:** For 16MB flash boards, change the partition table:
`Partition Table → Partition Table (Custom) → Custom partition table CSV` →
`partitions_16mb.csv` (default is 8MB).

### 5. Build the project
```bash
idf.py build
```

### 6. Flash to device
```bash
# Connect ESP32-S3 via USB
idf.py flash
```

### 7. Monitor output
```bash
idf.py monitor
```

## Troubleshooting

### WiFi Connection Issues
- Set WiFi in `idf.py menuconfig` (Stream Video Example) or in `sdkconfig.defaults`; ensure SSID and password are correct
- Ensure ESP32-S3 is in range of WiFi network
- Check WiFi router supports 2.4GHz (ESP32-S3 doesn't support 5GHz)

### Auth Request Fails
- Ensure backend server is running and accessible
- The token service URL is set in the app (`STREAM_AUTH_BASE_URL` in main.c). See [auth_flow.md](../../docs/auth_flow.md).
- Verify backend returns correct JSON format

### Coordinator Connection Fails
- Check internet connectivity
- Verify auth token is valid
- Check coordinator URL: `wss://video.stream-io-api.com/video/connect`

### Join Call Fails
- Verify call_type and call_id are correct
- Check user has permission to join the call
- Ensure backend auth token includes call permissions

## Next Steps

This example already implements auth, coordinator and SFU connection, join call, and audio/video publishing. Optional improvements:

- Improve networking and reconnects as needed
- Add remote media rendering (subscribe to and display remote tracks)
- Extend WebRTC negotiation or data channels as needed

## See Also

- [Example Configuration Guide](../../docs/example_configuration.md)
- [Auth Flow](../../docs/auth_flow.md)
