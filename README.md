# FFmpegKitNext

<img src="https://raw.githubusercontent.com/arthenica/ffmpeg-kit-next/main/docs/assets/ffmpeg-kit-next-icon-v1.png" width="240">

`FFmpegKitNext` is the official continuation of `FFmpegKit`.

It builds on the foundations of `FFmpegKit` while improving the build infrastructure to reduce build failures, simplify maintenance, and support future platform updates.

### 0. Features How is FFmpegKitNext different from FFmpegKit?

`FFmpegKitNext` does not publish ready-to-use packages to `Maven Central`, `CocoaPods`, `pub.dev`, or `npm` as `FFmpegKit` did.

To use it, build it locally for your target platform via [Nix](https://nixos.org/), then integrate the generated artifacts into your application.

### 1. Features
- Nix-based scripts to build FFmpeg native libraries
- `FFmpegKitNext` wrapper library to run `FFmpeg`/`FFprobe` commands in applications
- Supports native platforms: Android, iOS, iPadOS, Linux, macOS, tvOS and visionOS
- Supports hybrid platforms: Flutter, React Native
- Based on FFmpeg `v6.1.x` or later with optional system and external libraries
- Licensed under `LGPL 3.0` by default, `GPL v3.0` if GPL licensed libraries are enabled

### 2. Android

See [Android](android) to learn more about `FFmpegKitNext` for `Android`.

### 3. iOS, iPadOS, macOS, tvOS, visionOS

See [Apple](apple) to use `FFmpegKitNext` on `Apple` platforms (`iOS`, `iPadOS`, `macOS`, `tvOS`, `visionOS`).

### 4. Flutter

See [Flutter](flutter/flutter) to learn more about `FFmpegKitNext` for `Flutter`.

### 5. Linux

See [Linux](linux) to learn more about `FFmpegKitNext` for `Linux`.

### 6. React Native

See [React Native](react-native) to learn more about `FFmpegKitNext` for `React Native`.

### 7. Build Scripts

`FFmpegKitNext` build scripts depend on [Nix](https://nixos.org/). Nix provides the build environment, inputs and toolchains required to produce the same binaries across machines.

Use `nix-android.sh`, `nix-ios.sh` (both for iOS and iPadOS), `nix-linux.sh`, `nix-macos.sh`, `nix-tvos.sh` and `nix-visionos.sh` to build `FFmpegKitNext` locally for each native platform.

All scripts support additional options to enable optional libraries and disable platform architectures. See [Building](https://github.com/arthenica/ffmpeg-kit-next/wiki/Building) wiki page for the details.

### 8. FFmpegKitNext Library

`FFmpegKitNext` is a wrapper library that allows you to easily run `FFmpeg`/`FFprobe` commands in applications. It
provides additional features on top of `FFmpeg` to enable platform specific resources, control how commands are 
executed and how the results are handled.

`Android` library of `FFmpegKitNext` has a `Kotlin` API, `Apple` libraries (`iOS`, `iPadOS`, `macOS`, `tvOS`, `visionOS`) have an
`Objective-C` API, `Flutter` library comes with a `Dart` API, `Linux` library has a `C++` API and `React Native`
library provides a `JavaScript` API with `Typescript` definitions, which are identical in terms of features and
capabilities.

### 9. External Libraries

The table below lists the optional external libraries that can be enabled in local `FFmpegKitNext` builds.

Some parts of `FFmpeg` and some external libraries are licensed under the `GPL`. GPL libraries are included only when a GPL-enabled build variant is selected.

<table>
<thead>
<tr>
<th align="center"></th>
<th align="center"><sup>library name</sup></th>
</tr>
</thead>
<tbody>
<tr>
<td align="center"><sup>external libraries</sup></td>
<td align="center"><sup>chromaprint</sup><br><sup>dav1d</sup><br><sup>fontconfig</sup><br><sup>freetype</sup><br><sup>fribidi</sup><br><sup>gmp</sup><br><sup>gnutls</sup><br><sup>harfbuzz</sup><br><sup>kvazaar</sup><br><sup>lame</sup><br><sup>libaom</sup><br><sup>libass</sup><br><sup>libiconv</sup><br><sup>libilbc</sup><br><sup>libjxl</sup><br><sup>liblc3</sup><br><sup>libsvtav1</sup><br><sup>libtheora</sup><br><sup>libvorbis</sup><br><sup>libvpx</sup><br><sup>libwebp</sup><br><sup>libxml2</sup><br><sup>opencore-amr</sup><br><sup>openh264</sup><br><sup>openssl</sup><br><sup>opus</sup><br><sup>rubberband</sup><br><sup>sdl</sup><br><sup>shine</sup><br><sup>snappy</sup><br><sup>soxr</sup><br><sup>speex</sup><br><sup>srt</sup><br><sup>tesseract</sup><br><sup>twolame</sup><br><sup>vid.stab</sup><br><sup>vo-amrwbenc</sup><br><sup>vvenc</sup><br><sup>x264</sup><br><sup>x265</sup><br><sup>xvidcore</sup><br><sup>zimg</sup></td>
</tr>
<tr>
<td align="center"><sup>android system libraries</sup></td>
<td align="center"><sup>zlib</sup><br><sup>MediaCodec</sup></td>
</tr>
<tr>
<td align="center"><sup>ios/ipados system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>AVFoundation</sup><br><sup>iconv</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>macos system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>AVFoundation</sup><br><sup>Core Image</sup><br><sup>iconv</sup><br><sup>OpenCL</sup><br><sup>OpenGL</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>tvos system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>iconv</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
</tbody>
</table>

 - `AVFoundation` is not available on `tvOS`

### 10. Versions

`FFmpegKitNext` binaries generated use the same major and minor version numbers as the upstream `FFmpeg`<sup>1</sup> project. The
third and last number in the version string, if exists, is specific to `FFmpegKitNext`. It shows different releases from
the same `FFmpeg` release branch.

|    Platforms     |                                 FFmpegKit Version                                 | FFmpeg Version | Release Date |
|:----------------:|:---------------------------------------------------------------------------------:|:--------------:|:------------:|
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [8.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v8.1.0)    |      8.1.2       | Jul 02, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [7.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v7.1.0)    |      7.1.5       | Jul 01, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [6.1.1](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v6.1.1)    |      6.1.6       | Jun 27, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [6.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v6.1.0)    |      6.1.2       | Jun 26, 2026 |


### 11. Documentation

A more detailed documentation is available under [Wiki](https://github.com/arthenica/ffmpeg-kit-next/wiki).

### 12. Test Applications

You can see how `FFmpegKitNext` is used inside an application by running test applications created under
[FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.

All applications are identical and supports command execution, video encoding, accessing https urls, encoding audio,
burning subtitles, video stabilisation, pipe operations, ffkit protocols and concurrent command execution.

### 13. License

`FFmpegKitNext` library alone is licensed under the `LGPL v3.0`.

`FFmpegKitNext` bundles (`.aar` archives, `frameworks`, `xcframeworks`), which include both `FFmpegKitNext` and `FFmpeg`
libraries, are also licensed under the `LGPL v3.0`. However, if the source code is built using the optional
`--enable-gpl` flag, then `FFmpegKitNext` bundles become subject to the `GPL v3.0`. Because, `FFmpeg` is licensed
under the `GPL v3.0` in those bundles. And that makes the whole bundle effectively subject to the `GPL v3.0`.

`FFmpegKitNext` build scripts always configure `FFmpeg` with `--enable-version3` option. And never enable non-free
libraries. Thus, `FFmpeg` libraries created by `FFmpegKitNext` are licensed under the `LGPL v3.0` by default. Only when
`--enable-gpl` is provided they become subject to `GPL v3.0`.

Refer to [Licenses and Notices](https://github.com/arthenica/ffmpeg-kit-next/wiki/Licenses-and-Notices) to see the
licenses of all libraries. [Trademarks](https://github.com/arthenica/ffmpeg-kit-next/wiki/Trademarks) lists the
trademarks used in the `FFmpegKitNext` documentation.

### 14. Patents

It is not clearly explained in their documentation, but it is believed that `FFmpeg`, `kvazaar`, `x264` and `x265`
include algorithms which are subject to software patents. If you live in a country where software algorithms are
patentable then you'll probably need to pay royalty fees to patent holders. We are not lawyers though, so we recommend
that you seek legal advice first. See [FFmpeg Patent Mini-FAQ](https://ffmpeg.org/legal.html).

`openh264` clearly states that it uses patented algorithms. Therefore, if you build `ffmpeg-kit-next` with `openh264` and
distribute that library, then you are subject to pay MPEG LA licensing fees. Refer to
[OpenH264 FAQ](https://www.openh264.org/faq.html) page for the details.

### 15. Trademarks

<sup>1</sup> `FFmpeg` is a trademark of [Fabrice Bellard](http://www.bellard.org/). `FFmpegKitNext` and `FFmpegKit` are independent projects and not affiliated with the `FFmpeg` trademark holder.

### 16. See Also

- [FFmpeg API Documentation](https://ffmpeg.org/doxygen/4.0/index.html)
- [FFmpeg Wiki](https://trac.ffmpeg.org/wiki/WikiStart)
- [FFmpeg External Library Licenses](https://www.ffmpeg.org/doxygen/4.0/md_LICENSE.html)
