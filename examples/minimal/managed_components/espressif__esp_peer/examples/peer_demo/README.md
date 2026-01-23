# ESP Peer WebRTC Demo

This example demonstrates real-time communication using the **ESP Peer WebRTC** library. It simulates a chat conversation between a cat (🐱) and a sheep (🐑), featuring both audio streaming and messaging over WebRTC data channels.

---

## 🧩 Overview

This demo showcases core WebRTC capabilities on ESP32, including:

- **Peer-to-Peer Communication**: Direct media and message transmission between two ESP-based peers.
- **Audio Streaming**: Real-time audio using the G.711A codec.
- **Data Channels**: Bidirectional messaging with low-latency delivery.
- **SDP Negotiation**: Simulated offer/answer exchange between peers.

---

## ✨ Features

### 🎵 Audio Communication

- Real-time audio streaming between local peers.
- Uses G.711A codec for voice transmission.
- Directional audio support (send, receive, or both).
- PTS-based (presentation timestamp) verification of audio frames.

### 💬 Data Channel Messaging

- Simulated Q&A interaction between the cat 🐱 and the sheep 🐑.
- Auto-generated replies based on simple keyword matching.
- Reliable message exchange over data channels.

### 🌐 Network Setup

- Operates in **SoftAP mode** for peer-to-peer connection.
- No external router or internet required for demo operation.

### ⚙️ Configurable Options

- Agent timeout configuration
- RTP jitter buffer parameters
- Data channel caching
- RTP rolling buffer settings for outgoing packets

---

## 🚀 Building and Running

### Prerequisites

- [ESP-IDF](https://github.com/espressif/esp-idf) v5.0 or later
- Properly set up ESP-IDF development environment
- Component dependencies installed via `idf.py add-dependency` (or `idf_component.yml`)

### Steps to Build and Flash

```bash
# Set your target chip (e.g., ESP32-S3)
idf.py set-target esp32s3

# Build and flash the demo
idf.py -p <SerialDevice> flash monitor
```

---

## 🧪 Comparison with Browser

To compare the ESP implementation with browser-based WebRTC:

- A simple HTML file [`animal_chat.html`](main/animal_chat.html) is included.
- It mimics the behavior of the ESP demo for easier testing and API alignment.

---

## 📌 Notes

- This demo runs both peers locally on the same ESP device for convenience.
- In real deployments:
  - Configure valid **STUN/TURN servers** for NAT traversal.
  - Implement a **signaling mechanism** (e.g., MQTT, WebSocket, HTTP) to exchange SDP and ICE candidates across devices.
