# FFmpegKitNext for Linux

`FFmpegKitNext` for Linux can be built via [Nix](https://nixos.org/) and integrated locally.

### 1. Features
- Provides a `C++` API built with `c++11`
- Includes `arm64` and `x86_64` architectures
- Libraries are compiled natively, so only the architecture of the host machine is built
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Builds shared native libraries (`.so`)

### 2. Building

Linux builds are Nix-based. You must install [Nix](https://nixos.org/) first to build the binaries. Then run the
`nix-linux.sh` wrapper from the project root.

Note that, `FFmpegKitNext` does not publish binaries and building it yourself is the only way to use it.

Use `--list-profiles` to see the local Nix profiles available on your machine.

```
./nix-linux.sh --list-profiles
```

The current Linux profile is `default`.

```
./nix-linux.sh -p default
```

This command compiles the native `FFmpeg` and `ffmpeg-kit` shared libraries together with the `C++` API for the
architecture of the host machine. Building `arm64` libraries requires an `arm64` Linux host, and building `x86_64`
libraries requires an `x86_64` Linux host.

Unlike the other platforms, the Linux profile is also published under a versioned name that depends on the
architecture and the `glibc` version of your machine, for example `linux-x86_64-glibc-2_40` on an `x86_64` host or
`linux-arm64-glibc-2_40` on an `arm64` host. Both names select the same toolchain, so use `--list-profiles` to see the
versioned name available locally when you need to state the `glibc` version explicitly.

The build downloads `FFmpeg`, `RapidJSON` and enabled external libraries when they are not already available locally.
Nix provides the compiler toolchain (`clang`/`llvm`), `pkg-config` inputs and the build tools.

#### 2.1 Prerequisites

Linux builds require the following tools.

- **Nix** — the `default` profile supplies the `clang`/`llvm` toolchain, `pkg-config` and the build packages.

#### 2.2 Options

Use `--enable-<library name>` flag to support additional external or system libraries and
`--disable-<architecture name>` (`--disable-arm64`, `--disable-x86-64`) to skip the architecture. Use `--enable-gpl` to
allow GPL-licensed libraries.

```
./nix-linux.sh -p default --enable-fontconfig
```

Run `--help` to see all available build options.

#### 2.3 Build Output

All libraries created can be found under the `prebuilt` directory.

- Headers and shared libraries are created under the `bundle-linux` folder.

### 3. Using

#### 3.1 Local Integration

Build it locally first, then integrate the generated artifacts from `prebuilt/bundle-linux`.

- Add the generated headers to your include path and link your application against the generated shared libraries.
- Link the `ffmpegkit` library together with every generated `FFmpeg` library.

#### 3.2 C++ API

1. Execute synchronous `FFmpeg` commands.

    ```C++
    #include <FFmpegKit.h>
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    auto session = FFmpegKit::execute("-i file1.mp4 -c:v mpeg4 file2.mp4");
    if (ReturnCode::isSuccess(session->getReturnCode())) {

        // SUCCESS

    } else if (ReturnCode::isCancel(session->getReturnCode())) {

        // CANCEL

    } else {

        // FAILURE
        std::cout << "Command failed with state " << FFmpegKitConfig::sessionStateToString(session->getState()) << " and rc " << session->getReturnCode() << "." << session->getFailStackTrace() << std::endl;

    }
    ```

2. Each `execute` call (sync or async) creates a new session. Access every detail about your execution from the
   session created.

    ```C++
    #include <FFmpegKit.h>
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    auto session = FFmpegKit::execute("-i file1.mp4 -c:v mpeg4 file2.mp4");

    // Unique session id created for this execution
    long sessionId = session->getSessionId();

    // Command arguments as a single string
    auto command = session->getCommand();

    // Command arguments
    auto arguments = session->getArguments();

    // State of the execution. Shows whether it is still running or completed
    SessionState state = session->getState();

    // Return code for completed sessions. Will be null if session is still running or ends with a failure
    auto returnCode = session->getReturnCode();

    auto startTime = session->getStartTime();
    auto endTime = session->getEndTime();
    long duration = session->getDuration();

    // Console output generated for this execution
    auto output = session->getOutput();

    // The stack trace if FFmpegKit fails to run a command
    auto failStackTrace = session->getFailStackTrace();

    // The list of logs generated for this execution
    auto logs = session->getLogs();

    // The list of statistics generated for this execution
    auto statistics = session->getStatistics();
    ```

3. Execute asynchronous `FFmpeg` commands by providing session specific `execute`/`log`/`session` callbacks.

    ```C++
    #include <FFmpegKit.h>
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    FFmpegKit::executeAsync("-i file1.mp4 -c:v mpeg4 file2.mp4", [](auto session) {
        const auto state = session->getState();
        auto returnCode = session->getReturnCode();

        // CALLED WHEN SESSION IS EXECUTED

        std::cout << "FFmpeg process exited with state " << FFmpegKitConfig::sessionStateToString(state) << " and rc " << returnCode << "." << session->getFailStackTrace() << std::endl;
    }, [](auto log) {

        // CALLED WHEN SESSION PRINTS LOGS

    }, [](auto statistics) {

        // CALLED WHEN SESSION GENERATES STATISTICS

    });
    ```

4. Execute `FFprobe` commands.

    - Synchronous

    ```C++
    #include <FFprobeKit.h>
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    auto session = FFprobeKit::execute(ffprobeCommand);

    if (!ReturnCode::isSuccess(session->getReturnCode())) {
        std::cout << "Command failed. Please check output for the details." << std::endl;
    }
    ```

    - Asynchronous

    ```C++
    #include <FFprobeKit.h>
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    FFprobeKit::executeAsync(ffprobeCommand, [](auto session) {

        // CALLED WHEN SESSION IS EXECUTED

    });
    ```

5. Get media information for a file.

    ```C++
    #include <FFprobeKit.h>

    using namespace ffmpegkit;

    auto mediaInformation = FFprobeKit::getMediaInformation("<file path or uri>");
    mediaInformation->getMediaInformation();
    ```

6. Stop ongoing `FFmpeg` operations.

    - Stop all executions
        ```C++
        FFmpegKit::cancel();
        ```
    - Stop a specific session
        ```C++
        FFmpegKit::cancel(sessionId);
        ```

7. Get previous `FFmpeg` and `FFprobe` sessions from session history.

    ```C++
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    auto sessions = FFmpegKitConfig::getSessions();
    int i = 0;
    std::for_each(sessions->begin(), sessions->end(), [](const auto session) {
        std::cout << "Session " << i++ << " = id:" << session->getSessionId() << ", startTime:" << session->getStartTime() << ", duration:" << session->getDuration() << ", state:" << FFmpegKitConfig::sessionStateToString(session->getState()) << ", returnCode:" << session->getReturnCode() << "." << std::endl;
    });
    ```

8. Enable global callbacks.

    - Session type specific Complete Callbacks, called when an async session has been completed

        ```C++
        #include <FFmpegKitConfig.h>

        using namespace ffmpegkit;

        FFmpegKitConfig::enableFFmpegSessionCompleteCallback([](auto session) {

        });

        FFmpegKitConfig::enableFFprobeSessionCompleteCallback([](auto session) {

        });

        FFmpegKitConfig::enableMediaInformationSessionCompleteCallback([](auto session) {

        });
        ```

    - Log Callback, called when a session generates logs

        ```C++
        #include <FFmpegKitConfig.h>

        using namespace ffmpegkit;

        FFmpegKitConfig::enableLogCallback([](auto log) {
            ...
        });
        ```

    - Statistics Callback, called when a session generates statistics

        ```C++
        #include <FFmpegKitConfig.h>

        using namespace ffmpegkit;

        FFmpegKitConfig::enableStatisticsCallback([](auto statistics) {
            ...
        });
        ```

9. Register system fonts and custom font directories.

    ```C++
    #include <FFmpegKitConfig.h>

    using namespace ffmpegkit;

    FFmpegKitConfig::setFontDirectoryList(std::list<std::string>{"/usr/share/fonts"}, std::map<std::string,std::string>());
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running the `Linux` test application developed under
the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.
