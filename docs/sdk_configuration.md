# SDK Configuration Reference

This document describes all **Stream Video SDK** Kconfig options. They appear in **idf.py menuconfig** under **Component config → Stream Video SDK**. The example app adds two options under **Stream Video Example** (WiFi SSID and password only); see [Example configuration](example_configuration.md).

---

## Where to configure

- **menuconfig:** `idf.py menuconfig` → **Component config** → **Stream Video SDK** (and **Stream Video SDK → Debug** for debug options).
- **sdkconfig.defaults:** Set `CONFIG_<NAME>=value` for non-interactive or CI builds.

---

## Core / tasks

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **Join flow task stack size** (`STREAM_VIDEO_JOIN_TASK_STACK`) | int (4096–32768) | 12288 | Stack size in bytes for the join flow task (coordinator, joinCall, SFU connect). Increase if you add features or see stack overflows. |
| **Video encoder task stack size** (`STREAM_VIDEO_VENC_TASK_STACK`) | int (32768–262144) | 131072 | Stack size in bytes for the H.264 video encoder task(s). Increase if the encoder overflows. |

---

## ICE / STUN / TURN

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **STUN server override** (`STREAM_VIDEO_STUN_OVERRIDE`) | string | `stun:stun.l.google.com:19302` | Override the ICE server list with a single STUN URL. **Leave empty** to use the SFU-provided ICE servers (recommended for production). |
| **Allow TURN over TCP/TLS when provided** (`STREAM_VIDEO_ALLOW_TCP_TURN`) | bool | n | When enabled, use TURN URLs with `transport=tcp` or `turns://` when the SFU offers them. Enable if UDP is blocked. |
| **Use STUN only (ignore TURN servers)** (`STREAM_VIDEO_STUN_ONLY`) | bool | n | Ignore TURN servers from the SFU and use only STUN. Use as a fallback when TURN causes issues. |

---

## Board / hardware

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **Camera board pin map** (`STREAM_VIDEO_BOARD_*`) | choice | ESP32-S3 WROOM | Pin mapping for the camera. Options: **ESP32-S3 WROOM (default wiring)**, **XIAO ESP32-S3 Sense (OV2640/OV3660)**. |
| **Codec board type** (`STREAM_CODEC_BOARD_TYPE`) | string (derived) | Set by board choice | Audio codec board identifier. Derived from the camera board choice unless overridden (e.g. in `sdkconfig.defaults`). |

---

## Video capture / encode

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **Force fixed video caps** (`STREAM_VIDEO_USE_FIXED_CAPS`) | bool | y | When enabled, the pipeline uses the configured width, height, fps, and pixel format. When disabled, camera driver defaults may be used. |
| **Video width (pixels)** (`STREAM_VIDEO_WIDTH`) | int (160–1920) | 640 | Capture and encode width. |
| **Video height (pixels)** (`STREAM_VIDEO_HEIGHT`) | int (120–1080) | 480 | Capture and encode height. |
| **Video frame rate (fps)** (`STREAM_VIDEO_FPS`) | int (1–30) | 15 | Capture and encode frame rate. |
| **Video bitrate (bps)** (`STREAM_VIDEO_BITRATE`) | int (50000–5000000) | 800000 | Target H.264 bitrate in bits per second. |
| **Camera pixel format** (`STREAM_VIDEO_CAMERA_FMT_*`) | choice | YUV422 (YUYV) | Pixel format from the camera. **YUV422 (YUYV)** is typical for OV sensors; use **YUV420 (I420)** only if the sensor supports it. |

---

## Audio capture / encode

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **Enable audio capture/publish** (`STREAM_AUDIO_ENABLE`) | bool | y | Enable microphone capture and Opus publish. Disable for video-only. |
| **Audio sample rate (Hz)** (`STREAM_AUDIO_SAMPLE_RATE`) | int (8000–48000) | 16000 | Microphone sample rate. |
| **Audio channel count** (`STREAM_AUDIO_CHANNELS`) | int (1–2) | 2 | Number of channels to capture and encode. |
| **Audio bits per sample** (`STREAM_AUDIO_BITS_PER_SAMPLE`) | int (16–32) | 16 | Bit depth of captured samples. |
| **Audio bitrate (bps)** (`STREAM_AUDIO_BITRATE`) | int (6000–128000) | 32000 | Target Opus bitrate in bits per second. |
| **Audio input gain (dB)** (`STREAM_AUDIO_INPUT_GAIN`) | int (0–60) | 30 | Hardware input gain for the microphone/codec. |
| **Enable AGC (ALC) in audio pipeline** (`STREAM_AUDIO_AGC_ENABLE`) | bool | y | Automatic level control to stabilize voice loudness. |
| **AGC base gain (dB)** (`STREAM_AUDIO_AGC_GAIN_DB`) | int (-12–12) | 0 | Base gain applied by AGC. Negative values reduce sensitivity. Only used when AGC is enabled. |
| **Audio I2S mode** (`STREAM_AUDIO_I2S_MODE_*`) | choice | TDM on ESP32-S3, else standard | **I2S standard** or **I2S TDM** for the codec/microphone interface. |

---

## Debug (Stream Video SDK → Debug)

All options in the **Debug** submenu are for development and diagnostics. Leave them **disabled** for production.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| **Log STUN UDP traffic** (`STREAM_VIDEO_UDP_STUN_TRACE`) | bool | n | Compiles a wrapper that logs STUN send/recv (IP:port, size, txid). To use it, you must also add linker wrap options to your project CMakeLists.txt (see the option’s help in menuconfig). |
| **Run resolution/encoder test on boot** (`STREAM_VIDEO_RUN_RES_TEST`) | bool | n | Runs a short encoder test over several resolutions at boot and then stops. Use to find stable resolution/caps. |
| **Probe mic level before publish** (`STREAM_AUDIO_LEVEL_PROBE`) | bool | n | Reads raw mic samples briefly and logs RMS/peak level. |
| **Mic level probe duration (ms)** (`STREAM_AUDIO_LEVEL_PROBE_MS`) | int (100–3000) | 500 | Duration of the mic level probe. Only used when level probe is enabled. |
| **Enable audio frame monitor** (`STREAM_AUDIO_DEBUG_MONITOR`) | bool | n | Logs audio frame counts for a few seconds after capture starts. |
| **Dump Opus frames to RAM** (`STREAM_AUDIO_DUMP_OPUS`) | bool | n | Writes encoded Opus frames to a RAM buffer for inspection. |
| **Opus dump duration (seconds)** (`STREAM_AUDIO_DUMP_SECONDS`) | int (1–300) | 30 | How long to dump Opus frames. Only when dump is enabled. |
| **Opus dump RAM cap (bytes)** (`STREAM_AUDIO_DUMP_MAX_BYTES`) | int (1024–2097152) | 262144 | Maximum RAM for the Opus dump buffer. Only when dump is enabled. |

---

## Example-only options (Stream Video Example menu)

These are in the **main** component of the minimal example, not in the SDK:

| Option | Type | Description |
|--------|------|-------------|
| **WiFi SSID** (`STREAM_VIDEO_WIFI_SSID`) | string | WiFi network name for the example. |
| **WiFi password** (`STREAM_VIDEO_WIFI_PASSWORD`) | string | WiFi password. |

Your own app can use different Kconfig symbols or hardcode credentials (not recommended); the SDK does not reference these.

---

## See also

- [Authentication](auth_flow.md) – App-side token fetch and passing auth data to the SDK
- [Example configuration](example_configuration.md) – Token service URL, call type/call ID, and flow
