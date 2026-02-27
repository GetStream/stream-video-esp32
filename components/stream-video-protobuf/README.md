# Stream Video Protobuf Component

This component handles Protocol Buffer message definitions and code generation for the Stream Video ESP32 SDK.

## Proto Files

The proto files are located in `../stream-video/proto/`:

- `signaling.proto` - Signaling messages (connect, join room, publish/subscribe tracks)
- `sfu.proto` - SFU connection and WebRTC signaling messages (SDP, ICE candidates)
- `models.proto` - Shared SFU model types (tracks, errors)

## Code Generation

**Pre-generated sources are committed** in `generated/`, so a normal clone-and-build works without any extra tools. You only need to regenerate if you change the `.proto` files.

To regenerate (e.g. after editing `.proto` files), install `protoc` and `pip install nanopb`, then either run CMake (it will generate if the files are missing) or run manually:

### Manual Generation

```bash
# Install protoc if not already installed
# Install nanopb component

# Generate C code
protoc --plugin=protoc-gen-nanopb=<nanopb_path>/generator/protoc-gen-nanopb \
       --nanopb_out=generated \
       --proto_path=../stream-video/proto \
       ../stream-video/proto/signaling.proto \
       ../stream-video/proto/sfu.proto \
       ../stream-video/proto/models.proto \
       ../stream-video/proto/sfu_signal.proto
```

## Generated Files

Generated files will be placed in the `generated/` directory:
- `signaling.pb.c` / `signaling.pb.h`
- `sfu.pb.c` / `sfu.pb.h`
- `sfu_signal.pb.c` / `sfu_signal.pb.h`
- `models.pb.c` / `models.pb.h`

## Usage

The generated nanopb structures can be used with the serialization/deserialization utilities in `stream-video/src/protobuf/`.

