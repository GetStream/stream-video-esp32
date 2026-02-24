# API Reference

This API reference is incomplete. More functions will be added as the SDK develops.

## Contents

- [Initialization](#initialization) — `stream_video_init`, `stream_video_deinit`
- [Error Handling](#error-handling) — `stream_video_error_to_string`, error codes
- [Types](#types) — client and room handles
- More sections to come.

## Initialization

### stream_video_init

Initialize the Stream Video SDK.

```c
stream_video_error_t stream_video_init(void);
```

**Returns:**
- `STREAM_VIDEO_ERR_OK` on success
- Error code on failure

**Note:** This must be called before using any other SDK functions.

### stream_video_deinit

Deinitialize the Stream Video SDK.

```c
void stream_video_deinit(void);
```

## Error Handling

### stream_video_error_to_string

Get a human-readable error string.

```c
const char* stream_video_error_to_string(stream_video_error_t error);
```

**Parameters:**
- `error`: Error code

**Returns:** Error string

## Error Codes

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

## Types

### stream_video_client_handle_t

Opaque handle for Stream Video client.

### stream_video_room_handle_t

Opaque handle for Stream Video room.



