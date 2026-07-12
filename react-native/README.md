# FFmpegKitNext for React Native

### 1. Features
- Includes both `FFmpeg` and `FFprobe`
- Supports
  - `Android`, `iOS` and `iPadOS`
  - `arm-v7a`, `arm-v7a-neon`, `arm64-v8a`, `x86` and `x86_64` architectures on Android
  - `Android API Level 24` or later
  - `armv7`, `armv7s`, `arm64`, `arm64-simulator`, `i386`, `x86_64`, `x86_64-mac-catalyst` and `arm64-mac-catalyst` architectures on iOS
  - `iOS 12.1+` deployment targets
  - iPad devices and the iPad Simulator load `iOS` frameworks/xcframeworks
  - Can process Storage Access Framework (SAF) Uris on Android
  - 25 external libraries

    `dav1d`, `fontconfig`, `freetype`, `fribidi`, `gmp`, `gnutls`, `kvazaar`, `lame`, `libass`, `libiconv`, `libilbc`, `libtheora`, `libvorbis`, `libvpx`, `libwebp`, `libxml2`, `opencore-amr`, `opus`, `shine`, `snappy`, `soxr`, `speex`, `twolame`, `vo-amrwbenc`, `zimg`

  - 4 external libraries with GPL license

    `vid.stab`, `x264`, `x265`, `xvidcore`

  - `zlib` and `MediaCodec` Android system libraries
  - `bzip2`, `iconv`, `libuuid`, `zlib` system libraries and `AudioToolbox`, `VideoToolbox`, `AVFoundation` system frameworks on iOS/iPadOS

- Includes Typescript definitions
- Licensed under `LGPL 3.0` by default, `GPL v3.0` if GPL licensed libraries are enabled

### 2. Installation

`ffmpeg-kit-next-react-native` is not published to `npm`. Build `FFmpegKitNext` locally for the native platforms you
target, then integrate the React Native package from this repository using a local file dependency.

> **Building the native binaries:** see [BUILD.md](BUILD.md) for the one-time setup that builds `FFmpegKitNext`
> from source and copies the native binaries into this plugin before you use it.

```sh
yarn add file:../ffmpeg-kit-next/react-native
```

Or with npm:

```sh
npm install ../ffmpeg-kit-next/react-native
```

Adjust the path to match where this repository is located relative to your React Native application.

Do not install this plugin from the npm registry. The plugin and the native binaries are expected to come from your
local build.

**Android requires one extra step.** The plugin's native AAR is served from a local Maven repository bundled inside
the plugin (`android/libs-maven`). Because Gradle does not inherit a dependency's repositories, your app must declare
that repository too, or it will fail to resolve `com.arthenica:ffmpeg-kit-next` while building the APK. Add it to your
app's `android/build.gradle`, deriving the path from the autolinked plugin project so nothing is hardcoded:

```groovy
allprojects {
    repositories {
        def ffmpegKitProject = rootProject.findProject(":ffmpeg-kit-next-react-native")
        if (ffmpegKitProject != null) {
            maven {
                url "${ffmpegKitProject.projectDir}/libs-maven"
            }
        }
    }
}
```

`ffmpegKitProject.projectDir` is the plugin's `android/` folder inside `node_modules`, so the same snippet works for
any app. iOS and iPadOS need no extra repository step.

#### 2.1 Build Variants and External Libraries

`FFmpeg` includes built-in encoders for some popular formats. Some formats/codecs require external libraries to be
enabled in the native build. For example, `mp3` encoding needs `lame` or `shine`, `h264` needs `x264`, and `vp8`/`vp9`
needs `libvpx`.

Those libraries are selected when you build `FFmpegKitNext` locally with the Nix-based build scripts. There are no
separate npm packages such as `min`, `https`, `audio`, `video`, `full`, or `full-gpl` to install.

#### 2.2 Platform Support

`ffmpeg-kit-next-react-native` supports `Android API Level 24` or later and `iOS/iPadOS 12.1+` deployment targets.

### 3. Using

1. Execute FFmpeg commands.

    ```js
    import { FFmpegKit } from 'ffmpeg-kit-next-react-native';

    FFmpegKit.execute('-i file1.mp4 -c:v mpeg4 file2.mp4').then(async (session) => {
      const returnCode = await session.getReturnCode();

      if (ReturnCode.isSuccess(returnCode)) {

        // SUCCESS

      } else if (ReturnCode.isCancel(returnCode)) {

        // CANCEL

      } else {

        // ERROR

      }
    });
    ```

2. Each `execute` call creates a new session. Access every detail about your execution from the
   session created.

    ```js
    FFmpegKit.execute('-i file1.mp4 -c:v mpeg4 file2.mp4').then(async (session) => {

      // Unique session id created for this execution
      const sessionId = session.getSessionId();

      // Command arguments as a single string
      const command = session.getCommand();

      // Command arguments
      const commandArguments = session.getArguments();

      // State of the execution. Shows whether it is still running or completed
      const state = await session.getState();

      // Return code for completed sessions. Will be undefined if session is still running or FFmpegKit fails to run it
      const returnCode = await session.getReturnCode()

      const startTime = session.getStartTime();
      const endTime = await session.getEndTime();
      const duration = await session.getDuration();

      // Console output generated for this execution
      const output = await session.getOutput();

      // The stack trace if FFmpegKit fails to run a command
      const failStackTrace = await session.getFailStackTrace()

      // The list of logs generated for this execution
      const logs = await session.getLogs();

      // The list of statistics generated for this execution (only available on FFmpegSession)
      const statistics = await session.getStatistics();

    });
    ```

3. Execute `FFmpeg` commands by providing session specific `execute`/`log`/`session` callbacks.

    ```js
    FFmpegKit.executeAsync('-i file1.mp4 -c:v mpeg4 file2.mp4', session => {

      // CALLED WHEN SESSION IS EXECUTED

    }, log => {

      // CALLED WHEN SESSION PRINTS LOGS

    }, statistics => {

      // CALLED WHEN SESSION GENERATES STATISTICS

    });
    ```

4. Execute `FFprobe` commands.

    ```js
    FFprobeKit.execute(ffprobeCommand).then(async (session) => {

      // CALLED WHEN SESSION IS EXECUTED

    });
    ```

5. Get media information for a file/url.

    ```js
    FFprobeKit.getMediaInformation(testUrl).then(async (session) => {
      const information = await session.getMediaInformation();

      if (information === undefined) {

        // CHECK THE FOLLOWING ATTRIBUTES ON ERROR
        const state = FFmpegKitConfig.sessionStateToString(await session.getState());
        const returnCode = await session.getReturnCode();
        const failStackTrace = await session.getFailStackTrace();
        const duration = await session.getDuration();
        const output = await session.getOutput();
      }
    });
    ```

6. Stop ongoing FFmpeg operations.

  - Stop all sessions
    ```js
    FFmpegKit.cancel();
    ```
  - Stop a specific session
    ```js
    FFmpegKit.cancel(sessionId);
    ```

7. (Android) Convert Storage Access Framework (SAF) Uris into paths that can be read or written by
`FFmpegKit` and `FFprobeKit`.

  - Reading a file:
    ```js
    FFmpegKitConfig.selectDocumentForRead('*/*').then(uri => {
        // By default a saf url can be used only once and is released automatically
        // when the execution completes. Pass the optional reusable flag to use the
        // same url in more than one command and release it manually afterwards.
        FFmpegKitConfig.getSafParameterForRead(uri, true).then(safUrl => {
            FFmpegKit.executeAsync(`-i ${safUrl} -c:v mpeg4 file2.mp4`).then(_ => {
                FFmpegKitConfig.unregisterSafProtocolUrl(safUrl);
            });
        });
    });
    ```

  - Writing to a file:
    ```js
    FFmpegKitConfig.selectDocumentForWrite('video.mp4', 'video/*').then(uri => {
        FFmpegKitConfig.getSafParameterForWrite(uri).then(safUrl => {
            FFmpegKit.executeAsync(`-i file1.mp4 -c:v mpeg4 ${safUrl}`);
        });
    });
    ```

8. Get previous `FFmpeg`, `FFprobe` and `MediaInformation` sessions from the session history.

    ```js
    FFmpegKit.listSessions().then(sessionList => {
      sessionList.forEach(async session => {
        const sessionId = session.getSessionId();
      });
    });

    FFprobeKit.listFFprobeSessions().then(sessionList => {
      sessionList.forEach(async session => {
        const sessionId = session.getSessionId();
      });
    });

    FFprobeKit.listMediaInformationSessions().then(sessionList => {
      sessionList.forEach(async session => {
        const sessionId = session.getSessionId();
      });
    });
    ```

9. Enable global callbacks.
  - Session type specific Complete Callbacks, called when an async session has been completed

    ```js
    FFmpegKitConfig.enableFFmpegSessionCompleteCallback(session => {
      const sessionId = session.getSessionId();
    });

    FFmpegKitConfig.enableFFprobeSessionCompleteCallback(session => {
      const sessionId = session.getSessionId();
    });

    FFmpegKitConfig.enableMediaInformationSessionCompleteCallback(session => {
      const sessionId = session.getSessionId();
    });
    ```

  - Log Callback, called when a session generates logs

    ```js
    FFmpegKitConfig.enableLogCallback(log => {
      const message = log.getMessage();
    });
    ```

  - Statistics Callback, called when a session generates statistics

    ```js
    FFmpegKitConfig.enableStatisticsCallback(statistics => {
      const size = statistics.getSize();
    });
    ```

10. Register system fonts and custom font directories.

    ```js
    FFmpegKitConfig.setFontDirectoryList(["/system/fonts", "/System/Library/Fonts", "<folder with fonts>"]);
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running `react-native` test applications developed under
the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.

### 5. Tips

See [Tips](https://github.com/arthenica/ffmpeg-kit-next/wiki/Tips) wiki page.

### 6. License

See [License](https://github.com/arthenica/ffmpeg-kit-next/wiki/License) wiki page.

### 7. Patents

See [Patents](https://github.com/arthenica/ffmpeg-kit-next/wiki/Patents) wiki page.
