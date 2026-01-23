# Testing the Signaling Module on ESP32-S3

This guide explains how to test the Stream Video signaling module on an actual ESP32-S3 device.

## Prerequisites

1. **ESP-IDF v5.4 or higher** installed and configured
2. **ESP32-S3 development board** (e.g., ESP32-S3-DevKitC-1)
3. **USB cable** to connect the board to your computer
4. **WiFi network** (2.4GHz)
5. **Stream account** with video app credentials

## Quick Start

### 1. Get Stream Credentials

1. Sign up at [getstream.io](https://getstream.io) if you don't have an account
2. Create a new Video app
3. Get your API key and secret
4. Generate a user token (you can use Stream's token generator or your backend)
5. Get the WebSocket signaling URL (usually something like `wss://your-app.getstream.io/video/ws`)

### 2. Configure the Example

Edit `examples/minimal/main/main.c`:

```c
// Set your WiFi credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Set your Stream credentials
#define STREAM_SIGNALING_URL "wss://your-app.getstream.io/video/ws"
#define STREAM_USER_ID "user123"
#define STREAM_TOKEN "your-generated-token"
```

### 3. Build and Flash

```bash
# Navigate to the example directory
cd examples/minimal

# Set target to ESP32-S3
idf.py set-target esp32s3

# Build the project
idf.py build

# Flash to device (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash monitor
```

**Note:** On macOS, the port is usually `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
On Linux, it's usually `/dev/ttyUSB0` or `/dev/ttyACM0`
On Windows, it's usually `COM3` or similar

### 4. Monitor Output

You should see output like:

```
I (xxx) minimal_example: ========================================
I (xxx) minimal_example: Stream Video ESP32 - Signaling Test
I (xxx) minimal_example: ========================================
I (xxx) minimal_example: ✓ Stream Video SDK initialized
I (xxx) minimal_example: ✓ Signaling client created
I (xxx) minimal_example: WiFi initialized, connecting to YOUR_WIFI_SSID...
I (xxx) minimal_example: Got IP: 192.168.1.100
I (xxx) minimal_example: Connecting to Stream signaling server...
I (xxx) stream_signaling_ws: WebSocket connected
I (xxx) minimal_example: ✓ Connected to Stream signaling server
I (xxx) minimal_example: Sending connect message...
I (xxx) minimal_example: Signaling state: CONNECTED
```

## What to Test

### Basic Connection Test
- ✅ WiFi connects successfully
- ✅ WebSocket connects to Stream signaling server
- ✅ Connect message is sent
- ✅ Server responds (check logs for received messages)

### Reconnection Test
- Disconnect WiFi temporarily
- Verify automatic reconnection when WiFi comes back
- Check that signaling reconnects automatically

### Message Exchange Test
- Send join room message
- Send publish track message
- Verify server responses are received and parsed

## Troubleshooting

### WiFi Issues
- **Problem:** WiFi doesn't connect
- **Solution:** 
  - Verify SSID and password are correct
  - Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
  - Check signal strength

### Signaling Connection Issues
- **Problem:** Can't connect to Stream signaling server
- **Solution:**
  - Verify WebSocket URL is correct (should start with `wss://`)
  - Check user ID and token are valid
  - Ensure device has internet connectivity
  - Check Stream dashboard for errors

### Build Issues
- **Problem:** Build fails
- **Solution:**
  - Ensure ESP-IDF v5.4+ is installed
  - Run `idf.py fullclean` and rebuild
  - Check all dependencies are installed: `idf.py reconfigure`

### Runtime Issues
- **Problem:** Device crashes or reboots
- **Solution:**
  - Check available heap memory: `idf.py monitor` shows heap info
  - Increase task stack sizes if needed
  - Check for memory leaks

## Next Steps

Once signaling is working:
1. Test joining a room
2. Test publishing audio/video tracks
3. Test subscribing to remote tracks
4. Integrate with WebRTC (Phase 5)

## Getting Help

- Check the logs for error messages
- Review Stream Video documentation: https://getstream.io/video/docs/
- Check ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/

