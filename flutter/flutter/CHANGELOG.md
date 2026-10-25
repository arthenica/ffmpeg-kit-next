## development
- Adds Web platform support (`wasm32`), built on the `ffmpeg-kit-next-web` WebAssembly package. Support both JavaScript and WebAssembly Flutter web builds
- Adds the web-only `FFmpegKitVirtualFile`, `FFmpegKitVirtualDirectory`, `FFmpegKitLocalFile` and `FFmpegKitWebConfig` classes
- Requires Flutter 3.41 / Dart 3.11 or later on all platforms
- Requires `ffmpeg_kit_next_flutter_platform_interface` 0.4.0, which adds `FFmpegKitPlatform.events`. Custom `FFmpegKitPlatform` implementations and mocks must override it
- Ships the native libraries' licence texts with Linux, Windows and web builds: an `ffmpeg-kit-next-licenses` folder next to the Linux and Windows executables, and `LICENSE`, `SOURCE` and `licenses/` next to the web bundle

## 9.0.2
- Bugfix release based on native 9.0.2 (FFmpeg 9.0.2)
- Deprecates named pipes in favour of FFmpegKit input/output buffers and streams
- Fixes [#85](https://github.com/arthenica/ffmpeg-kit-next/issues/85)

## 9.0.1
- Bugfix release based on native 9.0.1 (FFmpeg 9.0.2)
- Introduces Windows platform support (`x86_64`, `arm64`)
- Adds `FFmpegKitConfig.shutdown(int waitTimeout)` method
- Requires `ffmpeg_kit_next_flutter_platform_interface` 0.3.1

## 9.0.0
- Feature release based on native 9.0.0 (FFmpeg 9.0.1)
- Fixes issues [#22](https://github.com/arthenica/ffmpeg-kit-next/issues/22) and [#36](https://github.com/arthenica/ffmpeg-kit-next/issues/36)

## 8.1.1
- Documents iPadOS as a supported platform
- Adds Linux platform support (`x86_64`, `arm64`)

## 8.1.0
- Feature release based on native 8.1.0 (FFmpeg 8.1.2)

## 7.1.0
- Feature release based on native 7.1.0 (FFmpeg 7.1.5)
- Fixes [#1](https://github.com/arthenica/ffmpeg-kit-next/issues/1)

## 6.1.1
- Feature release based on native v6.1.1 (FFmpeg 6.1.6)

## 6.1.0
- Feature release based on native v6.1.0 (FFmpeg 6.1.2)
- Adds Flutter support for the native "ffkit" protocol classes:
  `FFmpegKitInputBuffer`, `FFmpegKitOutputBuffer`, `FFmpegKitStreamInput` and
  `FFmpegKitStreamOutput` for in-memory and streaming input/output
- Requires `ffmpeg_kit_next_flutter_platform_interface` 0.3.0
