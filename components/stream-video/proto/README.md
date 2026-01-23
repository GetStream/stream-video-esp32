# Stream Video Protocol Buffer Definitions

This directory contains Protocol Buffer (protobuf) message definitions for the Stream Video ESP32 SDK.

## Files

- `signaling.proto` - Basic signaling messages for Stream Video
  - Connection/disconnection
  - Room join/leave
  - Track publish/unpublish
  - Track subscribe/unsubscribe
  - Events (track published, participant joined, etc.)

- `sfu.proto` - SFU connection and WebRTC signaling messages
  - SDP offer/answer
  - ICE candidates
  - SFU connection
  - Media stream configuration

## Design Principles

- **Minimal**: Only essential messages for core functionality
- **Basic**: No advanced features or optional methods
- **Embedded-friendly**: Designed for ESP32's resource constraints
- **Stream-compatible**: Matches Stream's signaling protocol

## Message Categories

### Signaling Messages
- Connection management (ConnectRequest, ConnectResponse)
- Room operations (JoinRoomRequest, LeaveRoomRequest)
- Track publishing (PublishTrackRequest, UnpublishTrackRequest)
- Track subscription (SubscribeTrackRequest, UnsubscribeTrackRequest)
- Events (TrackPublishedEvent, ParticipantJoinedEvent, etc.)

### SFU Messages
- WebRTC signaling (SdpOffer, SdpAnswer, IceCandidate)
- SFU connection (SfuConnectRequest, SfuConnectResponse)
- Media configuration (TrackConfig, MediaStream)

## Future Enhancements

As the SDK develops, additional proto messages may be added, but we will maintain the principle of keeping only essential messages to minimize code size and memory usage.

