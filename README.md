# V Editor Simple

Simple UI, full-featured video editor built with C++ / Qt6 / FFmpeg.

## Supported Formats

| Codec | Decode | Encode |
|-------|--------|--------|
| H.264 (x264) | Yes | Planned |
| H.265 (HEVC) | Yes | Planned |
| AV1 | Yes | Planned |

| Container | Support |
|-----------|---------|
| MP4 | Yes |
| MKV | Yes |
| MOV | Yes |
| WebM | Yes |
| FLV | Yes |

## Platforms

- **Windows** (primary)
- **macOS** (planned)
- **Linux** (planned)

## Build

### Prerequisites

- CMake 3.20+
- Qt6 (Widgets, Multimedia, MultimediaWidgets)
- FFmpeg (libavformat, libavcodec, libavutil, libswscale, libswresample)
- pkg-config

### Windows (vcpkg)

```bash
vcpkg install qt6 ffmpeg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### macOS (Homebrew)

```bash
brew install qt@6 ffmpeg pkg-config
cmake -B build -S .
cmake --build build
```

### Linux (apt)

```bash
sudo apt install qt6-base-dev qt6-multimedia-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev
cmake -B build -S .
cmake --build build
```

## Roadmap

- [x] Phase 1: Video load, preview, timeline, basic cut
- [ ] Phase 2: Multi-track, transitions, text overlay
- [ ] Phase 3: Color grading, effects, keyframes
- [ ] Phase 4: Export, GPU acceleration, plugin system

## License

MIT
