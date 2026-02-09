# ESP Peer – WebRTC PeerConnection Component

`esp_peer` is a full-featured WebRTC PeerConnection implementation optimized for Espressif platforms. It enables peer-to-peer communication with audio, video, and data channels, adhering to the standard WebRTC protocol stack.

---

## 🔍 Overview

Derived from [libpeer](https://github.com/sepfy/libpeer.git), `esp_peer` adds platform-specific enhancements and optimizations for the ESP32 series. It provides a production-grade WebRTC stack supporting:

- **ICE** – Interactive Connectivity Establishment
- **DTLS** – Datagram Transport Layer Security
- **SCTP** – Stream Control Transmission Protocol
- **RTP/SRTP** – Real-time Transport Protocol with secure variants

---

## ✨ Key Features

### ✅ WebRTC Protocol Stack

- **TURN Support**: Implements [RFC5766](https://datatracker.ietf.org/doc/html/rfc5766) and [RFC8656](https://datatracker.ietf.org/doc/html/rfc8656)
- **Dual ICE Roles**: Operates as *Controlling* or *Controlled*
- **Optimized ICE Pairing**: Fast and efficient candidate selection
- **RTP Reliability**: Implements NACK, jitter buffering, and retransmission
- **Robust SCTP**: Supports multi-streaming, fragmentation, and SACK
- **Threaded Operation**: Non-blocking send/receive with dedicated tasks

### 🎹 Media Support

- **Audio Codecs**: G711A (PCMA), G711U (PCMU), OPUS
- **Video Codecs**: H.264, MJPEG
- **Data Channels**: Reliable/unreliable; ordered/unordered modes
- **Flexible Media Modes**: Send-only, receive-only, or full-duplex

### 🧹 Modular & Configurable

- **Pluggable Interfaces**: Clean abstraction via `esp_peer_ops_t`
- **Default Implementation**: Ready-to-use via `esp_peer_get_default_impl()`
- **Resource Control**: Tune memory usage, timeouts, buffer sizes

---

## 🚀 Getting Started

### 1️⃣ Peer Configuration

```c
esp_peer_cfg_t cfg = {
    .role = ESP_PEER_ROLE_CONTROLLING,
    .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL,
    .audio_info = {
        .codec = ESP_PEER_AUDIO_CODEC_OPUS,
        .sample_rate = 48000,
        .channel = 1
    },
    .video_info = {
        .codec = ESP_PEER_VIDEO_CODEC_H264,
        .width = 640,
        .height = 480,
        .fps = 30
    },
    .audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
    .video_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
    .enable_data_channel = true,
    .ctx = user_context,
    .on_state = state_callback,
    .on_msg = message_callback,
    .on_audio_data = audio_callback,
    .on_video_data = video_callback,
    .on_data = data_callback
};
```

---

### 2️⃣ Migration: Browser WebRTC → ESP Peer

Easily migrate from browser-style WebRTC code to the embedded `esp_peer` API. Refer to [`peer_demo.c`](examples/peer_demo/main/peer_demo.c) for a complete walkthrough.

#### 🔄 API Mapping

| Browser WebRTC                                     | ESP Peer                            | Notes                             |
| -------------------------------------------------- | ----------------------------------- | --------------------------------- |
| `new RTCPeerConnection()`                          | `esp_peer_open()`                   | Configuration-based instantiation |
| `pc.onicecandidate`                                | `.on_msg` callback                  | Candidate/SDP exchange            |
| `pc.onconnectionstatechange`                       | `.on_state` callback                | Connection lifecycle              |
| `pc.ontrack`                                       | `.on_audio_data` / `.on_video_data` | Media reception                   |
| `pc.ondatachannel`                                 | `.on_data` callback                 | Data channel messages             |
| `createOffer()` / `createAnswer()`                 | `esp_peer_new_connection()`         | Auto-generates SDP                |
| `setLocalDescription()` / `setRemoteDescription()` | `esp_peer_send_msg()`               | Exchange SDP manually             |
| `dataChannel.send()`                               | `esp_peer_send_data()`              | Send data via SCTP                |
| Manual event loop                                  | `esp_peer_main_loop()`              | Handled in background task        |

---

### 3️⃣ Data Channel Usage

#### ✔️ Auto Creation

If `.enable_data_channel = true`:

- **Client Role**: Creates the default channel automatically
- **Server Role**: Waits for peer to create and pair the channel

#### 🛠 Manual Creation

For advanced use cases (e.g., multiple or custom-configured channels):

```c
esp_peer_cfg_t cfg = {
    .enable_data_channel = true,
    .manual_ch_create = true,
    // ...
};
esp_peer_open(&cfg, esp_peer_get_default_impl(), &peer);

// After ESP_PEER_STATE_DATA_CHANNEL_CONNECTED
esp_peer_data_channel_cfg_t ch_cfg = {
    .type = ESP_PEER_DATA_CHANNEL_RELIABLE,
    .ordered = true,
    .label = "my_channel"
};
esp_peer_create_data_channel(peer, &ch_cfg);

// Once open:
esp_peer_data_frame_t data_frame = {
    .type = ESP_PEER_DATA_CHANNEL_STRING,
    .stream_id = 0,
    .data = (uint8_t*)"Hello WebRTC!",
    .size = 13
};
esp_peer_send_data(peer, &data_frame);
```

## 4️⃣ Signaling Explanation

WebRTC requires a signaling mechanism to exchange SDP and ICE candidates between peers. This module **does not provide a built-in signaling transport**.

You can refer to the complete [esp-webrtc-solution](https://github.com/espressif/esp-webrtc-solution) which includes practical signaling implementations such as handy OpenAI, WHIP, and AppRTC, or you may develop a custom signaling system tailored to your application.

---

## ⚙️ Fine-Tuning (Optional)

Tune for memory or performance trade-offs using `esp_peer_default_cfg_t`:

```c
esp_peer_default_cfg_t default_cfg = {
    .agent_recv_timeout = 100,
    .data_ch_cfg = {
        .cache_timeout = 5000,
        .send_cache_size = 102400,
        .recv_cache_size = 102400,
    },
    .rtp_cfg = {
        .audio_recv_jitter = {
            .cache_timeout = 100,
            .resend_delay = 20,
            .cache_size = 102400,
        },
        .video_recv_jitter = {
            .cache_timeout = 100,
            .resend_delay = 20,
            .cache_size = 409600,
        },
        .send_pool_size = 409600,
        .send_queue_num = 256,
        .max_resend_count = 3
    }
};

cfg.extra_cfg = &default_cfg;
cfg.extra_size = sizeof(default_cfg);
esp_peer_open(&cfg, esp_peer_get_default_impl(), &peer);
```

---

## 📉 Minimum Resource Requirements

`esp_peer` is highly configurable to support low-memory environments. Even on platforms **without PSRAM**, a minimal setup uses **< 60 KB** RAM.

See the [`peer_demo`](examples/peer_demo) example for how two peers can run concurrently on an **ESP32-S3** without external memory.

---

## 📦 Dependencies

### Libraries

- **libSRTP** – Secure RTP (bundled)
- **mbedTLS** – Required for DTLS (bundled with ESP-IDF)

### ESP-IDF Settings

Ensure these config options are enabled:

```ini
CONFIG_MBEDTLS_SSL_PROTO_DTLS=y
CONFIG_MBEDTLS_SSL_DTLS_SRTP=y
For IDFv6.0 need turn on following option also
CONFIG_MBEDTLS_X509_CREATE_C=y
```

---

## 🔀 PeerConnection State Machine

```mermaid
stateDiagram-v2
direction LR
[*] --> NEW_CONNECTION
NEW_CONNECTION --> PAIRING
PAIRING --> PAIRED
PAIRED --> CONNECTING
CONNECTING --> CONNECTED
CONNECTED --> DATA_CHANNEL_CONNECTED
DATA_CHANNEL_CONNECTED --> DATA_CHANNEL_OPENED
DATA_CHANNEL_OPENED --> DISCONNECTED
DISCONNECTED --> CLOSED
CLOSED --> [*]
```

---

## 🧠 Performance Tips

- **Tune Buffer Sizes**: Trade latency for memory based on use case
- **Adjust Timeouts**: Adapt to high-latency or lossy networks
- **Use Dedicated Task**: Run `esp_peer_main_loop()` in its own thread
- **Profile Resource Usage**: Monitor heap and stack for optimization

---

## 📬 Contact & Support

This component is part of the [esp-webrtc-solution](https://github.com/espressif/esp-webrtc-solution), offering complete WebRTC capabilities including media stream (esp_capture, av_render), varieties  of signaling (OpenAI, WHIP, AppRTC, etc.) and many practical examples.

🔧 Found a bug? Have a suggestion?\
Open an issue on GitHub: [esp-webrtc-solution/issues](https://github.com/espressif/esp-webrtc-solution/issues)

We’re here to help!

---
