# FFmpegKitNext for iOS, iPadOS, macOS, tvOS and visionOS

`FFmpegKitNext` for Apple platforms can be built via [Nix](https://nixos.org/) and integrated locally.

### 1. Features
#### 1.1 iOS / iPadOS
- Supports `iOS 12.1+` deployment targets
- Includes `arm64`, `arm64-simulator`, `arm64e`, `x86_64`, `x86_64-mac-catalyst` and `arm64-mac-catalyst`
  architectures
- iPad devices and the iPad Simulator load `iOS` frameworks/xcframeworks
- Objective-C API
- Camera access
- `ARC` enabled library
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Creates local shared `frameworks`, `xcframeworks` and Swift package manifests

#### 1.2 macOS
- Supports `macOS 10.15+` deployment targets
- Includes `arm64` and `x86_64` architectures
- Objective-C API
- Camera access
- `ARC` enabled library
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Creates local shared `frameworks`, `xcframeworks` and Swift package manifests

#### 1.3 tvOS
- Supports `tvOS 11.0+` deployment targets
- Includes `arm64`, `arm64-simulator` and `x86_64` architectures
- Objective-C API
- `ARC` enabled library
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Creates local shared `frameworks`, `xcframeworks` and Swift package manifests

#### 1.4 visionOS
- Supports `visionOS 1.0+` deployment targets
- Includes `arm64` and `arm64-simulator` architectures
- Objective-C API
- `ARC` enabled library
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Creates local shared `frameworks`, `xcframeworks` and Swift package manifests

### 2. Building

Apple builds are Nix-based. You must install [Nix](https://nixos.org/) first to build the binaries. Then run the `nix-*` wrappers from the project root.

Note that, `FFmpegKitNext` does not publish binaries and building it yourself is the only way to use it.

Use `--list-profiles` to see the local Nix profiles available on your machine.

```
./nix-ios.sh --list-profiles
```

The current Apple profile is `xcode26`.

```
./nix-ios.sh -p xcode26 -x --spm
./nix-macos.sh -p xcode26 -x --spm
./nix-tvos.sh -p xcode26 -x --spm
./nix-visionos.sh -p xcode26 -x --spm
```

These commands build local `xcframeworks` and create a local `Package.swift` next to them. Omit `-x --spm` if you want
plain `framework` bundles instead.

After building the platform-specific frameworks, use `nix-apple.sh` to create an umbrella Apple `xcframework` bundle.

```
./nix-apple.sh -p xcode26 --spm
```

The build downloads `FFmpeg` and enabled external libraries when they are not already available locally. Nix provides
the build tools and pkg-config inputs. Xcode provides the Apple SDKs and Apple toolchain.

#### 2.1 Prerequisites

Apple builds require the following tools.

##### 2.1.1 iOS / iPadOS

- **Nix**
- **Xcode 26.0** or later
- **Command Line Tools**

##### 2.1.2 macOS

- **Nix**
- **Xcode 26.0** or later
- **Command Line Tools**

##### 2.1.3 tvOS

- **Nix**
- **Xcode 26.0** or later
- **Command Line Tools**

##### 2.1.4 visionOS

- **Nix**
- **Xcode 26.0** or later with the **visionOS (xros) SDK** installed
- **Command Line Tools**

#### 2.2 Options

Use `--enable-<library name>` flag to support additional external or system libraries and
`--disable-<architecture name>` to disable architectures you don't want to build.

```
./nix-ios.sh -p xcode26 --enable-fontconfig --disable-arm64e

./nix-macos.sh -p xcode26 --enable-freetype --enable-macos-avfoundation --disable-arm64

./nix-tvos.sh -p xcode26 --enable-dav1d --enable-libvpx --disable-arm64-simulator

./nix-visionos.sh -p xcode26 --enable-visionos-videotoolbox --enable-visionos-avfoundation --disable-arm64-simulator
```

Run `--help` to see all available build options.

Use `-x` or `--xcframework` to build `xcframework` bundles instead of framework bundles. Use `--spm` with `-x` to
create a local Swift package manifest next to the generated `xcframeworks`.

#### 2.3 Build Output

All libraries created can be found under the `prebuilt` directory.

- Platform `xcframeworks` are created under folders named `bundle-apple-xcframework-<platform>-<target>`.
- Platform `frameworks` are created under folders named `bundle-apple-framework-<platform-or-variant>-<target>`.
- Umbrella Apple `xcframeworks` are created under folders named `umbrella-apple-xcframework-...`.
- When `--spm` is used, `Package.swift` is created in the generated `xcframework` folder.

### 3. Using

#### 3.1 Local Integration

Build it locally first, then integrate the generated artifacts.

- For Swift Package Manager, add the generated local package folder from `prebuilt/...` to your Xcode project.
- For manual integration, add the generated `xcframeworks` or `frameworks` from `prebuilt/...` to your app target.
- Link every generated FFmpeg framework together with `ffmpegkit`.

#### 3.2 Objective-C API

1. Execute synchronous `FFmpeg` commands.

    ```objectivec
    #include <ffmpegkit/FFmpegKit.h>

    FFmpegSession *session = [FFmpegKit execute:@"-i file1.mp4 -c:v mpeg4 file2.mp4"];
    ReturnCode *returnCode = [session getReturnCode];
    if ([ReturnCode isSuccess:returnCode]) {

        // SUCCESS

    } else if ([ReturnCode isCancel:returnCode]) {

        // CANCEL

    } else {

        // FAILURE
        NSLog(@"Command failed with state %@ and rc %@.%@", [FFmpegKitConfig sessionStateToString:[session getState]], returnCode, [session getFailStackTrace]);

    }
    ```

2. Each `execute` call (sync or async) creates a new session. Access every detail about your execution from the
   session created.

    ```objectivec
    FFmpegSession *session = [FFmpegKit execute:@"-i file1.mp4 -c:v mpeg4 file2.mp4"];

    // Unique session id created for this execution
    long sessionId = [session getSessionId];

    // Command arguments as a single string
    NSString *command = [session getCommand];

    // Command arguments
    NSArray *arguments = [session getArguments];
   
    // State of the execution. Shows whether it is still running or completed
    SessionState state = [session getState];

    // Return code for completed sessions. Will be null if session is still running or ends with a failure
    ReturnCode *returnCode = [session getReturnCode];

    NSDate *startTime =[session getStartTime];
    NSDate *endTime =[session getEndTime];
    long duration =[session getDuration];

    // Console output generated for this execution
    NSString *output = [session getOutput];

    // The stack trace if FFmpegKit fails to run a command
    NSString *failStackTrace = [session getFailStackTrace];

    // The list of logs generated for this execution
    NSArray *logs = [session getLogs];

    // The list of statistics generated for this execution
    NSArray *statistics = [session getStatistics];
    ```

3. Execute asynchronous `FFmpeg` commands by providing session specific `execute`/`log`/`session` callbacks.

    ```objectivec
    FFmpegSession* session = [FFmpegKit executeAsync:@"-i file1.mp4 -c:v mpeg4 file2.mp4" withCompleteCallback:^(FFmpegSession* session){
        SessionState state = [session getState];
        ReturnCode *returnCode = [session getReturnCode];

        // CALLED WHEN SESSION IS EXECUTED

        NSLog(@"FFmpeg process exited with state %@ and rc %@.%@", [FFmpegKitConfig sessionStateToString:state], returnCode, [session getFailStackTrace]);

    } withLogCallback:^(Log *log) {

        // CALLED WHEN SESSION PRINTS LOGS

    } withStatisticsCallback:^(Statistics *statistics) {

        // CALLED WHEN SESSION GENERATES STATISTICS

    }];
    ```

4. Execute `FFprobe` commands.

    - Synchronous

    ```objectivec
    FFprobeSession *session = [FFprobeKit execute:ffprobeCommand];

    if (![ReturnCode isSuccess:[session getReturnCode]]) {
        NSLog(@"Command failed. Please check output for the details.");
    }
    ```

   - Asynchronous

    ```objectivec
    [FFprobeKit executeAsync:ffmpegCommand withCompleteCallback:^(FFprobeSession* session) {

        CALLED WHEN SESSION IS EXECUTED

    }];
    ```

5. Get media information for a file.

    ```objectivec
    MediaInformationSession *mediaInformation = [FFprobeKit getMediaInformation:@"<file path or uri>"];
    MediaInformation *mediaInformation =[mediaInformation getMediaInformation];
    ```

6. Stop ongoing `FFmpeg` operations.

   - Stop all executions
       ```objectivec
       [FFmpegKit cancel];
       ```
   - Stop a specific session
       ```objectivec
       [FFmpegKit cancel:sessionId];
       ```

7. Get previous `FFmpeg` and `FFprobe` sessions from session history.

    ```objectivec
    NSArray* sessions = [FFmpegKitConfig getSessions];
    for (int i = 0; i < [sessions count]; i++) {
        id<Session> session = [sessions objectAtIndex:i];
        NSLog(@"Session %d = id: %ld, startTime: %@, duration: %ld, state:%@, returnCode:%@.\n",
            i,
            [session getSessionId],
            [session getStartTime],
            [session getDuration],
            [FFmpegKitConfig sessionStateToString:[session getState]],
            [session getReturnCode]);
    }
    ```

8. Enable global callbacks.

    - Session type specific Complete Callbacks, called when an async session has been completed

        ```objectivec
        [FFmpegKitConfig enableFFmpegSessionCompleteCallback:^(FFmpegSession* session) {
            ...
        }];

        [FFmpegKitConfig enableFFprobeSessionCompleteCallback:^(FFprobeSession* session) {
            ...
        }];

        [FFmpegKitConfig enableMediaInformationSessionCompleteCallback:^(MediaInformationSession* session) {
            ...
        }];
        ```

    - Log Callback, called when a session generates logs

        ```objectivec
        [FFmpegKitConfig enableLogCallback:^(Log *log) {
            ...
        }];
        ```

    - Statistics Callback, called when a session generates statistics

        ```objectivec
        [FFmpegKitConfig enableStatisticsCallback:^(Statistics *statistics) {
            ...
        }];
        ```

9. Ignore the handling of a signal. Required by `Mono` and frameworks that use `Mono`, e.g. `Unity` and `Xamarin`.

    ```objectivec
    [FFmpegKitConfig ignoreSignal:SIGXCPU];
    ```

10. Register system fonts and custom font directories.

    ```objectivec
    [FFmpegKitConfig setFontDirectoryList:[NSArray arrayWithObjects:@"/System/Library/Fonts", @"<folder with fonts>", nil] with:nil];
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running `iOS`, `iPadOS`, `macOS` and `tvOS` test
applications developed under the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.
