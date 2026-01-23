# Stream Android SDK vs ESP32 SDK - Configuration Comparison

## Key Finding: No Manual Signaling URL Needed

After checking the Stream Android SDK, we found that **you do NOT need to provide a signaling URL manually**. The Android SDK constructs it automatically from the API key, just like we've now implemented in the ESP32 SDK.

## Android SDK Pattern

### What Android SDK Requires:
```kotlin
val client = StreamVideoBuilder(
    context = applicationContext,
    apiKey = "your-api-key",        // Only API key needed
    user = user,
    token = userToken
).build()
```

**No signaling URL parameter!** The SDK constructs it internally.

## ESP32 SDK Pattern (Now Matches Android)

### What ESP32 SDK Requires:
```c
stream_signaling_config_t config = {
    .api_key = "your-api-key",      // Only API key needed (matches Android)
    .user_id = "user123",
    .token = "your-token",
    .base_url = NULL,                // NULL = use default getstream.io
    .event_cb = on_event,
    .user_data = NULL
};
```

**No signaling URL needed!** It's constructed automatically from the API key.

## How URL Construction Works

### API Key Format
Stream API keys typically come in two formats:

1. **Full format**: `"app_name:key_part"`
   - Example: `"my-video-app:abc123xyz"`
   - We extract `"my-video-app"` as the app name

2. **App name only**: `"app_name"`
   - Example: `"my-video-app"`
   - Used directly as app name

### URL Construction
```
API Key: "my-video-app:abc123"
  ↓
Extract app name: "my-video-app"
  ↓
Construct URL: "wss://my-video-app.getstream.io/video/ws"
```

## Benefits of This Approach

✅ **Matches Android SDK** - Same API pattern, easier for developers familiar with Stream
✅ **Less configuration** - Users don't need to know the URL format
✅ **Automatic** - URL is always correct based on API key
✅ **Flexible** - Can override base URL for self-hosted Stream

## Migration from Old Pattern

If you were using the old pattern with explicit URL:

**Old (explicit URL):**
```c
#define STREAM_SIGNALING_URL "wss://my-app.getstream.io/video/ws"
config.url = STREAM_SIGNALING_URL;
```

**New (API key only, matches Android):**
```c
#define STREAM_API_KEY "my-app:abc123"
config.api_key = STREAM_API_KEY;  // URL constructed automatically
```

## Self-Hosted Stream

If you're using self-hosted Stream, you can override the base URL:

```c
stream_signaling_config_t config = {
    .api_key = "my-app",
    .base_url = "custom-domain.com",  // Override default getstream.io
    // ...
};
// Constructs: wss://my-app.custom-domain.com/video/ws
```

## Summary

- ✅ **Android SDK**: Only needs API key, constructs URL internally
- ✅ **ESP32 SDK**: Now matches Android - only needs API key, constructs URL internally
- ✅ **No manual URL needed**: Both SDKs handle it automatically
- ✅ **Same developer experience**: Familiar API for Stream developers

