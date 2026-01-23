# Coordinator and SFU Connection Flow

This document describes the two-WebSocket architecture matching the Stream Android SDK.

## Architecture Overview

Stream Video uses a **two-WebSocket architecture**:

1. **Coordinator WebSocket** - For signaling and discovery
2. **SFU WebSocket** - For media signaling (obtained from coordinator)

```
┌─────────────┐
│   ESP32     │
│   Device    │
└──────┬──────┘
       │
       ├───► Coordinator WebSocket (Signaling)
       │     wss://video.stream-io-api.com/video/connect
       │
       └───► SFU WebSocket (Media Signaling)
             wss://[sfu-endpoint] (from joinCall response)
```

## Complete Flow

### Step 1: Connect to Coordinator WebSocket

**URL:** `wss://video.stream-io-api.com/video/connect` (default, matches Android SDK)

**Purpose:** Signaling and discovery service

```c
stream_signaling_config_t config = {
    .api_key = auth_data->api_key,
    .user_id = auth_data->user_id,
    .token = auth_data->token,
    .coordinator_url = NULL,  // NULL = use default
    // ...
};

stream_signaling_client_handle_t coordinator_client;
stream_signaling_client_create(&config, &coordinator_client);
stream_signaling_client_connect(coordinator_client);
```

**What happens:**
- Connects to coordinator WebSocket
- Used for signaling messages
- Ready to join calls

### Step 2: Join Call via Coordinator REST API

**Endpoint:** `POST https://video.stream-io-api.com/video/calls/{call_type}/{call_id}/join`

**Purpose:** Get SFU credentials

```c
stream_video_join_call_request_t join_req = {
    .api_key = auth_data->api_key,
    .user_id = auth_data->user_id,
    .token = auth_data->token,
    .call_type = "default",
    .call_id = "call123",  // Optional
    .create = true,        // Create if doesn't exist
};

stream_video_coordinator_join_call(&join_req, on_join_call_response, NULL);
```

**Response (JoinCallResponse):**
```json
{
    "call": {
        "id": "call123"
    },
    "credentials": {
        "server": {
            "wsEndpoint": "wss://sfu-123.getstream.io/video/ws",
            "url": "https://sfu-123.getstream.io"
        },
        "token": "sfu-auth-token",
        "iceServers": [
            {
                "urls": ["stun:stun.getstream.io:3478"]
            }
        ]
    }
}
```

### Step 3: Extract SFU Credentials

From the joinCall response, extract:

- `credentials.server.wsEndpoint` → SFU WebSocket URL
- `credentials.server.url` → SFU HTTP API URL
- `credentials.token` → SFU authentication token
- `credentials.iceServers` → STUN/TURN servers for WebRTC

```c
void on_join_call_response(const stream_video_join_call_response_t *response, void *user_data) {
    if (response->success) {
        // Extract SFU credentials
        stream_video_sfu_credentials_t *creds = &response->credentials;
        
        // creds->ws_endpoint = "wss://sfu-123.getstream.io/video/ws"
        // creds->token = "sfu-auth-token"
        // creds->ice_servers = "[{...}]"
    }
}
```

### Step 4: Connect to SFU WebSocket

**URL:** From `credentials.server.wsEndpoint` (e.g., `wss://sfu-123.getstream.io/video/ws`)

**Purpose:** Media signaling (SDP/ICE exchange for WebRTC)

```c
stream_sfu_config_t sfu_config = {
    .ws_endpoint = response->credentials.ws_endpoint,  // From joinCall
    .token = response->credentials.token,               // From joinCall
    .event_cb = on_sfu_event,
    .user_data = NULL,
};

stream_sfu_client_handle_t sfu_client;
stream_sfu_client_create(&sfu_config, &sfu_client);
stream_sfu_client_connect(sfu_client);
```

**What happens:**
- Connects to SFU WebSocket
- Used for WebRTC SDP/ICE exchange
- Media signaling happens here

### Step 5: WebRTC Negotiation via SFU

Once connected to SFU WebSocket:

1. **Receive SDP Offer** (via SFU WebSocket)
2. **Create SDP Answer** (using esp_peer)
3. **Send SDP Answer** (via SFU WebSocket)
4. **Exchange ICE Candidates** (via SFU WebSocket)
5. **Establish WebRTC Connection** (direct to SFU)

### Step 6: Media Flow

Once WebRTC is established:
- Media flows directly: ESP32 ←→ SFU
- Coordinator WebSocket: Still used for control (room events, etc.)
- SFU WebSocket: Used for media signaling only

## Summary

| Connection | URL Source | Purpose |
|------------|------------|---------|
| **Coordinator WebSocket** | Default: `wss://video.stream-io-api.com/video/connect` | Signaling, discovery |
| **SFU WebSocket** | From `joinCall` response: `credentials.server.wsEndpoint` | Media signaling (SDP/ICE) |
| **WebRTC** | Direct to SFU (from credentials) | Actual media streams |

## Key Differences from Previous Implementation

**Before (incorrect):**
- Single WebSocket connection
- URL constructed from API key
- Mixed signaling and media

**After (correct, matches Android SDK):**
- ✅ Two WebSocket connections (coordinator + SFU)
- ✅ Coordinator URL: `wss://video.stream-io-api.com/video/connect`
- ✅ SFU URL: From `joinCall` REST API response
- ✅ Clear separation: Coordinator = signaling, SFU = media signaling

## Implementation Status

✅ **Implemented:**
- Coordinator WebSocket client
- joinCall REST API client
- SFU WebSocket client
- Credentials parsing

⏳ **To be completed:**
- Integration in example
- WebRTC negotiation via SFU
- Media flow

