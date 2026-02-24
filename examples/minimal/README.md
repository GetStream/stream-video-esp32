# Stream Video ESP32-S3 - Complete Example

This example demonstrates the complete flow matching the Android demo app:
1. Request auth data (with user and environment selection)
2. Connect to coordinator WebSocket
3. Join call (with call_type and call_id selection)
4. Connect to SFU WebSocket
5. WebRTC negotiation (to be implemented)

## Configuration

**WiFi:** Set SSID and password in `sdkconfig.defaults` (e.g. `CONFIG_STREAM_VIDEO_WIFI_SSID="MyWiFi"`) or run `idf.py menuconfig` and go to **Stream Video Example**. The example reads these from Kconfig; do not hardcode credentials in source.

**Stream (environment, user, call):** Edit `main/main.c` and set:

- `STREAM_ENVIRONMENT` — e.g. `"production"`, `"staging"`, or `"development"`
- `STREAM_USER_ID` — user ID for auth (or `NULL` for auto-generated)
- `STREAM_CALL_TYPE` — e.g. `"default"`, `"livestream"`
- `STREAM_CALL_ID` — call ID to join, or `NULL` to create a new call

### Audio Capture
Audio publishing uses the microphone when a supported codec/mic board is present.
It uses Espressif's `codec_board` component to initialize the audio codec and provide a record handle.
Use `idf.py menuconfig` and configure:
- `Stream Video Example → Enable audio capture/publish`
- `Stream Video Example → Audio sample rate / channel count / bitrate / I2S mode`
Audio codec board selection follows the camera board pin map to avoid mismatches.
If you need a different audio board type, add `CONFIG_STREAM_CODEC_BOARD_TYPE="..."`
to `sdkconfig.defaults` or `sdkconfig`.
For `XIAO_ESP32S3_Sense`, audio input uses the PDM mic path automatically.

Supported boards from `codec_board` (as of esp-webrtc-solution main):
- `S3_Korvo_V2`
- `S3_Korvo_V4`
- `ESP32_S3_KORVO_2L`
- `ESP32_KORVO_V1`m
- `ESP32_LYRAT_V43`
- `LYRAT_MINI_V1`
- `ESP32S3_BOX`
- `ESP32_S3_BOX_3`
- `ESP32S3_EYE`
- `ESP32_P4_DEV_V14`
- `ESP32_P4_EYE`
- `ESP32_S3_EchoEar`
- `XIAO_ESP32S3_Sense`
- `ATOMS3_ECHO_BASE`
- `XD_AIOT_C3`
- `ESP_SPOT`
- `DUMMY_CODEC_BOARD`

If your board is not listed:
1. Build once so the component is downloaded.
2. Copy `examples/minimal/managed_components/tempotian__codec_board` to
   `examples/minimal/components/codec_board`.
3. Edit `examples/minimal/components/codec_board/board_cfg.txt` and add your board
   definition (pins + codec type).
4. Set `CONFIG_STREAM_CODEC_BOARD_TYPE` to your new board name and rebuild.

If you see `Failed to resolve component 'codec_board'` during build:
- Your ESP-IDF component registry does not have `tempotian/codec_board`.
- Manually vendor the component by copying it into
  `examples/minimal/components/codec_board` (from
  `https://github.com/espressif/esp-webrtc-solution/tree/main/components/codec_board`)
  and rebuild.

## Prerequisites

1. **ESP-IDF v5.4 or higher** - Install from [ESP-IDF Getting Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)

2. **Backend Server** - You need a backend that implements the auth endpoint. The SDK uses the default auth base URL (see [Auth base URL](../../docs/auth_flow.md#auth-base-url) in the docs). Example:
   ```
   GET https://pronto.getstream.io/api/auth/create-token?environment=xxx&user_id=xxx&exp=xxx
   ```
   Response:
   ```json
   {
       "userId": "user123",
       "apiKey": "my-app:abc123",
       "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
   }
   ```

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

### 4. Select partition table
This example defaults to an 8MB layout. For 16MB boards, switch to the
provided 16MB table:
```bash
idf.py menuconfig
```
Then set:
`Partition Table → Partition Table (Custom) → Custom partition table CSV` to
`partitions_16mb.csv`.

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

## Expected Output

When running successfully, you should see:

```
I (1234) main: ========================================
I (1234) main: Stream Video ESP32-S3 - Complete Flow
I (1234) main: ========================================
I (1234) main: Configuration:
I (1234) main:   Environment: production
I (1234) main:   User ID: user123
I (1234) main:   Call Type: default
I (1234) main:   Call ID: your-call-id
I (1234) main: ========================================
I (1234) main: ✓ NVS initialized
I (1234) main: ✓ Stream Video SDK initialized
I (1234) main: WiFi initialized, connecting to MyWiFi...
I (2345) main: Got IP: 192.168.1.100
I (2345) main: WiFi connected, starting Stream Video flow...
I (2345) main: ✓ Auth data received
I (2345) main:   User ID: user123
I (2345) main:   API Key: my-app:abc123
I (2345) main: Connecting to coordinator WebSocket...
I (3456) main: ✓ Connected to coordinator WebSocket
I (3456) main: Sending connect message to coordinator...
I (3456) main: Joining call: type=default, id=your-call-id
I (4567) main: ✓ Join call successful
I (4567) main:   Call ID: your-call-id
I (4567) main:   SFU WebSocket: wss://sfu-123.getstream.io/video/ws
I (4567) main: ✓ Connecting to SFU WebSocket...
I (5678) main: ✓ Connected to SFU WebSocket
I (5678) main: Ready for WebRTC negotiation
```

## Troubleshooting

### WiFi Connection Issues
- Set WiFi in `idf.py menuconfig` (Stream Video Example) or in `sdkconfig.defaults`; ensure SSID and password are correct
- Ensure ESP32-S3 is in range of WiFi network
- Check WiFi router supports 2.4GHz (ESP32-S3 doesn't support 5GHz)

### Auth Request Fails
- Ensure backend server is running and accessible
- The auth base URL is configured in the SDK (default: `pronto.getstream.io`). See [Auth base URL](../../docs/auth_flow.md#auth-base-url) for where it is defined and how to change it.
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
- [Coordinator and SFU Flow](../../docs/coordinator_sfu_flow.md)
- [SDK Flow After Auth](../../docs/sdk_flow_after_auth.md)
