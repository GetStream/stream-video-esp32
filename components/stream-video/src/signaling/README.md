# Stream Video Signaling Module

This module implements the WebSocket-based signaling protocol for Stream Video SDK on ESP32.

## Components

### 1. WebSocket Client (`websocket_client.c`)
- Manages WebSocket connection to Stream's signaling server
- Handles connection, disconnection, and reconnection
- Implements automatic reconnection with configurable intervals and max attempts
- Uses ESP-IDF's `esp_websocket_client` component
- **No singleton pattern** - client handle passed explicitly
- **Event loop based** - no unnecessary synchronization

### 2. Signaling Protocol (`signaling_protocol.c`)
- Implements Stream's signaling message format (JSON-based)
- Creates connect, join room, publish track messages
- Parses incoming messages
- Uses cJSON for JSON handling

### 3. Message Handler (`message_handler.c`)
- Routes incoming messages to appropriate handlers
- Handles server responses (connect, join room)
- Handles events (track published, participant joined/left)
- Processes error responses

## Features

- **Authentication**: Token-based authentication via Stream's token system
- **Reconnection**: Automatic reconnection with exponential backoff (configurable)
- **Event-driven**: Uses callbacks for events (connected, disconnected, message received)
- **Thread-safe**: Uses FreeRTOS event groups and tasks (minimal synchronization)
- **No singleton**: All functions take explicit client handle

## Usage Example

```c
// Configure signaling client
stream_signaling_config_t config = {
    .url = "wss://your-stream-signaling-url.com",
    .user_id = "user123",
    .token = "your-auth-token",
    .event_cb = on_signaling_event,
    .user_data = NULL,
    .reconnect_interval_ms = 5000,
    .reconnect_max_attempts = 10
};

// Create client
stream_signaling_client_handle_t client;
stream_signaling_client_create(&config, &client);

// Connect
stream_signaling_client_connect(client);

// Send message (e.g., join room)
uint8_t *message;
size_t message_len;
stream_signaling_create_join_room_message("room123", "user123", &message, &message_len);
stream_signaling_client_send(client, message, message_len);
free(message);
```

## Message Types

### Client -> Server
- Connect (with user_id and token)
- Join Room
- Leave Room
- Publish Track
- Unpublish Track
- Subscribe Track
- Unsubscribe Track

### Server -> Client
- Connect Response
- Join Room Response
- Track Published Event
- Track Subscribed Event
- Participant Joined Event
- Participant Left Event
- Error Response

## Design Decisions

1. **JSON over Protobuf for signaling**: Stream's signaling uses JSON, so we use cJSON for parsing
2. **Event loop based**: Uses ESP-IDF event loops, no unnecessary mutexes
3. **Explicit handles**: No singleton pattern, all functions take client handle
4. **Automatic reconnection**: Handled in background task with configurable behavior

