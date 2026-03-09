# API Reference

This document describes the **application-facing API** for the Stream Video ESP32 SDK. The app uses a small set of calls: init, optional capture provider, join, and leave.

## Contents

- [Application flow](#application-flow)
- [Initialization](#initialization)
- [Capture provider](#capture-provider)
- [Join and leave](#join-and-leave)
- [Error handling](#error-handling)
- [Types](#types)

## Application flow

Recommended usage:

1. **Initialize** – `stream_video_init()`
2. **Optional: set capture provider** – `stream_video_set_capture_provider()` so the SDK prepares/stops capture (e.g. using the default capture component for ESP32-S3/P4)
3. **Get auth data** – The **app** fetches a token (userId, apiKey, token) from its backend or Stream’s token service and fills a `stream_video_auth_data_t`. The SDK does not fetch tokens. See [Authentication](auth_flow.md).
4. **Join call** – `stream_video_join_call(&params, &client)` with **params.auth_data** pointing to that auth data, plus call_type, call_id, mute flags, and result callback
5. **Leave call** – `stream_video_leave_call(client)`

The SDK uses the auth data for coordinator connection, joinCall REST, SFU WebSocket, and publishing. Publishing starts automatically after the SFU join response (no separate start/stop publish API).

---

## Initialization

### stream_video_init

Initialize the Stream Video SDK. Must be called before any other SDK functions.

```c
stream_video_error_t stream_video_init(void);
```

**Returns:** `STREAM_VIDEO_ERR_OK` on success, or an error code.

### stream_video_deinit

Deinitialize the Stream Video SDK.

```c
void stream_video_deinit(void);
```

---

## Capture provider

### stream_video_set_capture_provider

Register optional callbacks so the SDK prepares and stops capture when needed. If set, the app does not pass a sink in join params; the SDK calls `prepare_cb` when it needs a sink (after SFU connect) and `stop_cb` when leaving the call.

```c
void stream_video_set_capture_provider(
    stream_video_capture_prepare_cb_t prepare_cb,
    stream_video_capture_stop_cb_t stop_cb,
    void *user_data);
```

**Parameters:**
- `prepare_cb` – Called when the SDK needs a sink; set `*sink_out` and return `STREAM_VIDEO_ERR_OK`. Pass `NULL` to clear.
- `stop_cb` – Called when leaving the call to release capture resources.
- `user_data` – Passed to both callbacks.

To use the built-in default capture (ESP32-S3/P4), link the `stream-video-capture-default` component and pass `stream_video_default_capture_prepare` and `stream_video_default_capture_stop` here.

---

## Join and leave

### stream_video_join_call

Join a call using the SDK-managed flow (coordinator, joinCall REST, SFU connect). The app must pass **auth_data** (from its own token fetch) in params. Publishing starts automatically after the SFU join response, using the sink from params or from the capture provider, and the mute flags.

```c
stream_video_error_t stream_video_join_call(
    const stream_video_join_call_params_t *params,
    stream_video_client_handle_t *client_out);
```

**Parameters:**
- `params` – Join parameters (see below).
- `client_out` – On success, receives the client handle.

**Returns:** `STREAM_VIDEO_ERR_OK` on success.

### stream_video_join_call_params_t

| Field        | Type                            | Description |
|-------------|----------------------------------|-------------|
| `auth_data`  | `const stream_video_auth_data_t *` | **Required.** Auth data (userId, apiKey, token) from the app’s token service. The app fetches the token; see [Authentication](auth_flow.md). |
| `call_type`   | `const char *`                 | Call type (e.g. `"default"`). |
| `call_id`     | `const char *`                 | Call ID; `NULL` to create a new call. |
| `create`      | `bool`                         | If true, create the call if it does not exist. |
| `location`    | `const char *`                 | Optional location hint; `NULL` for auto. |
| `result_cb`   | `stream_video_join_result_cb_t`| Called with join success or failure. |
| `user_data`   | `void *`                       | User context for `result_cb`. |
| `sink`        | `esp_capture_sink_handle_t`    | Optional. Capture sink for publishing; `NULL` to use capture provider or no publish. |
| `mute_audio`  | `bool`                         | If true, do not publish audio. |
| `mute_video`  | `bool`                         | If true, do not publish video. |

### stream_video_leave_call

Leave the call and release resources. Stops publishing and disconnects from the SFU and coordinator.

```c
stream_video_error_t stream_video_leave_call(stream_video_client_handle_t client);
```

**Parameters:** `client` – Handle returned from `stream_video_join_call`.

---

## Error handling

### stream_video_error_to_string

Get a human-readable string for an error code.

```c
const char* stream_video_error_to_string(stream_video_error_t error);
```

### stream_video_error_t

```c
typedef enum {
    STREAM_VIDEO_ERR_OK = 0,
    STREAM_VIDEO_ERR_INVALID_ARG,
    STREAM_VIDEO_ERR_NO_MEM,
    STREAM_VIDEO_ERR_INVALID_STATE,
    STREAM_VIDEO_ERR_NETWORK,
    STREAM_VIDEO_ERR_TIMEOUT,
    STREAM_VIDEO_ERR_FAIL,
} stream_video_error_t;
```

### stream_video_join_result_t

Passed to the join result callback:

```c
typedef struct {
    bool success;
    char error_message[256];
} stream_video_join_result_t;
```

---

## Types

- **stream_video_client_handle_t** – Opaque handle for the Stream Video client (from `stream_video_join_call`).
- **stream_video_room_handle_t** – Opaque handle for a room (reserved for future use).
- **stream_video_join_result_cb_t** – Callback type for join result: `void (*)(const stream_video_join_result_t *result, void *user_data)`.
- **stream_video_capture_prepare_cb_t** – Capture prepare callback: `stream_video_error_t (*)(void *user_data, esp_capture_sink_handle_t *sink_out)`.
- **stream_video_capture_stop_cb_t** – Capture stop callback: `void (*)(void *user_data)`.

---

## Default capture component (optional)

When using the **stream-video-capture-default** component (ESP32-S3 / ESP32-P4):

- **stream_video_default_capture_prepare** – Use as `prepare_cb` for `stream_video_set_capture_provider`.
- **stream_video_default_capture_stop** – Use as `stop_cb`.
- **stream_video_default_capture_run_resolution_test** – Optional diagnostic to probe supported resolutions.

Include `stream_video_capture.h` and add `stream-video-capture-default` to your project's `REQUIRES`.
