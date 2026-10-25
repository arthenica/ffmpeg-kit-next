# Using this plugin with locally-built native binaries

This plugin is **not** published to pub.dev and does **not** download native
binaries from any remote repository (Maven Central / CocoaPods trunk). Instead
the FFmpegKitNext native binaries are built from source and copied into the
plugin's own platform folders, so the package is self-contained and consumed
via a local path dependency.

## One-time setup (per machine / CI)

From the repository root, build the native binaries for the platforms you need
with the desired external libraries:

```bash
./nix-android.sh -p <profile>   # produces prebuilt/bundle-android-aar-*-maven/...
./nix-ios.sh     -p <profile>   # produces prebuilt/bundle-apple-xcframework-ios-*/... (covers both iOS and iPadOS)
./nix-macos.sh   -p <profile>   # produces prebuilt/bundle-apple-xcframework-macos-*/...
./nix-linux.sh   -p <profile>   # produces prebuilt/bundle-linux/... (native host architecture only)
./nix-web.sh     -p <profile>   # produces prebuilt/bundle-web-wasm32/ffmpeg-kit-next/
./windows.sh                    # produces prebuilt/bundle-windows/ffmpeg-kit-next/
```

Then copy the built artifacts into this plugin:

```bash
cd flutter/flutter
./copy_local_binaries.sh            # all available platforms
# or copy selectively:
./copy_local_binaries.sh android ios
./copy_local_binaries.sh linux
./copy_local_binaries.sh windows
./copy_local_binaries.sh web
```

This places:

| Platform | Destination (git-ignored)                                | Referenced by                       |
|----------|----------------------------------------------------------|-------------------------------------|
| Android  | `android/libs-maven/` (local Maven repository)           | `android/build.gradle`              |
| iOS/iPadOS     | `ios/ffmpeg_kit_next_flutter/Frameworks/*.xcframework`   | `ios/*.podspec` + `Package.swift`   |
| macOS    | `macos/ffmpeg_kit_next_flutter/Frameworks/*.xcframework` | `macos/*.podspec` + `Package.swift` |
| Linux    | `linux/Frameworks/ffmpeg-kit-next/{include,lib,licenses}` | `linux/CMakeLists.txt`             |
| Windows  | `windows/Frameworks/ffmpeg-kit-next/{include,lib,bin,licenses}` | `windows/CMakeLists.txt`     |
| Web      | `web/ffmpeg-kit-next/{dist,lib,licenses}`, `LICENSE`, `SOURCE` | `pubspec.yaml` (web-only assets) |

The copied binaries are ignored by git on purpose — every consumer rebuilds and
copies them. Re-run `copy_local_binaries.sh` whenever you rebuild.

The licence texts of the native libraries ship with them. Linux and Windows apps
get them in an `ffmpeg-kit-next-licenses` folder next to the executable; web
builds get `LICENSE`, `SOURCE` and `licenses/` next to the web bundle. Android and
Apple carry them inside the AAR and the xcframeworks.

On Android the copy is a small local Maven repository at
`android/libs-maven/com/arthenica/ffmpeg-kit-next/<version>/` (e.g.
`ffmpeg-kit-next-<version>.aar` plus its `.pom`). `android/build.gradle` reads the
version from that POM and uses a strict Maven dependency on it. The copy must
contain exactly one native version and its matching AAR; missing, incomplete,
or ambiguous copies fail during configuration. The native version is independent
of the Flutter package version, and neither needs to be edited into Gradle.

Linux and Windows consume the copied headers and native libraries without a
release-number requirement. Linux checks for the required headers and shared
libraries; Windows keeps its CMake imported targets and x64 checks. Rebuild and
copy the native bundle whenever its sources change.

The Apple plugins ship both a CocoaPods podspec and a Swift Package Manager
`Package.swift` (under `<platform>/ffmpeg_kit_next_flutter/`). Flutter uses the
Swift package when Swift Package Manager is enabled and falls back to the podspec
otherwise; both reference the same `Sources/` and `Frameworks/` trees.
Both podspecs read the Flutter package version from `pubspec.yaml`.

## Consuming the plugin from an app

### Windows x64

Windows builds require MSYS2 CLANG64, Visual Studio's Desktop development with C++
workload, and the Windows Flutter SDK. Complete the Windows build and copy steps
in the one-time setup above before consuming the plugin.

The default bundle is LGPL without optional external libraries. It is created
at `prebuilt/bundle-windows/ffmpeg-kit-next` and copied to the git-ignored
`flutter/flutter/windows/Frameworks/ffmpeg-kit-next` directory.

The plugin copies its runtime DLLs beside the app executable and their licence
texts into an `ffmpeg-kit-next-licenses` folder there. Distribute the entire
Release directory. Rebuild the bundle and repeat the copy step above whenever its native sources change.

After adding the local dependency below, run `flutter build windows --debug` or
`flutter build windows --release` from the consuming app.

Pipe registration and closing are forwarded to the native library. The current
Windows implementation returns `null` from `registerNewFFmpegPipe`, and
`closeFFmpegPipe` does nothing. The `writeToPipe` helper returns a `NOT_SUPPORTED`
platform error on Windows.

### Local dependency

In the app's `pubspec.yaml`, reference the plugin by path:

```yaml
dependencies:
  ffmpeg_kit_next_flutter:
    path: /path/to/ffmpeg-kit-next/flutter/flutter
```

The plugin already resolves `ffmpeg_kit_next_flutter_platform_interface` from
the sibling repository directory. A consuming application can explicitly
override that location when needed:

```yaml
dependency_overrides:
  ffmpeg_kit_next_flutter_platform_interface:
    path: /path/to/ffmpeg-kit-next/flutter/flutter_platform_interface
```

### Android: register the bundled Maven repository

The plugin's Android AAR is served from the local Maven repository copied into
`android/libs-maven` (see the table above). Gradle does **not** inherit a
dependency's repositories, so the **consuming app** must declare that repository
too — otherwise the app fails to resolve `com.arthenica:ffmpeg-kit-next:<version>`
while packaging the APK.

Add it to the app's `android/build.gradle` as a **project-level** repository.
Derive the path from the Gradle project Flutter already includes for the plugin,
so you never hardcode a path into the plugin's source tree:

```groovy
allprojects {
    repositories {
        google()
        mavenCentral()
        def ffmpegKitProject = rootProject.findProject(":ffmpeg_kit_next_flutter")
        if (ffmpegKitProject != null) {
            exclusiveContent {
                forRepository {
                    maven {
                        url "${ffmpegKitProject.projectDir}/libs-maven"
                    }
                }
                filter {
                    includeModule('com.arthenica', 'ffmpeg-kit-next')
                }
            }
        }
    }
}
```

`ffmpegKitProject.projectDir` is the plugin's `android/` folder wherever Flutter
resolved the dependency (local path, pub cache, …), so the same snippet works for
any consumer without editing.

The `exclusiveContent` rule reserves `com.arthenica:ffmpeg-kit-next` for this
local repository, including while resolving the app's runtime dependencies.
The plugin's strict dependency selects the version recorded in the copied POM;
an incompatible version request fails instead of silently upgrading it. Other
dependencies continue to resolve from their normal repositories.

Keep this in `build.gradle` (project-level). Do **not** move it into
`settings.gradle`'s `dependencyResolutionManagement` with
`RepositoriesMode.PREFER_SETTINGS`: that mode suppresses Flutter's own
project-level engine repository and breaks `io.flutter:*` resolution. iOS,
iPadOS and macOS need no extra repository step — their frameworks are vendored
via the podspec / Swift package.

## Notes / caveats

- **ABIs / archs are whatever you built.** The Android AAR contains only the
  ABIs compiled by your nix profile, and the Apple xcframeworks only the
  built slices. If the AAR lacks an ABI your build targets (e.g. `x86_64` for an
  emulator), restrict the app with `ndk { abiFilters ... }` or build that ABI.
- **`smart-exception-java` and `kotlin-stdlib` are still fetched from Maven
  Central.** They are JVM dependencies of the AAR (not native binaries), declared
  in the AAR's POM inside `android/libs-maven` and resolved transitively
  (`android/build.gradle` keeps `mavenCentral()` for this). If you must avoid
  Maven entirely, vendor them as local jars instead.
- **External libraries are fixed by what you enabled.** They are determined by
  the flags passed to `nix-<platform>.sh`.
