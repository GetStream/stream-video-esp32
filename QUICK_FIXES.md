# Quick Fixes for Build Errors

## File: `components/stream-video/src/sfu/sfu_client.c`

### Fix 1: Line 85 - Const qualifier warning
**Find:**
```c
                    .data = data->data_ptr,
```

**Replace with:**
```c
                    .data = (void *)data->data_ptr,  // Cast to void* to avoid const qualifier warning
```

### Fix 2: Line 224 - Deprecated API
**Find:**
```c
    int len = esp_websocket_client_send(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

**Replace with:**
```c
    // Use esp_websocket_client_send_text for text messages
    // The deprecated esp_websocket_client_send was removed in newer versions
    int len = esp_websocket_client_send_text(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

## File: `components/stream-video/src/signaling/websocket_client.c`

### Fix 1: Line 97 - Pointer type mismatch
**Find:**
```c
                stream_signaling_handle_message(client, data->data_ptr, data->data_len);
```

**Replace with:**
```c
                // Cast to uint8_t* to match function signature
                stream_signaling_handle_message(client, (const uint8_t *)data->data_ptr, data->data_len);
```

### Fix 2: Line 104 - Const qualifier warning
**Find:**
```c
                    .data = data->data_ptr,
```

**Replace with:**
```c
                    .data = (void *)data->data_ptr,  // Cast to void* to avoid const qualifier warning
```

### Fix 3: Line 324 - Deprecated API
**Find:**
```c
    int len = esp_websocket_client_send(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

**Replace with:**
```c
    // Use esp_websocket_client_send_text for text messages (JSON signaling)
    // The deprecated esp_websocket_client_send was removed in newer versions
    int len = esp_websocket_client_send_text(client->ws_client, (const char *)message, message_len, portMAX_DELAY);
```

## Quick Find & Replace Commands

If you're using VS Code or similar editor, you can use these find/replace patterns:

1. **Find:** `esp_websocket_client_send(client->ws_client, (const char *)message, message_len, portMAX_DELAY);`
   **Replace:** `esp_websocket_client_send_text(client->ws_client, (const char *)message, message_len, portMAX_DELAY);`

2. **Find:** `.data = data->data_ptr,`
   **Replace:** `.data = (void *)data->data_ptr,  // Cast to void* to avoid const qualifier warning`

3. **Find:** `stream_signaling_handle_message(client, data->data_ptr, data->data_len);`
   **Replace:** `stream_signaling_handle_message(client, (const uint8_t *)data->data_ptr, data->data_len);`
