# Using the Stream Video SDK in Your Own App

This guide explains how to add and use the Stream Video ESP32 SDK in a **new or existing ESP-IDF project** (outside this repository).

## Option A: Git dependency (recommended)

Add the SDK as a Git dependency so the Component Manager downloads it and its transitive dependencies automatically.

### 1. Add the SDK dependency

In your **project root**, create or edit `idf_component.yml`:

```yaml
dependencies:
  stream-video:
    git: "https://github.com/GetStream/stream-video-esp32.git"
    path: "components/stream-video"
    version: "v0.1.0"
```

Then run `idf.py reconfigure` (or `idf.py build`) so the Component Manager downloads the SDK into `managed_components/`.

### 2. Require the SDK in your main component

In your **main component** (e.g. `main/CMakeLists.txt`), list the SDK in `idf_component_register()`:

```cmake
idf_component_register(
    SRCS "main.c" ...
    INCLUDE_DIRS "."
    REQUIRES
        stream-video
        nvs_flash
        esp_wifi
        esp_netif
        # ... other components your app needs
)
```

You do **not** need `EXTRA_COMPONENT_DIRS` when using the Component Manager; the SDK is under `managed_components/`.

---

## Option B: Using the SDK from source (clone or submodule)

If you are developing against a local clone, point your project at the SDK's components directory.

### 1. Get the SDK source

- **Clone:**
  `git clone https://github.com/GetStream/stream-video-esp32.git`
  (e.g. next to your project or in a `vendor` folder.)
- **Or submodule:**
  `git submodule add https://github.com/GetStream/stream-video-esp32.git path/to/stream-video-esp32`

### 2. Add the SDK's components directory to your project

In your **project root** `CMakeLists.txt`, set `EXTRA_COMPONENT_DIRS` to the `components` directory inside the SDK repo.

Example (SDK repo at `../stream-video-esp32` relative to your project):

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../stream-video-esp32/components")

project(my_video_app)
```

Adjust the path if you cloned or placed the repo elsewhere.

### 3. Declare registry dependencies

The SDK depends on registry components (`esp_peer`, `esp_capture`, etc.). Your project must pull them. In your **project root**, create or edit `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_peer: "^1.2.7"
  espressif/esp_capture: "*"
  espressif/esp_video_codec: "*"
  espressif/esp_audio_codec: "*"
  espressif/esp_websocket_client: "^1.6.1"
  espressif/cjson: "*"
  livekit/nanopb: "^0.4.9"
```

Then run `idf.py reconfigure` or `idf.py build`.

### 4. Require the SDK in your main component

Same as Option A: in your main component's `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c" ...
    INCLUDE_DIRS "."
    REQUIRES
        stream-video
        nvs_flash
        esp_wifi
        # ... etc.
)
```

---

## Summary

| Step | Git dependency (Option A) | From source (Option B) |
|------|---------------------------|------------------------|
| 1 | Add `stream-video` git dep in `idf_component.yml` | Clone/submodule repo; set `EXTRA_COMPONENT_DIRS` |
| 2 | — | Add SDK's registry deps to your `idf_component.yml` |
| 3 | In main `CMakeLists.txt`: `REQUIRES stream-video` | Same |

After that, use the SDK API (e.g. `stream_video_init()`, `stream_video_join_call()`) as in [Getting started](getting_started.md) and the [minimal example](../examples/minimal/).
