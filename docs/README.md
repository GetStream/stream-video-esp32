# Stream Video ESP32 – Documentation

Start with the [main README](../README.md) and the [minimal example README](../examples/minimal/README.md) for building and running.

## Doc index

| Document | Description |
|----------|--------------|
| [getting_started.md](getting_started.md) | Getting started, installation, app flow (auth on app side, then join), minimal example |
| [api_reference.md](api_reference.md) | App-facing API: init, set_capture_provider, join_call, leave_call, types, default capture |
| [auth_flow.md](auth_flow.md) | Authentication: app fetches token, SDK accepts auth_data; token service endpoint, join params |
| [example_configuration.md](example_configuration.md) | Example: token service URL, call type/call ID in main.c; WiFi in menuconfig |
| [sdk_configuration.md](sdk_configuration.md) | All SDK Kconfig options (Stream Video SDK menu): core, ICE, board, video, audio, debug |
| [token_and_environment.md](token_and_environment.md) | Token and environment handling |
| [signaling_architecture.md](signaling_architecture.md) | Signaling architecture |
| [stream_url_guide.md](stream_url_guide.md) | Stream URL usage |
