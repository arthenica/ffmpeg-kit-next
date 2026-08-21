# FFmpegKitNext

<img src="https://raw.githubusercontent.com/arthenica/ffmpeg-kit-next/main/docs/assets/ffmpeg-kit-next-icon-v5.png" width="240">

`FFmpegKitNext` is the official continuation of `FFmpegKit`.

It builds on the foundations of `FFmpegKit` while improving the build infrastructure to reduce build failures, simplify maintenance and support future platform updates.

### 0. Features How is FFmpegKitNext different from FFmpegKit?

`FFmpegKitNext` does not publish ready-to-use packages to `Maven Central`, `CocoaPods`, `pub.dev`, or `npm` as `FFmpegKit` did.

To use it, build it locally for your target platform via the [Nix package manager](https://github.com/arthenica/ffmpeg-kit-next/wiki/Nix) or the MSYS2 / MinGW-w64 toolchain on Windows, then integrate the generated artifacts into your application.

### 1. Features
- Nix package manager-based scripts to build FFmpeg native libraries for Android, Apple platforms, Linux and Web
- Native Windows build script based on MSYS2 / MinGW-w64
- `FFmpegKitNext` wrapper library to run `FFmpeg`/`FFprobe` commands in applications
- Supports native platforms: Android, iOS, iPadOS, Linux, macOS, tvOS, visionOS, Web and Windows
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

### 7. Web

See [Web](web) to learn more about `FFmpegKitNext` for `Web` (`WebAssembly`).

### 8. Windows

See [Windows](windows) to learn more about `FFmpegKitNext` for `Windows`.

### 9. Build Scripts

`FFmpegKitNext` uses the [Nix package manager](https://github.com/arthenica/ffmpeg-kit-next/wiki/Nix) as the default and recommended build environment. It provides pinned inputs, toolchains, and platform-specific build packages for supported hosts.

Install Nix on a supported host to build the project. Note that NixOS itself is not supported as a build host. See [Host Support](https://github.com/arthenica/ffmpeg-kit-next/wiki/Host-Support) before setting up a machine or CI image.

Use `nix-android.sh`, `nix-ios.sh` (for both iOS and iPadOS), `nix-linux.sh`, `nix-macos.sh`, `nix-tvos.sh`, `nix-visionos.sh` and `nix-web.sh` to build `FFmpegKitNext` locally for Nix-supported target platforms.

Building `FFmpegKitNext` without Nix is also supported. The non-Nix build workflow, together with script options for enabling optional libraries and disabling target architectures is explained in the [Building](https://github.com/arthenica/ffmpeg-kit-next/wiki/Building) wiki page. `Windows` is built without Nix using the `windows.sh` script from the project root.

### 10. FFmpegKitNext Library

`FFmpegKitNext` is a wrapper library that allows you to easily run `FFmpeg`/`FFprobe` commands in applications. It
provides additional features on top of `FFmpeg` to enable platform specific resources, control how commands are 
executed and how the results are handled.

`Android` library of `FFmpegKitNext` has a `Kotlin` API, `Apple` libraries (`iOS`, `iPadOS`, `macOS`, `tvOS`, `visionOS`) have an
`Objective-C` API, `Flutter` library comes with a `Dart` API, `Linux` has a `C++` API, `Windows` has `C++` and `C` APIs, `React Native` and `Web`
libraries provide a `JavaScript` API with `Typescript` definitions, which are identical in terms of features and
capabilities.

### 11. External Libraries

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
<td align="center"><sup>chromaprint</sup><br><sup>dav1d</sup><br><sup>fontconfig</sup><br><sup>freetype</sup><br><sup>fribidi</sup><br><sup>gmp</sup><br><sup>gnutls</sup><br><sup>harfbuzz</sup><br><sup>kvazaar</sup><br><sup>lame</sup><br><sup>libaom</sup><br><sup>libass</sup><br><sup>libiconv</sup><br><sup>libilbc</sup><br><sup>libjxl</sup><br><sup>liblc3</sup><br><sup>libsvtav1</sup><br><sup>libtheora</sup><br><sup>libvorbis</sup><br><sup>libvpx</sup><br><sup>libwebp</sup><br><sup>libxml2</sup><br><sup>opencore-amr</sup><br><sup>openh264</sup><br><sup>openssl</sup><br><sup>opus</sup><br><sup>rubberband</sup><br><sup>sdl</sup><br><sup>shine</sup><br><sup>snappy</sup><br><sup>soxr</sup><br><sup>speex</sup><br><sup>srt</sup><br><sup>tesseract</sup><br><sup>twolame</sup><br><sup>vid.stab</sup><br><sup>vo-amrwbenc</sup><br><sup>vvenc</sup><br><sup>x264</sup><br><sup>x265</sup><br><sup>xvidcore</sup><br><sup>zimg</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>android system libraries</sup></td>
<td align="center"><sup>zlib</sup><br><sup>MediaCodec</sup></td>
</tr>
<tr>
<td align="center"><sup>ios/ipados system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>AVFoundation</sup><br><sup>libiconv</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>linux system libraries</sup></td>
<td align="center"><sup>alsa</sup><br><sup>libiconv</sup><br><sup>OpenCL</sup><br><sup>vaapi</sup><br><sup>v4l2</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>macos system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>AVFoundation</sup><br><sup>Core Image</sup><br><sup>libiconv</sup><br><sup>OpenCL</sup><br><sup>OpenGL</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>tvos system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>libiconv</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>visionos system libraries</sup></td>
<td align="center"><sup>bzip2</sup><br><sup>AudioToolbox</sup><br><sup>libiconv</sup><br><sup>VideoToolbox</sup><br><sup>zlib</sup></td>
</tr>
<tr>
<td align="center"><sup>web system libraries</sup></td>
<td align="center"><sup>libiconv</sup><br><sup>zlib</sup></td>
</tr>
</tbody>
</table>

 - `AVFoundation` is not available on `tvOS` and `visionOS`

### 12. Versions

`FFmpegKitNext` binaries generated use the same major and minor version numbers as the upstream `FFmpeg`<sup>1</sup> project. The
third and last number in the version string, if exists, is specific to `FFmpegKitNext`. It shows different releases from
the same `FFmpeg` release branch.

|    Platforms     |                                 FFmpegKit Version                                 | FFmpeg Version | Release Date |
|:----------------:|:---------------------------------------------------------------------------------:|:--------------:|:------------:|
|  Windows      |   [8.1.1](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v8.1.1.windows)    |      8.1.2       | Aug 21, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS<br>visionOS<br>Web      |   [8.1.1](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v8.1.1)    |      8.1.2       | Jul 28, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [8.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v8.1.0)    |      8.1.2       | Jul 02, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [7.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v7.1.0)    |      7.1.5       | Jul 01, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [6.1.1](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v6.1.1)    |      6.1.6       | Jun 27, 2026 |
|  Android<br>Flutter<br>iOS / iPadOS<br>Linux<br>macOS<br>React Native<br>tvOS      |   [6.1.0](https://github.com/arthenica/ffmpeg-kit-next/releases/tag/v6.1.0)    |      6.1.2       | Jun 26, 2026 |


### 13. Documentation

A more detailed documentation is available under [Wiki](https://github.com/arthenica/ffmpeg-kit-next/wiki).

### 14. Test Applications

You can see how `FFmpegKitNext` is used inside an application by running test applications created under
[FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.

All applications are identical and supports command execution, video encoding, accessing https urls, encoding audio,
burning subtitles, video stabilisation, pipe operations, ffkit protocols and concurrent command execution.

### 15. License

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

### 16. Patents

`FFmpeg`, `kvazaar`, `x264` and `x265` may implement codec technologies that are covered by patents in some
 jurisdictions. Patent rules vary by country and this documentation is not legal advice. If you use or distribute
 `ffmpeg-kit-next` in a country where software or codec patents are enforceable, you should consult qualified legal
 counsel to determine whether your use case requires a patent license or royalty payments. For more context,
 see the [FFmpeg Patent Mini-FAQ](https://ffmpeg.org/legal.html).

`openh264` has separate patent-licensing considerations. Cisco states that it covers MPEG LA AVC/H.264 patent-pool
royalties only for Cisco-distributed OpenH264 binary modules, subject to the applicable license terms and conditions.
If you build `openh264` from source, include it in `ffmpeg-kit-next` and distribute the resulting library yourself,
you may be responsible for any applicable patent licenses or royalties. See the
[OpenH264 FAQ](https://www.openh264.org/faq.html) for details.

### 17. Trademarks

<sup>1</sup> `FFmpeg` is a trademark of [Fabrice Bellard](http://www.bellard.org/). `FFmpegKitNext` and `FFmpegKit` are independent projects and not affiliated with the `FFmpeg` trademark holder.

### 18. See Also

- [FFmpeg API Documentation](https://ffmpeg.org/doxygen/4.0/index.html)
- [FFmpeg Wiki](https://trac.ffmpeg.org/wiki/WikiStart)
- [FFmpeg External Library Licenses](https://www.ffmpeg.org/doxygen/4.0/md_LICENSE.html)
