# Getting Started with Stream Video ESP32 SDK

This guide will help you get started with the Stream Video ESP32 SDK.

## Prerequisites

1. **ESP-IDF v5.4 or higher** - Install from [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

2. **ESP32-S3 or ESP32-P4 device** - This SDK is designed for these chips

3. **Stream account** - Sign up at [getstream.io](https://getstream.io)

## Using the SDK in your own project

If you have a **new or existing ESP-IDF app** (outside this repo), see **[Using the SDK in your app](using_sdk_in_your_app.md)** for how to add the dependency (Component Manager or from source) and wire it into your main component.

## Installation

From your **ESP-IDF project directory** (your own app or this repo’s example), run:

```bash
idf.py add-dependency "GetStream/stream-video-esp32=^0.1.0"
```

This uses **ESP-IDF's Component Manager** (built into ESP-IDF) to download the Stream Video SDK and its dependencies. The package includes the core SDK and the default camera/microphone capture component for ESP32-S3 and ESP32-P4.

## Application flow

The app uses a small API:

1. **Initialize** – `stream_video_init()`
2. **Set capture provider** – `stream_video_set_capture_provider(prepare_cb, stop_cb, user_data)` so the SDK can capture and publish audio/video (e.g. use the default capture component for camera and microphone)
3. **Get auth data** – The **app** fetches a token from your backend or Stream’s token service and fills a `stream_video_auth_data_t` (userId, apiKey, token). The SDK does not fetch tokens. See [Authentication](auth_flow.md).
4. **Join call** – `stream_video_join_call(&params, &client)` with **params.auth_data** (the auth data from step 3), call type, call ID, mute flags, and a result callback
5. **Leave call** – `stream_video_leave_call(client)`

The SDK uses the auth data for coordinator, joinCall, and SFU. Publishing starts automatically once you have joined.

## Basic example

See `examples/minimal/` for a complete working example. It uses the **stream-video-capture-default** component for camera and microphone capture on ESP32-S3/P4. The example fetches the token in app code (`app_token.c`) and passes auth data to the SDK. Configuration (WiFi in menuconfig; token service URL, user, call type/call ID in `main.c`; video/audio/board in **Stream Video SDK** menuconfig) is described in [Example configuration](example_configuration.md) and [SDK configuration](sdk_configuration.md).

## Next Steps

- Read the [API Reference](api_reference.md)
- Check out the examples in the `examples/` directory
- Review the [Stream Video Documentation](https://getstream.io/video/docs/)


