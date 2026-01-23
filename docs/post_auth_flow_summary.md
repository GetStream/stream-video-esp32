# Post-Authentication Flow - Quick Reference

## Step-by-Step Flow After Authentication

### ✅ Step 1: Create Signaling Client
**After:** Auth data received (userId, apiKey, token)

**Action:**
```c
stream_signaling_client_create(&config, &client);
```

**What it does:**
- Constructs signaling URL: `wss://[app_name].pronto.getstream.io/video/ws`
- Initializes WebSocket client
- Sets up event handlers

---

### ✅ Step 2: Connect to Signaling Server
**Action:**
```c
stream_signaling_client_connect(client);
```

**What it does:**
- Opens WebSocket connection
- Waits for connection established

**Event:** `STREAM_SIGNALING_EVENT_CONNECTED`

---

### ✅ Step 3: Send Connect Message (Authenticate Session)
**When:** WebSocket connected event received

**Action:**
```c
stream_signaling_create_connect_message(user_id, token, NULL, &msg, &len);
stream_signaling_client_send(client, msg, len);
```

**Message sent:**
```json
{
    "type": 1,
    "user_id": "user123",
    "token": "jwt-token"
}
```

**What it does:**
- Authenticates with Stream server
- Establishes session
- Gets session ID

**Response:** `connect_response` with success/session_id

---

### ✅ Step 4: Join Room
**When:** Connect response received (session established)

**Action:**
```c
stream_signaling_create_join_room_message(room_id, user_id, &msg, &len);
stream_signaling_client_send(client, msg, len);
```

**Message sent:**
```json
{
    "type": 3,
    "room_id": "room123",
    "user_id": "user123"
}
```

**What it does:**
- Requests to join a room
- Server adds user to room
- Server sends participant list

**Response:** `join_room_response` + events:
- `participant_joined` events (for existing participants)
- `track_published` events (for existing tracks)

---

### ✅ Step 5: Publish Tracks
**When:** Room joined successfully

**Action:**
```c
// Publish audio
stream_signaling_create_publish_track_message(
    "track-audio-1", "audio", "microphone", &msg, &len);
stream_signaling_client_send(client, msg, len);

// Publish video
stream_signaling_create_publish_track_message(
    "track-video-1", "video", "camera", &msg, &len);
stream_signaling_client_send(client, msg, len);
```

**Message sent:**
```json
{
    "type": 5,
    "track_id": "track-audio-1",
    "kind": "audio",
    "track_type": "microphone"
}
```

**What it does:**
- Announces intent to publish track
- Server coordinates with SFU
- Server assigns track IDs

**Response:** `track_published` event

---

### ✅ Step 6: Subscribe to Remote Tracks
**When:** Remote tracks available (from track_published events)

**Action:**
```c
// Subscribe to remote track
// (Function to be implemented)
stream_signaling_create_subscribe_track_message(
    "track-remote-1", "participant-456", &msg, &len);
stream_signaling_client_send(client, msg, len);
```

**What it does:**
- Requests subscription to remote track
- Server initiates WebRTC negotiation
- Server sends SDP offer

---

### ✅ Step 7: WebRTC Negotiation
**When:** Track subscription requested

**Actions:**
1. **Receive SDP Offer** (via signaling)
2. **Create SDP Answer** (using esp_peer)
3. **Send SDP Answer** (via signaling)
4. **Exchange ICE Candidates** (via signaling)
5. **Establish WebRTC Connection** (to SFU)

**What it does:**
- Sets up WebRTC PeerConnection (esp_peer)
- Negotiates codecs
- Establishes DTLS connection
- Creates media path to SFU

---

### ✅ Step 8: Media Flow
**When:** WebRTC connection established

**What happens:**
- **Audio/Video streams** flow directly: ESP32 ←→ Stream SFU
- **Signaling** is only used for control messages
- **Media encoding**: Opus (audio), H.264 (video)
- **Transport**: RTP/SRTP over WebRTC

**Ongoing:**
- Track publish/unpublish
- Track subscribe/unsubscribe
- Participant events
- Reconnection handling

---

## Complete Sequence

```
[Auth Complete]
    ↓
1. Create Signaling Client
    ↓
2. Connect WebSocket → STREAM_SIGNALING_EVENT_CONNECTED
    ↓
3. Send Connect Message → connect_response
    ↓
4. Join Room → join_room_response + participant/track events
    ↓
5. Publish Tracks → track_published events
    ↓
6. Subscribe to Tracks → SDP offer received
    ↓
7. WebRTC Negotiation (SDP/ICE) → WebRTC connected
    ↓
8. Media Flows → Audio/Video streaming
```

## Key Points

- **Signaling is control plane**: All coordination happens via WebSocket
- **Media is data plane**: Actual streams go directly to SFU via WebRTC
- **Event-driven**: Everything happens via callbacks/events
- **Automatic reconnection**: Handled by signaling client
- **No singleton**: All operations use explicit handles

## Current Implementation Status

✅ **Implemented:**
- Steps 1-5 (Signaling connection, authentication, room join, track publish)

⏳ **To be implemented:**
- Step 6 (Subscribe tracks - function exists but needs completion)
- Step 7 (WebRTC negotiation - Phase 5)
- Step 8 (Media flow - Phase 6)

## Next Phases

- **Phase 4**: SFU Connection Module
- **Phase 5**: WebRTC Integration
- **Phase 6**: Media Integration

