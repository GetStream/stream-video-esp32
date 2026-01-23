# Build Fixes Required

## Files That Need Manual Fixes

### 1. `components/stream-video/include/stream_video_sfu.h`
**Line 14** - Add after existing includes:
```c
#include <stddef.h>
```

### 2. `components/stream-video/src/signaling/websocket_client.c`

**Line 97** - Change:
```c
stream_signaling_handle_message(client, data->data_ptr, data->data_len);
```
To:
```c
stream_signaling_handle_message(client, (const uint8_t *)data->data_ptr, data->data_len);
```

**Line 104** - Change:
```c
.data = data->data_ptr,
```
To:
```c
.data = (void *)data->data_ptr,  // Cast to void* to avoid const qualifier warning
```

**Line 324** - Change:
```c
int len = esp_websocket_client_send(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```
To:
```c
// Use esp_websocket_client_send_text for text messages (JSON signaling)
// The deprecated esp_websocket_client_send was removed in newer versions
int len = esp_websocket_client_send_text(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

### 3. `components/stream-video/src/sfu/sfu_client.c`

**Line 85** - Change:
```c
.data = data->data_ptr,
```
To:
```c
.data = (void *)data->data_ptr,  // Cast to void* to avoid const qualifier warning
```

**Line 224** - Change:
```c
int len = esp_websocket_client_send(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```
To:
```c
// Use esp_websocket_client_send_text for text messages
// The deprecated esp_websocket_client_send was removed in newer versions
int len = esp_websocket_client_send_text(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

### 4. `components/stream-video/src/utils/error_handler.c`

**Line 9** - Remove or comment out unused TAG:
```c
// static const char *TAG = "stream_error";  // Unused for now
```

### 5. `components/stream-video/src/utils/token_utils.c`

**Line 66** - Remove unused variable or use it:
```c
// size_t payload_len = second_dot - first_dot - 1;  // Unused for now
```

## Summary

1. ✅ `stream_video_signaling.h` - Already fixed (added `#include <stddef.h>`)
2. ⚠️ `stream_video_sfu.h` - Need to add `#include <stddef.h>`
3. ⚠️ `websocket_client.c` - Fix 3 issues (pointer cast, const qualifier, deprecated API)
4. ⚠️ `sfu_client.c` - Fix 2 issues (const qualifier, deprecated API)
5. ⚠️ `error_handler.c` - Remove unused TAG
6. ⚠️ `token_utils.c` - Remove unused variable
