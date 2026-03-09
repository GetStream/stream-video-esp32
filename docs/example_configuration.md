# Example Configuration Guide

This guide explains how to configure the minimal example: token service, user, environment, call, and publishing. The main app logic lives in **main.c**.

## Configuration options

1. **Token service** – Where the app fetches the token (your backend or Stream’s)
2. **User / environment** – Passed to the token service (user_id, environment name)
3. **Call selection** – Which call to join (call_type and call_id)
4. **Publishing** – Mute audio/video; sink from the default capture provider

## Configuration in main.c

Edit the configuration section near the top of `examples/minimal/main/main.c`:

```c
// Token service (app fetches token, then passes to SDK)
#define STREAM_AUTH_BASE_URL "https://pronto.getstream.io/"  // Your backend or Stream's token service
#define STREAM_ENVIRONMENT "pronto"   // "production", "staging", or "pronto"
#define STREAM_USER_ID "esp32_user"   // User ID (can be NULL for auto-generated)

// Call selection
#define STREAM_CALL_TYPE "default"    // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "79cYh3J5JgGk" // Call ID to join (can be NULL to create new call)
```

Flow:

1. The app calls `app_request_auth_data(STREAM_AUTH_BASE_URL, STREAM_ENVIRONMENT, STREAM_USER_ID, exp, &auth_data)` to get auth data from the token service.
2. The app passes `auth_data` in `stream_video_join_call_params_t.auth_data` to `stream_video_join_call()`.
3. The SDK uses that auth data for coordinator, joinCall, and SFU; it does not fetch the token.

Join parameters:

- **auth_data** – From your token fetch (required). Must contain valid `user_id`, `api_key`, and `token`.
- **call_type** – From `STREAM_CALL_TYPE`
- **call_id** – From `STREAM_CALL_ID` (or `NULL` to create a new call)
- **create** – Set to `true` to create the call if it does not exist
- **result_cb** – Callback with join success or failure
- **sink** – `NULL` in the example; the SDK uses the capture provider instead
- **mute_audio** / **mute_video** – Set to mute publishing

The example uses the default capture provider via `stream_video_set_capture_provider(...)`, so the SDK prepares and stops capture automatically.

## Complete flow

1. `stream_video_init()`
2. `stream_video_set_capture_provider(...)` (optional; example uses default capture)
3. Fetch token: `app_request_auth_data(..., &auth_data)`
4. `stream_video_join_call(&params, &client)` with `params.auth_data = &auth_data`
5. `stream_video_leave_call(client)` when done

## WiFi and SDK options

- **WiFi** – SSID and password are set via `sdkconfig.defaults` or **idf.py menuconfig** under **Stream Video Example**.
- **Token service URL** – Change `STREAM_AUTH_BASE_URL` to point to your own backend that returns `{ userId, apiKey, token }`. See [Authentication](auth_flow.md).
- **Video, audio, board, ICE** – All SDK options (resolution, bitrate, board, STUN/TURN, etc.) are under **Stream Video SDK** in menuconfig. See [SDK configuration reference](sdk_configuration.md).
