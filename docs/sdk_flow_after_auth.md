# SDK Flow After Authentication

This document describes the complete sequence of steps the SDK performs after authentication is completed.

## Overview

After authentication, the SDK goes through these phases:
1. **Signaling Connection** - Connect to Stream's signaling server
2. **Session Establishment** - Authenticate with Stream
3. **Room Management** - Join/leave rooms
4. **Track Management** - Publish and subscribe to tracks
5. **WebRTC Negotiation** - Set up media connections
6. **Media Flow** - Actual audio/video streaming

## Complete Flow Diagram (Matches Android SDK)

```
Authentication Complete
    ↓
[1] Connect to Coordinator WebSocket (Signaling)
    URL: wss://video.stream-io-api.com/video/connect
    ↓
[2] Send Connect Message (authenticate via coordinator)
    ↓
[3] Receive Connect Response
    ↓
[4] Join Call via Coordinator REST API
    POST /video/calls/{call_type}/{call_id}/join
    ↓
[5] Receive JoinCallResponse with SFU Credentials
    - credentials.server.wsEndpoint (SFU WebSocket URL)
    - credentials.server.url (SFU HTTP URL)
    - credentials.token (SFU token)
    - credentials.iceServers (STUN/TURN)
    ↓
[6] Connect to SFU WebSocket (Media Signaling)
    URL: From credentials.server.wsEndpoint
    ↓
[7] WebRTC Negotiation via SFU WebSocket
    - SDP offers/answers
    - ICE candidates
    ↓
[8] Media Flow (Direct to SFU via WebRTC)
    - Audio/Video streams
    - RTP/SRTP packets
```

## Detailed Steps

### Step 1: Connect to Coordinator WebSocket

**URL:** `wss://video.stream-io-api.com/video/connect` (default, matches Android SDK)

**Purpose:** Signaling and discovery service

After receiving auth data, create coordinator client:

```c
stream_signaling_config_t config = {
    .api_key = auth_data->api_key,      // From auth response
    .user_id = auth_data->user_id,      // From auth response
    .token = auth_data->token,           // From auth response
    .coordinator_url = NULL,             // NULL = use default
    .event_cb = on_coordinator_event,    // Event callback
    .user_data = NULL,
    .reconnect_interval_ms = 5000,
    .reconnect_max_attempts = 10,
};

stream_signaling_client_handle_t coordinator_client;
stream_signaling_client_create(&config, &coordinator_client);
stream_signaling_client_connect(coordinator_client);
```

**What happens:**
- Connects to coordinator WebSocket (default: `wss://video.stream-io-api.com/video/connect`)
- Initializes WebSocket client
- Sets up event handlers
- Creates reconnection task

**Events:**
- `STREAM_SIGNALING_EVENT_CONNECTED` - Coordinator WebSocket connected

### Step 3: Send Connect Message (Authenticate)

Once WebSocket is connected, send authentication message:

```c
// In STREAM_SIGNALING_EVENT_CONNECTED callback
uint8_t *message = NULL;
size_t message_len = 0;

stream_signaling_create_connect_message(
    auth_data->user_id,
    auth_data->token,
    NULL,  // session_id (optional)
    &message,
    &message_len
);

stream_signaling_client_send(client, message, message_len);
free(message);
```

**Message format:**
```json
{
    "type": 1,
    "user_id": "user123",
    "token": "jwt-token-here",
    "session_id": null
}
```

**What happens:**
- Sends connect request to Stream server
- Server validates token
- Server establishes session

### Step 4: Receive Connect Response

Server responds with connect result:

```c
// Handled in message_handler.c
void handle_connect_response(client, json) {
    // Parse response: { "success": true, "session_id": "..." }
    // Store session_id if needed
}
```

**Response format:**
```json
{
    "type": "connect_response",
    "success": true,
    "session_id": "session-abc123",
    "error_message": null
}
```

**What happens:**
- Session established
- Ready to join rooms

### Step 5: Join Room

Request to join a room:

```c
uint8_t *message = NULL;
size_t message_len = 0;

stream_signaling_create_join_room_message(
    "room123",      // room_id
    auth_data->user_id,
    &message,
    &message_len
);

stream_signaling_client_send(client, message, message_len);
free(message);
```

**Message format:**
```json
{
    "type": 3,
    "room_id": "room123",
    "user_id": "user123"
}
```

**What happens:**
- Server adds user to room
- Server sends participant list
- Server sends existing tracks in room

### Step 6: Receive Join Room Response

Server confirms room join:

```c
// Handled in message_handler.c
void handle_join_room_response(client, json) {
    // Parse: { "success": true, "room_id": "..." }
    // Room joined successfully
}
```

**Response format:**
```json
{
    "type": "join_room_response",
    "success": true,
    "room_id": "room123",
    "error_message": null
}
```

**Events received:**
- `ParticipantJoinedEvent` - For each existing participant
- `TrackPublishedEvent` - For each existing track

### Step 7: WebRTC Negotiation via SFU WebSocket

Once connected to SFU WebSocket, WebRTC negotiation happens:

**SDP Exchange (via SFU WebSocket):**
- SFU sends SDP offer for track
- Client creates SDP answer (using esp_peer)
- Send SDP answer via SFU WebSocket

**ICE Candidate Exchange (via SFU WebSocket):**
- Client gathers ICE candidates (using STUN/TURN from credentials.iceServers)
- Exchange candidates via SFU WebSocket
- Establish direct connection to SFU

**What happens:**
- WebRTC PeerConnection created (using esp_peer)
- SDP offers/answers exchanged via SFU WebSocket
- ICE candidates exchanged via SFU WebSocket
- DTLS handshake completed
- Media path established directly to SFU

### Step 8: Media Flow

Once WebRTC is established:

**Audio/Video Streaming:**
- Media flows directly: ESP32 ←→ Stream SFU (bypassing coordinator)
- Coordinator WebSocket: Still used for control (room events, participant updates)
- SFU WebSocket: Used for media signaling only
- RTP/SRTP packets carry media
- Opus audio encoding
- H.264 video encoding

**Ongoing Operations:**
- Track publish/unpublish (via coordinator)
- Track subscribe/unsubscribe (via coordinator)
- Participant join/leave events (via coordinator)
- WebRTC media signaling (via SFU WebSocket)
- Reconnection handling

## State Machine

```
[INITIAL]
    ↓
[AUTHENTICATING] → Request auth data
    ↓
[AUTHENTICATED] → Auth data received
    ↓
[CONNECTING] → Connect to signaling
    ↓
[CONNECTED] → WebSocket connected
    ↓
[AUTHENTICATING_SESSION] → Send connect message
    ↓
[SESSION_ESTABLISHED] → Connect response received
    ↓
[JOINING_ROOM] → Send join room message
    ↓
[IN_ROOM] → Join room response received
    ↓
[PUBLISHING_TRACKS] → Publish tracks
    ↓
[SUBSCRIBING_TRACKS] → Subscribe to tracks
    ↓
[WEBRTC_NEGOTIATING] → SDP/ICE exchange
    ↓
[MEDIA_FLOWING] → Media streaming
```

## Event Callbacks

Throughout the flow, events are delivered via callbacks:

```c
void on_signaling_event(stream_signaling_client_handle_t client,
                       const stream_signaling_event_t *event,
                       void *user_data)
{
    switch (event->type) {
        case STREAM_SIGNALING_EVENT_CONNECTED:
            // WebSocket connected - send connect message
            break;
            
        case STREAM_SIGNALING_EVENT_MESSAGE_RECEIVED:
            // Message received - handled by stream_signaling_handle_message
            break;
            
        case STREAM_SIGNALING_EVENT_DISCONNECTED:
            // Disconnected - reconnection will be attempted
            break;
            
        case STREAM_SIGNALING_EVENT_ERROR:
            // Error occurred
            break;
    }
}
```

## Summary

After authentication, the SDK:

1. ✅ **Connects** to Stream signaling server (WebSocket)
2. ✅ **Authenticates** session with token
3. ✅ **Joins** room(s)
4. ✅ **Publishes** local tracks (audio/video)
5. ✅ **Subscribes** to remote tracks
6. ✅ **Negotiates** WebRTC connection (SDP/ICE)
7. ✅ **Streams** media via WebRTC to SFU

All of this happens through the signaling server, which coordinates everything before media can flow directly to the SFU.

