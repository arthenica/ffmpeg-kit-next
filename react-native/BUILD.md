# Using this plugin with locally-built native binaries

This plugin is **not** published to `npm` and does **not** download native
binaries from any remote repository (Maven Central / CocoaPods trunk). Instead
the FFmpegKitNext native binaries are built from source and copied into the
plugin's own platform folders, so the package is self-contained and consumed
via a local file dependency.

## One-time setup (per machine / CI)

From the repository root, build the native binaries for the platforms you need
with the desired external libraries:

```bash
./nix-android.sh -p <profile>   # produces prebuilt/bundle-android-aar-*-maven/...
./nix-ios.sh     -p <profile>   # produces prebuilt/bundle-apple-xcframework-ios-*/... (covers both iOS and iPadOS)
```

Then copy the built artifacts into this plugin:

```bash
cd react-native
./copy_local_binaries.sh            # all available platforms
# or copy selectively:
./copy_local_binaries.sh android
./copy_local_binaries.sh ios
```

This places:

| Platform | Destination (git-ignored)      | Referenced by                                |
|----------|---------------------------------|-----------------------------------------------|
| Android  | `android/libs-maven/` (local Maven repository) | `android/build.gradle`          |
| iOS/iPadOS      | `ios/Frameworks/*.xcframework`  | `ffmpeg-kit-next-react-native.podspec`         |

The copied binaries are ignored by git on purpose — every consumer rebuilds and
copies them. Re-run `copy_local_binaries.sh` whenever you rebuild.

On Android the copy is a small local Maven repository at
`android/libs-maven/com/arthenica/ffmpeg-kit-next/<version>/` (e.g.
`ffmpeg-kit-next-7.1.0.aar` plus its `.pom`). `android/build.gradle` consumes it via
`maven { url "$projectDir/libs-maven" }` and depends on it by Maven coordinate
`implementation 'com.arthenica:ffmpeg-kit-next:<version>'`.

The iOS/iPadOS plugin ships a CocoaPods podspec
(`ffmpeg-kit-next-react-native.podspec`) that vendors the frameworks directly
via `s.vendored_frameworks = "ios/Frameworks/*.xcframework"`.

## Consuming the plugin from an app

In the app's `package.json`, reference the plugin by a local file path:

```bash
yarn add file:../ffmpeg-kit-next/react-native
```

Or with npm:

```bash
npm install ../ffmpeg-kit-next/react-native
```

Adjust the path to match where this repository is located relative to your
React Native application.

### Android: register the bundled Maven repository

The plugin's Android AAR is served from the local Maven repository copied into
`android/libs-maven` (see the table above). Gradle does **not** inherit a
dependency's repositories, so the **consuming app** must declare that repository
too — otherwise the app fails to resolve `com.arthenica:ffmpeg-kit-next:<version>`
while packaging the APK.

Add it to the app's `android/build.gradle` as a **project-level** repository.
Derive the path from the autolinked plugin project React Native already
includes, so you never hardcode a path into the plugin's source tree:

```groovy
allprojects {
    repositories {
        google()
        mavenCentral()
        def ffmpegKitProject = rootProject.findProject(":ffmpeg-kit-next-react-native")
        if (ffmpegKitProject != null) {
            maven {
                url "${ffmpegKitProject.projectDir}/libs-maven"
            }
        }
    }
}
```

`ffmpegKitProject.projectDir` is the plugin's `android/` folder inside
`node_modules` wherever autolinking resolved the dependency, so the same
snippet works for any consumer without editing. iOS and iPadOS need no extra
repository step — the framework is vendored via the podspec.

### iOS/iPadOS: install pods after copying binaries

After running `copy_local_binaries.sh ios`, run `pod install` (or
`bundle exec pod install`) in the app's `ios/` directory so CocoaPods picks up
the `.xcframework` files referenced by `s.vendored_frameworks`. Re-run it
whenever you rebuild and re-copy the binaries.

## Notes / caveats

- **ABIs / archs are whatever you built.** The Android AAR contains only the
  ABIs compiled by your nix profile, and the Apple xcframeworks only the built
  slices. If the AAR lacks an ABI your build targets (e.g. `x86_64` for an
  emulator), restrict the app with `ndk { abiFilters ... }` or build that ABI.
- **`smart-exception-java` is still fetched from Maven Central.** It is a small
  pure-Java dependency of the AAR (not a native binary), declared in the AAR's POM
  inside `android/libs-maven` and resolved transitively (`android/build.gradle`
  keeps `mavenCentral()` for this). If you must avoid Maven entirely, vendor it as
  a local jar instead.
- **External libraries are fixed by what you enabled.** They are determined by
  the flags passed to `nix-<platform>.sh`.
