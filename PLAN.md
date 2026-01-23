# Stream Video ESP32 SDK - Project Plan

## Overview
This project ports the Stream Video Android SDK's signaling and SFU connection logic to ESP32-S3, while maintaining a structure and library stack similar to LiveKit's ESP32 SDK.

**Design Principles:**
- **Simplicity First**: Minimal code, embedded-friendly design
- **No Unnecessary Synchronization**: Only use synchronization where absolutely required
- **Avoid Bad Patterns**: Skip singleton patterns and other unnecessary abstractions from Stream SDK
- **Minimum Viable Version**: Get first version working with minimal lines of code
- **Resource Efficient**: Optimize for ESP32-S3's limited CPU and memory

**Key Simplifications:**
- Use LiveKit's libraries directly (esp_peer, esp_capture, av_render, nanopb, media_lib_os)
- No custom media pipeline (use esp_capture and av_render directly)
- No singleton pattern (pass handles explicitly)
- Minimal synchronization (use event loop for thread safety)
- Basic proto methods only (skip advanced/optional features)
- Keep code minimal but organized into proper modules

## Project Structure

```
stream-video-esp32/
├── components/
│   ├── stream-video/              # Main SDK component
│   │   ├── include/
│   │   │   └── stream_video.h     # Main public API
│   │   ├── src/
│   │   │   ├── room.c             # Room management
│   │   │   ├── signaling/        # Stream SDK signaling logic
│   │   │   │   ├── websocket_client.c
│   │   │   │   ├── signaling_protocol.c
│   │   │   │   └── message_handler.c
│   │   │   ├── sfu/              # Stream SDK SFU connection logic
│   │   │   │   ├── sfu_client.c
│   │   │   │   ├── sfu_protocol.c
│   │   │   │   └── connection_manager.c
│   │   │   ├── webrtc/           # WebRTC integration
│   │   │   │   ├── peer_connection.c
│   │   │   │   ├── media_track.c
│   │   │   │   └── sdp_handler.c
│   │   │   ├── protobuf/         # Protobuf message handling
│   │   │   │   ├── message_serializer.c
│   │   │   │   └── message_deserializer.c
│   │   │   └── utils/
│   │   │       ├── logger.c
│   │   │       └── error_handler.c
│   │   ├── proto/                # Protocol buffer definitions
│   │   │   └── stream_video.proto
│   │   └── CMakeLists.txt
│   └── stream-video-protobuf/    # Generated protobuf code
│       ├── CMakeLists.txt
│       └── generated/            # Auto-generated from .proto files
├── examples/
│   ├── minimal/                  # Basic audio/video example
│   │   ├── main/
│   │   │   └── main.c
│   │   └── CMakeLists.txt
│   ├── minimal_video/            # Video publishing example
│   └── voice_agent/              # AI agent interaction example
├── docs/                         # Documentation
│   ├── api_reference.md
│   └── getting_started.md
├── .github/
│   └── workflows/                # CI/CD
├── CMakeLists.txt                # Root CMakeLists
├── LICENSE.txt
├── README.md
└── idf_component.yml             # Component registry manifest
```

## Key Components

### 1. Core SDK Component (`components/stream-video/`)

#### 1.1 Signaling Module (`src/signaling/`)
- **Purpose**: Port Stream Android SDK's signaling logic
- **Key Files**:
  - `websocket_client.c`: WebSocket connection management for signaling
  - `signaling_protocol.c`: Stream-specific signaling protocol implementation
  - `message_handler.c`: Handle incoming/outgoing signaling messages
- **Dependencies**: 
  - ESP-IDF WebSocket client
  - nanopb for message serialization
- **Porting Notes**: 
  - Convert Kotlin/Java async code to C with ESP-IDF event loops
  - Adapt Stream's signaling message format to C structures
  - Maintain Stream's authentication and token handling
  - **No unnecessary synchronization** - use event loop for thread safety
  - **Avoid singleton pattern** - pass context explicitly

#### 1.2 SFU Connection Module (`src/sfu/`)
- **Purpose**: Port Stream Android SDK's SFU connection logic
- **Key Files**:
  - `sfu_client.c`: SFU server connection management
  - `sfu_protocol.c`: Stream-specific SFU protocol implementation
  - `connection_manager.c`: Manage SFU connections and reconnection logic
- **Dependencies**:
  - esp_peer (~1.2.7) for WebRTC
  - nanopb for protobuf
- **Porting Notes**:
  - Port Stream's SFU negotiation logic
  - Adapt Stream's participant management
  - Maintain Stream's media routing logic
  - **No unnecessary synchronization** - single-threaded event loop
  - **Avoid singleton pattern** - pass context explicitly

#### 1.3 WebRTC Module (`src/webrtc/`)
- **Purpose**: WebRTC integration (similar to LiveKit's approach)
- **Key Files**:
  - `peer_connection.c`: WebRTC PeerConnection wrapper
  - `media_track.c`: Audio/Video track management
  - `sdp_handler.c`: SDP offer/answer handling
- **Dependencies**:
  - `esp_peer` (~1.2.7): Espressif WebRTC component
- **Notes**: 
  - Use esp_peer directly (same as LiveKit)
  - Integrate with Stream's signaling for SDP exchange
  - Let esp_peer handle SDP, ICE, DTLS internally

#### 1.4 Protobuf Module (`src/protobuf/`)
- **Purpose**: Protocol buffer message handling (basic methods only)
- **Key Files**:
  - `message_serializer.c`: Serialize messages to protobuf
  - `message_deserializer.c`: Deserialize protobuf messages
- **Dependencies**:
  - `nanopb`: Protocol Buffer implementation for embedded systems
- **Notes**:
  - Extract only basic/essential .proto files from Stream Android SDK
  - Focus on signaling and SFU connection messages only
  - Skip advanced/optional proto methods that aren't needed for core functionality
  - Generate C code from selected .proto files using nanopb

#### 1.5 Media Pipeline (Use LiveKit's approach directly)
- **Purpose**: Audio/Video capture and rendering (use existing components)
- **No Custom Code**: Use esp_capture and av_render directly
- **Dependencies**:
  - `esp_capture`: Media capture component (same as LiveKit)
  - `av_render`: Media rendering component (same as LiveKit)
  - `media_lib_os`: Media library OS abstraction (same as LiveKit)
- **Notes**: 
  - **No custom media pipeline code** - use esp_capture and av_render directly
  - Application provides capturer/renderer handles (same as LiveKit pattern)
  - Integrate with Stream's media routing via WebRTC tracks

### 2. Protocol Buffer Definitions (`components/stream-video/proto/`)
- Extract only basic/essential `.proto` files from Stream Android SDK
- Focus on core signaling and SFU connection messages only
- Skip advanced methods and optional features
- Generate C code using nanopb compiler for selected proto files only

### 3. Examples (`examples/`)
- **minimal**: Basic connection with audio
- **minimal_video**: Audio + video publishing
- **voice_agent**: AI agent interaction (similar to LiveKit's example)

## Dependencies

### Core Dependencies (exact same as LiveKit)
1. **ESP-IDF v5.4+**: Development framework
2. **WebRTC Library**: 
   - `esp_peer` (~1.2.7): Espressif WebRTC component
   - Supports ICE, DTLS, SCTP, RTP/SRTP
3. **Protocol Buffers**: 
   - `nanopb`: Protocol Buffer implementation for embedded systems
4. **Media Capture**: 
   - `esp_capture`: Media capture component
5. **Media Rendering**: 
   - `av_render`: Media rendering component
6. **OS Abstraction**: 
   - `media_lib_os`: Media library OS abstraction
7. **Network Stack**:
   - ESP-IDF HTTP/WebSocket client
   - TLS/SSL support (mbedTLS)

### Stream-Specific Dependencies
1. **Stream Protocol Definitions**: Extract from Android SDK
2. **Stream Authentication**: Token-based auth system
3. **Stream SFU Protocol**: SFU connection protocol

## Implementation Phases

### Phase 1: Project Setup & Structure
- [ ] Create directory structure
- [ ] Set up root CMakeLists.txt
- [ ] Create component CMakeLists.txt files
- [ ] Set up idf_component.yml
- [ ] Create basic README and documentation structure

### Phase 2: Protobuf Integration
- [ ] Identify and extract only basic/essential .proto files from Stream Android SDK
- [ ] Filter out advanced/optional proto methods
- [ ] Set up nanopb compilation pipeline
- [ ] Generate C code from selected .proto files using nanopb
- [ ] Create protobuf serialization/deserialization utilities for basic messages

### Phase 3: Signaling Module
- [ ] Port WebSocket client from Stream SDK
- [ ] Implement Stream signaling protocol
- [ ] Port message handlers
- [ ] Implement authentication/token handling
- [ ] Add reconnection logic
- [ ] **No unnecessary synchronization** - use event loop
- [ ] **Avoid singleton pattern** - pass context explicitly

### Phase 4: SFU Connection Module
- [ ] Port SFU client connection logic
- [ ] Implement Stream SFU protocol
- [ ] Port connection manager
- [ ] Implement participant management
- [ ] Add media routing logic
- [ ] **No unnecessary synchronization** - use event loop
- [ ] **Avoid singleton pattern** - pass context explicitly

### Phase 5: WebRTC Integration
- [ ] Integrate esp_peer (~1.2.7) component
- [ ] Create PeerConnection wrapper
- [ ] Implement SDP handling
- [ ] Integrate with Stream signaling for SDP exchange
- [ ] Implement media track management

### Phase 6: Media Integration (No Custom Pipeline)
- [ ] Integrate esp_capture component (use directly, no wrapper)
- [ ] Integrate av_render component (use directly, no wrapper)
- [ ] Integrate media_lib_os component
- [ ] Connect esp_capture to WebRTC tracks
- [ ] Connect av_render to WebRTC tracks
- [ ] **No custom media pipeline code** - use components directly

### Phase 7: Room API (Public Interface)
- [ ] Design public API (similar to Stream Android SDK's API style)
- [ ] Study Stream Android SDK's API patterns (StreamVideo, StreamVideoClient, Room, etc.)
- [ ] **Avoid singleton pattern** - use explicit handles
- [ ] Implement room creation/management following Stream's patterns
- [ ] Implement track publishing/subscribing with Stream's API style
- [ ] Add event callbacks matching Stream's callback patterns
- [ ] Create error handling system consistent with Stream SDK

### Phase 8: Examples
- [ ] Create minimal example
- [ ] Create minimal_video example
- [ ] Create voice_agent example
- [ ] Add example documentation

### Phase 9: Documentation & Testing
- [ ] Write API documentation
- [ ] Create getting started guide
- [ ] Add code comments
- [ ] Create unit tests (if applicable)
- [ ] Performance testing

## Key Design Decisions

### 1. API Design
- **Style**: Follow Stream Android SDK's API style and patterns
- **Naming**: Use `stream_video_` prefix for all public APIs, but match Stream's naming conventions
- **Structure**: Mirror Stream's class hierarchy (StreamVideo, StreamVideoClient, Room, Participant, Track, etc.)
- **Error Handling**: Follow Stream's error handling patterns (exceptions in Android → error codes in C)
- **Callbacks**: Use function pointers for events, matching Stream's callback/listener patterns
- **Examples**: 
  - Stream Android: `StreamVideo.instance().connectUser(user, token)` → **NO SINGLETON**
  - ESP32 C: `stream_video_connect_user(client_handle, user, token)` (explicit handle)
  - Stream Android: `room.publishVideoTrack(track)`
  - ESP32 C: `stream_video_room_publish_video_track(room_handle, track_handle)`

### 1.1 Stream Android SDK API Patterns (Reference)
- **StreamVideo**: **NO SINGLETON** - use explicit context/handle
  - `StreamVideo.instance()` → **SKIP** (bad pattern)
  - `connectUser()` → `stream_video_connect_user(handle, user, token)`
- **StreamVideoClient**: Main client for video operations
  - Methods map to `stream_video_client_*` functions
  - **Pass handle explicitly** - no singleton
- **Room**: Room management and operations
  - `room.join()` → `stream_video_room_join(room_handle)`
  - `room.publishVideoTrack()` → `stream_video_room_publish_video_track(room_handle, track_handle)`
  - `room.subscribe()` → `stream_video_room_subscribe(room_handle)`
- **Listeners/Callbacks**: Convert to function pointers
  - `RoomListener` → `stream_video_room_listener_t` struct with callbacks
  - `ParticipantListener` → `stream_video_participant_listener_t` struct
- **Error Handling**: 
  - Stream uses exceptions → ESP32 uses error codes
  - `StreamVideoException` → `stream_video_error_t` enum

### 2. Memory Management
- **Allocation**: Use ESP-IDF memory management
- **Ownership**: Clear ownership semantics for handles
- **Lifecycle**: Explicit create/destroy functions
- **Minimal Allocations**: Prefer stack allocation where possible

### 3. Threading Model
- **Event Loop**: Use ESP-IDF event loops (single-threaded where possible)
- **Threading**: Minimize thread usage - use single event loop if possible
- **Synchronization**: **AVOID unless absolutely required**
  - Use event loop for thread safety where possible
  - Only use mutexes/semaphores when truly necessary
  - Prefer lock-free patterns

### 4. Network Stack
- **WebSocket**: ESP-IDF WebSocket client for signaling
- **TLS**: ESP-IDF mbedTLS for secure connections
- **Reconnection**: Automatic reconnection with exponential backoff

### 5. Media Handling
- **Codecs**: Same as LiveKit (Opus, H.264)
- **Hardware**: Use hardware acceleration when available
- **Pipeline**: Use LiveKit's capturer/renderer pattern directly
  - Application provides `esp_capture_handle_t` for capture
  - Application provides `av_render_handle_t` for rendering
  - **No custom media pipeline code** - use components directly

## Porting Strategy from Stream Android SDK

### API Style Porting
1. **Study Stream Android SDK API**:
   - Analyze StreamVideo, StreamVideoClient classes
   - Study Room, Participant, Track APIs
   - Understand callback/listener patterns
   - Review error handling mechanisms
   - **Identify and avoid bad patterns** (singletons, unnecessary abstractions)
2. **Map to C API**:
   - Convert classes to opaque handles (e.g., `stream_video_handle_t`)
   - Convert methods to functions (e.g., `stream_video_room_publish_track()`)
   - Convert listeners to function pointer callbacks
   - Maintain Stream's API flow and patterns
   - **Pass handles explicitly** - no singleton pattern
   - **Keep API minimal** - only essential methods

### Signaling Porting
1. **Identify Core Classes**: 
   - WebSocket client implementation
   - Signaling message handlers
   - Authentication logic
2. **Convert to C**:
   - Replace Kotlin coroutines with ESP-IDF event loops
   - Convert classes to structs + functions
   - Replace async/await with callbacks
   - Maintain Stream's API patterns
   - **Avoid singleton pattern** - pass context explicitly
   - **No unnecessary synchronization** - use event loop
3. **Adapt Network Layer**:
   - Replace OkHttp with ESP-IDF HTTP client
   - Replace Kotlin serialization with nanopb (basic methods only)

### SFU Connection Porting
1. **Identify Core Classes**:
   - SFU client implementation
   - Connection manager
   - Participant management
2. **Convert to C**:
   - Similar conversion strategy as signaling
   - Maintain Stream's connection state machine
   - Preserve Stream's API patterns
   - **Avoid singleton pattern** - pass context explicitly
   - **No unnecessary synchronization** - use event loop
3. **Integrate with WebRTC**:
   - Use esp_peer (~1.2.7) directly
   - Maintain Stream's SDP negotiation flow

### Protobuf Porting
1. **Identify Basic Proto Files**:
   - Extract only essential .proto files for signaling
   - Extract only essential .proto files for SFU connection
   - Skip advanced/optional proto definitions
2. **Generate C Code**:
   - Use nanopb compiler to generate C code
   - Only include basic message types needed for core functionality

## Testing Strategy

1. **Unit Tests**: Test individual modules
2. **Integration Tests**: Test signaling + SFU + WebRTC together
3. **Hardware Tests**: Test on ESP32-S3 hardware
4. **Network Tests**: Test under various network conditions
5. **Performance Tests**: Measure CPU, memory, bandwidth usage

## Documentation Requirements

1. **API Reference**: Complete API documentation
2. **Getting Started**: Step-by-step setup guide
3. **Examples**: Well-documented example code
4. **Architecture**: High-level architecture documentation
5. **Porting Guide**: Guide for porting from Stream Android SDK

## Success Criteria

- [ ] Successfully connect to Stream SFU
- [ ] Publish audio/video tracks
- [ ] Subscribe to remote tracks
- [ ] Handle reconnections
- [ ] Support Stream's authentication
- [ ] Maintain API style similar to Stream Android SDK
- [ ] Work on ESP32-S3 hardware
- [ ] Provide working examples

## Next Steps

1. Review and approve this plan
2. Set up project structure
3. Begin Phase 1 implementation
4. Extract and analyze Stream Android SDK code
5. Set up development environment

