# Example Configuration Guide

This guide explains how to configure the minimal example to select user, environment, call, and publishing options. The main app logic lives in **main.c** (not a separate complete_example.c).

## Configuration options

The example allows you to configure:

1. **User selection** – Which user to authenticate as
2. **Environment selection** – Which environment to use (production/staging/development)
3. **Call selection** – Which call to join (call_type and call_id)
4. **Publishing** – Mute audio/video; sink comes from the default capture provider

## Configuration in main.c

Edit the configuration section near the top of `examples/minimal/main/main.c`:

```c
// User and Environment Selection (for auth request)
#define STREAM_ENVIRONMENT "production"  // "production", "staging", or "development"
#define STREAM_USER_ID "user123"         // User ID to authenticate as (can be NULL for auto-generated)

// Call Selection (for joining call)
#define STREAM_CALL_TYPE "default"       // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "call123"         // Call ID to join (can be NULL to create new call)
```

Join parameters are passed to `stream_video_join_call()`:

- **environment** – From `STREAM_ENVIRONMENT`
- **user_id** – From `STREAM_USER_ID` (or `NULL` for auto-generated)
- **call_type** – From `STREAM_CALL_TYPE`
- **call_id** – From `STREAM_CALL_ID` (or `NULL` to create a new call)
- **create** – Set to `true` to create the call if it does not exist
- **result_cb** – Callback with join success or failure
- **sink** – `NULL` in the example; the SDK uses the capture provider instead
- **mute_audio** / **mute_video** – Set to mute publishing

The example registers the default capture provider with `stream_video_set_capture_provider(stream_video_default_capture_prepare, stream_video_default_capture_stop, NULL)`, so the SDK prepares and stops capture automatically.

## User selection

### Option 1: Specify user ID
```c
#define STREAM_USER_ID "user123"  // Authenticate as this user
```

### Option 2: Auto-generate user ID
```c
#define STREAM_USER_ID NULL  // Backend will generate a user ID
```

**What happens:**
- If `STREAM_USER_ID` is set, the auth request includes `user_id` parameter
- If `STREAM_USER_ID` is NULL, backend generates a user ID

## Environment selection

```c
#define STREAM_ENVIRONMENT "production"  // or "staging" or "development"
```

**What happens:**
- Environment is passed to auth request: `GET api/auth/create-token?environment=production`
- Backend returns credentials for the selected environment

## Call selection

### Call type
```c
#define STREAM_CALL_TYPE "default"  // or "livestream", etc.
```

**What happens:**
- Call type is used in join call URL: `/video/calls/{call_type}/{call_id}/join`

### Call ID

#### Option 1: Join existing call
```c
#define STREAM_CALL_ID "call123"  // Join this specific call
```

#### Option 2: Create new call
```c
#define STREAM_CALL_ID NULL  // Create a new call (use with create=true in join params)
```

**What happens:**
- If `STREAM_CALL_ID` is set, joins that specific call
- If `STREAM_CALL_ID` is NULL and `create` is true in join params, creates a new call

## Complete flow (SDK-managed)

The app only calls:

1. `stream_video_init()`
2. `stream_video_set_capture_provider(...)` (optional; example uses default capture)
3. `stream_video_join_call(&params, &client)` with the above configuration
4. `stream_video_leave_call(client)` when done

The SDK internally: requests auth → connects to coordinator → joins call via REST → connects to SFU → starts publishing after SFU join response.

## Example configurations

### Example 1: Join existing call as specific user
```c
#define STREAM_ENVIRONMENT "production"
#define STREAM_USER_ID "alice"
#define STREAM_CALL_TYPE "default"
#define STREAM_CALL_ID "meeting-room-1"
```

### Example 2: Create new call as auto-generated user
```c
#define STREAM_ENVIRONMENT "staging"
#define STREAM_USER_ID NULL
#define STREAM_CALL_TYPE "default"
#define STREAM_CALL_ID NULL  // and set create=true in join params
```

### Example 3: Join livestream call
```c
#define STREAM_ENVIRONMENT "production"
#define STREAM_USER_ID "broadcaster"
#define STREAM_CALL_TYPE "livestream"
#define STREAM_CALL_ID "live-event-2024"
```

## WiFi and other options

- **WiFi** – SSID and password are set via `sdkconfig.defaults` or **idf.py menuconfig** under "Stream Video Example".
- **Runtime configuration** – Currently configuration is compile-time (`#define`). For runtime configuration you could use NVS, serial, or menuconfig.
