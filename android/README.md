# FFmpegKitNext for Android

`FFmpegKitNext` for Android can be built via [Nix](https://nixos.org/) and integrated locally.

### 1. Features
- Supports `API Level 24+`
- Includes `arm-v7a`, `arm-v7a-neon`, `arm64-v8a`, `x86` and `x86_64` architectures
- Kotlin API, fully compatible with Java and Kotlin callers
- Camera access on [supported devices](https://developer.android.com/ndk/guides/stable_apis#camera)
- Custom `FFmpegKit` protocols: `ffkitsaf:` for Storage Access Framework (SAF) Uris, `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Creates shared native libraries (`.so`) and a local Maven repository containing the Android archive (`.aar`)

### 2. Building

Android builds are Nix-based. You must install [Nix](https://nixos.org/) first to build the binaries. Then run the
`nix-android.sh` wrapper from the project root.

Note that, `FFmpegKitNext` does not publish binaries and building it yourself is the only way to use it.

Use `--list-profiles` to see the local Nix profiles available on your machine.

```
./nix-android.sh --list-profiles
```

The current Android profile is `android-r27d`.

```
./nix-android.sh -p android-r27d
```

This command cross-compiles the native `FFmpeg` and `ffmpeg-kit` shared libraries and packages them, together with
the Kotlin API, into a local Android archive (`.aar`).

The build downloads `FFmpeg` and enabled external libraries when they are not already available locally. Nix provides
the Android SDK, NDK (r27d), CMake and the build tools.

#### 2.1 Prerequisites

Android builds require the following tools.

- **Nix** — the `android-r27d` profile supplies the Android SDK, NDK r27d and CMake.

#### 2.2 Options

Use `--enable-<library name>` flag to support additional external or system libraries and
`--disable-<architecture name>` to disable architectures you don't want to build. Use `--enable-gpl` to allow
GPL-licensed libraries.

```
./nix-android.sh -p android-r27d --enable-fontconfig --disable-arm-v7a-neon
```

Run `--help` to see all available build options.

#### 2.3 Build Output

All libraries created can be found under the `prebuilt` directory.

- A local Maven repository is created under each `bundle-android-aar-*-maven` folder, containing the Android
  archive (`.aar`, artifact id `ffmpeg-kit-next`) and its generated POM.

For example, a default `API Level 24` build produces:

```
prebuilt/bundle-android-aar-24-maven/
└── com/arthenica/ffmpeg-kit-next/6.1.2/
    ├── ffmpeg-kit-next-6.1.2.aar
    └── ffmpeg-kit-next-6.1.2.pom
```

### 3. Using

#### 3.1 Local Integration

Build it locally first, then consume the generated local Maven repository.

```groovy
repositories {
    // Local Maven repository produced by the build (match the API level you built).
    maven { url "<path-to-repo>/prebuilt/bundle-android-aar-24-maven" }
    // Resolves smart-exception-java, a transitive dependency declared in the POM.
    mavenCentral()
}

dependencies {
    implementation 'com.arthenica:ffmpeg-kit-next:6.1.2'
}
```

#### 3.2 Android API

The library is written in Kotlin and exposes a Java-style API. The examples below are in Java; the same `getX()`
methods and callbacks work identically from Kotlin (callbacks accept lambdas, and the `Statistics`, `Log` and
`ReturnCode` classes also expose Kotlin properties such as `log.message`, `statistics.videoFps` and `returnCode.value`).

1. Execute synchronous `FFmpeg` commands.

    ```java
    import com.arthenica.ffmpegkit.FFmpegKit;

    FFmpegSession session = FFmpegKit.execute("-i file1.mp4 -c:v mpeg4 file2.mp4");
    ReturnCode returnCode = session.getReturnCode();
    if (ReturnCode.isSuccess(returnCode)) {

        // SUCCESS

    } else if (ReturnCode.isCancel(returnCode)) {

        // CANCEL

    } else {

        // FAILURE
        Log.d(TAG, String.format("Command failed with state %s and rc %s.%s", FFmpegKitConfig.sessionStateToString(session.getState()), returnCode, session.getFailStackTrace()));

    }
    ```

2. Each `execute` call (sync or async) creates a new session. Access every detail about your execution from the
   session created.

    ```java
    FFmpegSession session = FFmpegKit.execute("-i file1.mp4 -c:v mpeg4 file2.mp4");

    // Unique session id created for this execution
    long sessionId = session.getSessionId();

    // Command arguments as a single string
    String command = session.getCommand();

    // Command arguments
    String[] arguments = session.getArguments();

    // State of the execution. Shows whether it is still running or completed
    SessionState state = session.getState();

    // Return code for completed sessions. Will be null if session is still running or ends with a failure
    ReturnCode returnCode = session.getReturnCode();

    Date startTime = session.getStartTime();
    Date endTime = session.getEndTime();
    long duration = session.getDuration();

    // Console output generated for this execution
    String output = session.getOutput();

    // The stack trace if FFmpegKit fails to run a command
    String failStackTrace = session.getFailStackTrace();

    // The list of logs generated for this execution
    List<com.arthenica.ffmpegkit.Log> logs = session.getLogs();

    // The list of statistics generated for this execution
    List<Statistics> statistics = session.getStatistics();
    ```

3. Execute asynchronous `FFmpeg` commands by providing session specific `execute`/`log`/`session` callbacks (shown
   here as lambdas; anonymous classes work too).

    ```java
    FFmpegSession session = FFmpegKit.executeAsync("-i file1.mp4 -c:v mpeg4 file2.mp4", s -> {
        SessionState state = s.getState();
        ReturnCode returnCode = s.getReturnCode();

        // CALLED WHEN SESSION IS EXECUTED

        Log.d(TAG, String.format("FFmpeg process exited with state %s and rc %s.%s", FFmpegKitConfig.sessionStateToString(state), returnCode, s.getFailStackTrace()));

    }, log -> {

        // CALLED WHEN SESSION PRINTS LOGS

    }, statistics -> {

        // CALLED WHEN SESSION GENERATES STATISTICS

    });
    ```

4. Execute `FFprobe` commands.

    - Synchronous

    ```java
    FFprobeSession session = FFprobeKit.execute(ffprobeCommand);

    if (!ReturnCode.isSuccess(session.getReturnCode())) {
        Log.d(TAG, "Command failed. Please check output for the details.");
    }
    ```

    - Asynchronous

    ```java
    FFprobeKit.executeAsync(ffprobeCommand, session -> {

        // CALLED WHEN SESSION IS EXECUTED

    });
    ```

5. Get media information for a file.

    ```java
    MediaInformationSession session = FFprobeKit.getMediaInformation("<file path or uri>");
    MediaInformation information = session.getMediaInformation();
    ```

6. Stop ongoing `FFmpeg` operations.

    - Stop all executions
        ```java
        FFmpegKit.cancel();
        ```
    - Stop a specific session
        ```java
        FFmpegKit.cancel(sessionId);
        ```

7. Get previous `FFmpeg` and `FFprobe` sessions from session history.

    ```java
    List<Session> sessions = FFmpegKitConfig.getSessions();
    for (int i = 0; i < sessions.size(); i++) {
        Session session = sessions.get(i);
        Log.d(TAG, String.format("Session %d = id:%d, startTime:%s, duration:%d, state:%s, returnCode:%s.",
            i,
            session.getSessionId(),
            session.getStartTime(),
            session.getDuration(),
            FFmpegKitConfig.sessionStateToString(session.getState()),
            session.getReturnCode()));
    }
    ```

8. Enable global callbacks.

    - Session type specific Complete Callbacks, called when an async session has been completed

        ```java
        FFmpegKitConfig.enableFFmpegSessionCompleteCallback(session -> {
            ...
        });

        FFmpegKitConfig.enableFFprobeSessionCompleteCallback(session -> {
            ...
        });

        FFmpegKitConfig.enableMediaInformationSessionCompleteCallback(session -> {
            ...
        });
        ```

    - Log Callback, called when a session generates logs

        ```java
        FFmpegKitConfig.enableLogCallback(log -> {
            ...
        });
        ```

    - Statistics Callback, called when a session generates statistics

        ```java
        FFmpegKitConfig.enableStatisticsCallback(statistics -> {
            ...
        });
        ```

9. Ignore the handling of a signal. Required by `Mono` and frameworks that use `Mono`, e.g. `Unity` and `Xamarin`.

    ```java
    FFmpegKitConfig.ignoreSignal(Signal.SIGXCPU);
    ```

10. Register system fonts and custom font directories.

    ```java
    FFmpegKitConfig.setFontDirectoryList(context, Arrays.asList("/system/fonts", "<folder with fonts>"), Collections.emptyMap());
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running the `Android` test application developed
under the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.
