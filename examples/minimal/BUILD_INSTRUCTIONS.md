# Build and Flash Instructions for ESP32-S3

This guide will walk you through building and flashing the Stream Video ESP32 example on your ESP32-S3 board.

## Prerequisites

### 1. Install ESP-IDF

You need ESP-IDF v5.4 or higher installed.

**Linux/macOS:**
```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

**Windows:**
Download and run the ESP-IDF installer from:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/windows-setup.html

### 2. Set up ESP-IDF Environment

**Linux/macOS:**
```bash
. ~/esp/esp-idf/export.sh
```

**Windows:**
```cmd
%userprofile%\esp\esp-idf\export.bat
```

**PowerShell:**
```powershell
$HOME/esp/esp-idf/export.ps1
```

Verify installation:
```bash
idf.py --version
```

### 3. Install Python Dependencies

ESP-IDF requires Python 3.8+. The installer should handle this, but verify:
```bash
python3 --version
```

## Configuration

### 1. Configure WiFi

Set WiFi in `sdkconfig.defaults` (e.g. `CONFIG_STREAM_VIDEO_WIFI_SSID="MyWiFi"`) or run `idf.py menuconfig` and go to **Stream Video Example**. The example reads credentials from Kconfig.

### 2. Configure Stream Settings

Edit `main/main.c` and configure Stream:

```c
// User and Environment Selection
#define STREAM_ENVIRONMENT "production"  // or "staging", "development"
#define STREAM_USER_ID "user123"       // or NULL for auto-generated

// Call Selection
#define STREAM_CALL_TYPE "default"      // e.g., "default", "livestream"
#define STREAM_CALL_ID "call123"       // or NULL to create new call
```

### 3. Backend Server

Ensure you have a backend server running that implements:
```
GET https://pronto.getstream.io/api/auth/create-token?environment=xxx&user_id=xxx&exp=xxx
```

## Menuconfig reference (Stream Video Example)

Run `idf.py menuconfig` and open the top-level **Stream Video Example** menu. Below is a snapshot of the options and what to tweak.

### Menu snapshot

```
Stream Video Example
├── WiFi SSID                          [string]
├── WiFi password                      [string]
├── STUN server override               [string]  (empty = use SFU list)
├── Allow TURN over TCP/TLS            [n/y]
├── Use STUN only (ignore TURN)        [n/y]
├── Video encoder task stack size      [32768–262144]
├── Camera board pin map               (ESP32-S3 WROOM | XIAO ESP32-S3 Sense)
├── Force fixed video caps             [y/n]
├── Run resolution/encoder test on boot [n/y]
├── Video width (pixels)               [160–1920]
├── Video height (pixels)              [120–1080]
├── Video frame rate (fps)             [1–30]
├── Video bitrate (bps)                [50000–5000000]
├── Enable audio capture/publish       [y/n]
├── Audio sample rate (Hz)             [8000–48000]
├── Audio channel count                [1–2]
├── Audio bits per sample              [16–32]
├── Audio bitrate (bps)                [6000–128000]
├── Audio input gain (dB)               [0–60]
├── Enable AGC (ALC)                   [y/n]
├── AGC base gain (dB)                 [-12–12]
├── (debug) Probe mic level            [n/y]
├── (debug) Audio frame monitor        [n/y]
├── (debug) Dump Opus frames to RAM    [n/y]
├── Audio I2S mode                     (I2S standard | I2S TDM)
└── Camera pixel format                (YUV422 | YUV420)

Component config → Stream Video SDK
├── Join flow task stack size          [4096–32768]
└── Video encoder task stack size     [16384–131072]  (SDK default)
```

### Options worth tweaking

| Area | Option | What it does |
|------|--------|--------------|
| **WiFi** | WiFi SSID / WiFi password | Your network. Set here or in `sdkconfig.defaults`. |
| **ICE/connectivity** | STUN server override | Leave **empty** to use SFU-provided ICE servers. Set e.g. `stun:stun.l.google.com:19302` to force a single STUN for testing. |
| | Use STUN only | Enable if TURN causes issues; uses only STUN from the SFU list. |
| | Allow TURN over TCP/TLS | Enable if your network blocks UDP; tries TCP/TLS TURN when offered. |
| **Board** | Camera board pin map | **ESP32-S3 WROOM** for generic wiring; **XIAO ESP32-S3 Sense** for that board. |
| **Video** | Video width / height / fps | Resolution and frame rate. Lower (e.g. 320×240, 15 fps) saves CPU and bandwidth. |
| | Video bitrate (bps) | H.264 target bitrate. 500k–1M is typical; increase for higher quality. |
| | Video encoder task stack size | Increase (e.g. 131072) if the encoder task overflows. |
| **Audio** | Enable audio capture/publish | Turn off to publish video only. |
| | Audio sample rate / bitrate | 16 kHz is typical for voice; 32 kbps Opus is usually enough. |
| | Audio input gain | Mic gain (dB). Increase if audio is too quiet. |
| | Enable AGC (ALC) | Keeps voice level stable; disable if it causes artifacts. |
| **Debug** | Run resolution/encoder test on boot | Runs a short encoder test at boot then stops; use to find stable resolution/caps. |
| | Probe mic level / Audio frame monitor / Dump Opus | Debug-only; leave off unless diagnosing audio. |

Settings are stored in `sdkconfig` in the example directory (do not commit if it contains secrets).

## Build Steps

**Note:** Pre-generated protobuf sources (`components/stream-video-protobuf/generated/`) are committed in the repo, so you can clone and build without installing `protoc` or nanopb. If you change any `.proto` files, you need `protoc` and `pip install nanopb` to regenerate those files and then commit the updated `generated/` directory.

### 1. Navigate to Example Directory

```bash
cd examples/minimal
```

### 2. Set Target Chip

```bash
idf.py set-target esp32s3
```

This will:
- Create `sdkconfig` file
- Configure the project for ESP32-S3

### 3. Configure (Optional)

If you need to customize settings:

```bash
idf.py menuconfig
```

Common settings:
- **Component config → ESP32S3-Specific** → CPU frequency
- **Component config → Wi-Fi** → WiFi settings
- **Component config → mbedTLS** → TLS settings

### 4. Build the Project

**If in example directory:**
```bash
idf.py build
```

**If in project root:**
```bash
idf.py -C examples/minimal build
```

This will:
- Download dependencies (if using Component Manager)
- Compile all source files
- Link the application
- Generate binary files

**Expected output:**
```
Project build complete. To flash, run:
idf.py -p PORT flash
```

### 5. Check Build Output

After successful build, you'll find:
- `build/stream-video-esp32.bin` - Main application binary
- `build/bootloader/bootloader.bin` - Bootloader
- `build/partition_table/partition-table.bin` - Partition table

## Flash Steps

### 1. Connect ESP32-S3

Connect your ESP32-S3 board to your computer via USB cable.

### 2. Find Serial Port

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**macOS:**
```bash
ls /dev/cu.*
```

**Windows:**
Check Device Manager → Ports (COM & LPT)

Common ports:
- Linux: `/dev/ttyUSB0`
- macOS: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- Windows: `COM3`, `COM4`, etc.

### 3. Flash the Firmware

**Automatic port detection:**
```bash
idf.py flash
```

**Manual port specification:**
```bash
idf.py -p /dev/ttyUSB0 flash    # Linux
idf.py -p /dev/cu.usbserial-* flash  # macOS
idf.py -p COM3 flash             # Windows
```

**With baud rate (if needed):**
```bash
idf.py -p PORT -b 921600 flash
```

### 4. Monitor Output

**Flash and monitor in one command:**
```bash
idf.py flash monitor
```

**Or monitor separately:**
```bash
idf.py monitor
```

**Exit monitor:** Press `Ctrl+]`

## Complete Build and Flash Command

All-in-one command:
```bash
cd examples/minimal
idf.py set-target esp32s3
idf.py build flash monitor
```

## Troubleshooting

### Build Errors

**Error: "idf.py: command not found"**
- Solution: Run `export.sh` or `export.bat` to set up ESP-IDF environment

**Error: "CMake Error"**
- Solution: Make sure you're in the `examples/minimal` directory
- Solution: Run `idf.py fullclean` and rebuild

**Error: "Component not found"**
- Solution: Run `idf.py reconfigure` to refresh dependencies
- Solution: Check `idf_component.yml` in project root

### Flash Errors

**Error: "Serial port not found"**
- Solution: Check USB cable connection
- Solution: Install USB-to-serial drivers (CP2102, CH340, etc.)
- Solution: Check port permissions (Linux: `sudo usermod -a -G dialout $USER`)

**Error: "Failed to connect"**
- Solution: Hold BOOT button while connecting
- Solution: Try different USB port/cable
- Solution: Check baud rate: `idf.py -p PORT -b 115200 flash`

**Error: "Permission denied"**
- Linux: `sudo chmod 666 /dev/ttyUSB0` (or add user to dialout group)
- macOS: Usually not needed, but check System Preferences → Security

### Runtime Errors

**WiFi connection fails:**
- Check SSID and password are correct
- Ensure WiFi is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check signal strength

**Auth request fails:**
- Verify backend server is running
- Check backend URL is accessible
- Verify backend returns correct JSON format

**Coordinator connection fails:**
- Check internet connectivity
- Verify auth token is valid
- Check firewall/network restrictions

## Quick Reference

```bash
# Setup (one time)
. ~/esp/esp-idf/export.sh  # Linux/macOS
idf.py set-target esp32s3

# Build and flash (every time)
cd examples/minimal
idf.py build flash monitor

# Clean build
idf.py fullclean
idf.py build

# Just monitor
idf.py monitor

# Flash only
idf.py flash
```

## Next Steps

After successful flash:
1. Monitor the serial output
2. Verify WiFi connection
3. Check auth request succeeds
4. Verify coordinator connection
5. Confirm call join works
6. Check SFU connection

## See Also

- [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/build-system.html)
- [Example README](README.md)
