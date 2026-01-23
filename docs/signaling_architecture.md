# Stream Video Signaling Architecture

## Why Signaling URL is Needed

The Stream signaling URL is essential because **Stream Video uses a two-part architecture**:

1. **Signaling Server (WebSocket)** - Coordinates connections and exchanges control messages
2. **SFU (Selective Forwarding Unit)** - Handles actual media (audio/video) transport via WebRTC

## The Two-Phase Connection Process

### Phase 1: Signaling Connection (WebSocket)
```
ESP32 Device  ←→  Stream Signaling Server (WebSocket)
     ↓
- Authentication (user_id, token)
- Room management (join/leave)
- Track coordination (publish/subscribe)
- SDP exchange (WebRTC session descriptions)
- ICE candidate exchange (network connectivity)
```

### Phase 2: Media Connection (WebRTC)
```
ESP32 Device  ←→  Stream SFU (WebRTC)
     ↓
- Actual audio/video streams
- RTP/SRTP packets
- Media encoding/decoding
```

## What Signaling Does

The signaling server handles **all the coordination** needed before media can flow:

### 1. **Authentication & Authorization**
- Verifies your user token
- Checks permissions to join rooms
- Establishes session

### 2. **Room Coordination**
- Join/leave room requests
- Participant management
- Room state synchronization

### 3. **Track Management**
- Announce when you want to publish a track
- Coordinate track IDs
- Handle track subscriptions

### 4. **WebRTC Negotiation**
- Exchange SDP (Session Description Protocol) offers/answers
- Exchange ICE candidates (network connectivity info)
- Coordinate codec selection
- Set up encryption keys

### 5. **Event Notifications**
- Notify when participants join/leave
- Notify when tracks are published/subscribed
- Error notifications

## Why Not Direct Connection?

You might wonder: "Why not connect directly to the SFU?"

**Answer:** Because WebRTC needs coordination:

1. **Discovery**: How do devices find each other?
2. **Negotiation**: What codecs? What network paths?
3. **Security**: How do we exchange encryption keys securely?
4. **Coordination**: Who's in the room? What tracks are available?

The signaling server provides this coordination layer.

## Stream's Architecture

```
┌─────────────┐         ┌──────────────────┐         ┌─────────────┐
│   ESP32     │◄──WS───►│ Stream Signaling │         │   Browser   │
│   Device    │         │     Server       │◄──WS───►│   Client    │
└──────┬──────┘         └────────┬─────────┘         └──────┬──────┘
       │                         │                          │
       │                         │ Coordinates              │
       │                         │                          │
       │                         ▼                          │
       │                 ┌──────────────┐                  │
       └────WebRTC───────►│ Stream SFU   │◄────WebRTC──────┘
                          │  (Media)     │
                          └──────────────┘
```

## How It Works in Practice

### Step 1: Connect to Signaling
```c
// Connect to Stream signaling server via WebSocket
stream_signaling_client_connect(client);
// URL: wss://your-app.getstream.io/video/ws
```

### Step 2: Authenticate
```c
// Send connect message with credentials
stream_signaling_create_connect_message(user_id, token, ...);
// Server validates and establishes session
```

### Step 3: Join Room
```c
// Request to join a room
stream_signaling_create_join_room_message(room_id, user_id, ...);
// Server adds you to room, sends participant list
```

### Step 4: Publish Track
```c
// Announce you want to publish audio/video
stream_signaling_create_publish_track_message(track_id, "audio", ...);
// Server coordinates with SFU, returns track info
```

### Step 5: WebRTC Negotiation (via Signaling)
```c
// Exchange SDP offers/answers through signaling
// Exchange ICE candidates through signaling
// This happens automatically via signaling messages
```

### Step 6: Media Flows (Direct to SFU)
```c
// Once WebRTC is negotiated, media flows directly:
// ESP32 ←→ SFU (bypassing signaling server)
// Signaling is only used for control messages after this
```

## Getting the Signaling URL

The signaling URL is provided by Stream and typically follows this pattern:

```
wss://[your-app-name].getstream.io/video/ws
```

Or for self-hosted Stream:
```
wss://your-custom-domain.com/video/ws
```

You can find this in:
- Stream Dashboard → Your Video App → Settings
- Stream API documentation
- Your backend that generates tokens (if using server-side auth)

## Summary

**Signaling URL is needed because:**
1. ✅ It's the control plane for WebRTC coordination
2. ✅ It handles authentication and authorization
3. ✅ It coordinates room and participant management
4. ✅ It facilitates WebRTC SDP/ICE exchange
5. ✅ It's separate from media transport (which goes directly to SFU)

**Without signaling, you can't:**
- ❌ Authenticate with Stream
- ❌ Join rooms
- ❌ Coordinate WebRTC connections
- ❌ Publish or subscribe to tracks
- ❌ Know who else is in the room

The signaling server is the "traffic controller" that makes everything work together!

