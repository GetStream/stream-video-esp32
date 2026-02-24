# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.1.0] - 2025-02-23

### Added

- Initial open-source release of Stream Video ESP32 SDK.
- Support for ESP32-S3 and ESP32-P4.
- Auth flow: request token from backend, create user, connect to coordinator.
- Coordinator WebSocket: connect, join call, receive SFU URL.
- SFU WebSocket: connect and signaling for WebRTC.
- Audio/video publishing (H.264 video, Opus audio) with Stream SFU.
- Minimal example: full flow from WiFi and auth to join call and publish.
- Documentation: auth flow, coordinator/SFU flow, API reference (incomplete), known limitations.

### Known limitations

- Networking and reconnects: WiFi retry has no backoff; SFU uses health-based restart only.
- API reference is incomplete.
- Auth base URL is hardcoded (documented in [docs/auth_flow.md](docs/auth_flow.md)).
