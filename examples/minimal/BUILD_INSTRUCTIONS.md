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

Edit `main/main.c` and set your WiFi credentials:

```c
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

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

## Build Steps

### 1. Navigate to Example Directory

```bash
cd /Users/pratimmallick/Projects/Stream/stream-video-esp32/examples/minimal
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
