# Component Manager Fix Instructions

## Issue
Some components (`av_render`, `media_lib_os`) are not available in the Component Manager registry.

## Solution Options

### Option 1: Use LiveKit's nanopb (Recommended)
If you want to match LiveKit's setup exactly, use their nanopb component:

```yaml
# In idf_component.yml files, use:
livekit/nanopb: "^0.4.9"
```

### Option 2: Use Standard nanopb
Use the standard nanopb from the registry:

```yaml
# In idf_component.yml files, use:
nikas-belogolov/nanopb: "*"
```

### Option 3: Handle av_render and media_lib_os

These components might be:
1. Part of ESP-IDF itself (check your ESP-IDF installation)
2. Need to be added manually from LiveKit's repository
3. Not needed for initial build (can be added later)

For now, they're commented out in `CMakeLists.txt`. Uncomment when you have them available.

## Next Steps

1. Clean build directory:
```bash
cd examples/minimal
rm -rf build managed_components
```

2. Try building with updated component names:
```bash
idf.py set-target esp32s3
idf.py build
```

3. If `nanopb` still fails, try adding it manually:
```bash
idf.py add-dependency "nikas-belogolov/nanopb"
# or
idf.py add-dependency "livekit/nanopb^0.4.9"
```

4. For `av_render` and `media_lib_os`, check if they're in your ESP-IDF installation:
```bash
find $IDF_PATH -name "*av_render*" -o -name "*media_lib*"
```

If found, they're part of ESP-IDF and don't need to be in `idf_component.yml`.
