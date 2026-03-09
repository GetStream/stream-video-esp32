# Authentication Flow

This document explains how authentication works with the Stream Video ESP32 SDK.

## App fetches token, SDK uses it

**The app is responsible for obtaining the auth token.** The SDK does not fetch tokens. You can get the token from:

- Your own backend (recommended for production)
- Stream's token service (e.g. pronto for development/demo)

The app calls its token service, receives `{ userId, apiKey, token }`, then passes that **auth data** to the SDK when joining a call.

## Auth data type

The SDK expects auth data in this form (see `stream_video_auth.h`):

```c
typedef struct stream_video_auth_data {
    char user_id[128];
    char api_key[256];
    char token[512];   // JWT
} stream_video_auth_data_t;
```

## Joining a call

Pass auth data in the join params:

```c
stream_video_auth_data_t auth_data = {0};
// ... obtain auth_data from your token service ...

stream_video_join_call_params_t params = {
    .auth_data = &auth_data,
    .call_type = "default",
    .call_id = "your-call-id",
    .create = true,
    .location = NULL,
    .result_cb = on_join_result,
    .user_data = NULL,
    .sink = NULL,
    .mute_audio = false,
    .mute_video = false,
};

stream_video_client_handle_t client = NULL;
stream_video_join_call(&params, &client);
```

## Token service endpoint (for app implementation)

If you use a backend that matches the Stream demo pattern, the endpoint is:

```
GET <base_url>api/auth/create-token?environment=<env>&exp=<seconds>&user_id=<optional>
```

### Query parameters

- `environment` (string): e.g. `"production"`, `"staging"`, `"pronto"`
- `user_id` (string, optional): User ID (omit for server-generated)
- `exp` (integer): Token expiry in seconds (e.g. 7 days = `604800`)

### Response format

```json
{
    "userId": "user123",
    "apiKey": "my-app:abc123",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

### Example: minimal example’s app token helper

The minimal example includes an **app-side** implementation in `app_token.c` / `app_token.h` that requests auth data from a configurable base URL (e.g. `https://pronto.getstream.io/`). Use it as reference or copy it into your app:

```c
#include "app_token.h"

stream_video_auth_data_t auth_data = {0};
stream_video_error_t err = app_request_auth_data(
    "https://pronto.getstream.io/",  // or your backend
    "pronto",                         // environment
    "esp32_user",                     // user_id (or NULL)
    STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,
    &auth_data);
if (err != STREAM_VIDEO_ERR_OK) {
    // handle error
}
// then pass &auth_data to stream_video_join_call params
```

## Creating a user object (optional)

If you need a user object (e.g. for display), create it from auth data:

```c
stream_video_user_t user;
stream_video_create_user_from_auth(
    &auth_data,
    STREAM_VIDEO_USER_ROLE_ADMIN,
    &user
);
```

## Quick reference

- **Token fetching**: Implemented by the **app** (own backend or Stream token service).
- **SDK**: Accepts `stream_video_auth_data_t` in `stream_video_join_call_params_t.auth_data`.
- **Token expiry**: Use `STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS` (7 days) or your own value when calling your token service.
- **Example**: See `examples/minimal/main/app_token.c` for a blocking HTTP request that fills `stream_video_auth_data_t`.
