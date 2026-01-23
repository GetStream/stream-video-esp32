# Stream Video Protobuf Component

This component handles Protocol Buffer message definitions and code generation for the Stream Video ESP32 SDK.

## Proto Files

The proto files are located in `../stream-video/proto/`:

- `signaling.proto` - Signaling messages (connect, join room, publish/subscribe tracks)
- `sfu.proto` - SFU connection and WebRTC signaling messages (SDP, ICE candidates)

## Code Generation

The proto files need to be compiled to C code using nanopb. This can be done:

1. **Manually** using protoc and nanopb generator
2. **Via build script** (to be implemented)
3. **Via CMake custom command** (to be implemented)

### Manual Generation

```bash
# Install protoc if not already installed
# Install nanopb component

# Generate C code
protoc --plugin=protoc-gen-nanopb=<nanopb_path>/generator/protoc-gen-nanopb \
       --nanopb_out=generated \
       --proto_path=../stream-video/proto \
       ../stream-video/proto/signaling.proto \
       ../stream-video/proto/sfu.proto
```

## Generated Files

Generated files will be placed in the `generated/` directory:
- `signaling.pb.c` / `signaling.pb.h`
- `sfu.pb.c` / `sfu.pb.h`

## Usage

The generated nanopb structures can be used with the serialization/deserialization utilities in `stream-video/src/protobuf/`.

