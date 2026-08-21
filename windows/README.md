# FFmpegKitNext for Windows

`FFmpegKitNext` for Windows is built natively with the [MSYS2](https://www.msys2.org/) /
[MinGW-w64](https://www.mingw-w64.org/) toolchain and integrated locally.

### 1. Features
- Provides a `C++` API built with `c++11` and a `C` API
- Usable from both `MinGW-w64` and `MSVC` / `clang-cl`
- Includes `arm64`; `x86-64` is implemented but not yet tested
- Libraries are compiled natively, so only the architecture of the host machine is built
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Builds shared native libraries (`.dll`)

### 2. Building

Windows builds run from an MSYS2 shell using the `windows.sh` wrapper in the project root. Nix is not
used on Windows because it does not provide a native Windows build toolchain.

Note that, `FFmpegKitNext` does not publish binaries and building it yourself is the only way to use it.

```
./windows.sh
```

This command compiles the native `FFmpeg` and `ffmpeg-kit` shared libraries together with the `C++`
API for the architecture of the host machine. Building `arm64` libraries requires an `arm64` Windows
host.

The build downloads `FFmpeg` and `RapidJSON` when they are not already available locally.

#### 2.1 Prerequisites

Windows builds require an [MSYS2](https://www.msys2.org/) shell with the MinGW-w64 toolchain. On an
`arm64` host, use the `CLANGARM64` environment and install the build tools:

```bash
pacman -S --needed ${MINGW_PACKAGE_PREFIX}-clang ${MINGW_PACKAGE_PREFIX}-pkgconf make autoconf \
  automake libtool git curl rsync pkg-config gperf ${MINGW_PACKAGE_PREFIX}-meson \
  ${MINGW_PACKAGE_PREFIX}-ninja gtk-doc autogen bison gettext-devel
```

See [Windows Prerequisites](https://github.com/arthenica/ffmpeg-kit-next/wiki/Windows-Prerequisites)
for details.

#### 2.2 Options

Use `--enable-<library name>` to build with an external library and `--full` to enable all of them.
Use `--disable-arm64` / `--disable-x86-64` to skip an architecture. Use `--enable-gpl` to allow
GPL-licensed libraries.

```
./windows.sh --enable-openssl --enable-dav1d
```

The MinGW-w64 runtime can be linked statically into the DLL (default) or shipped as separate DLLs in
the bundle; see the [wiki](https://github.com/arthenica/ffmpeg-kit-next/wiki/Building-Windows) for
details.

Run `--help` to see all available build options.

#### 2.3 Build Output

All libraries created can be found under the `prebuilt` directory.

- Headers, DLLs, import libraries and package config files are created under the `bundle-windows`
  folder.

### 3. Using

#### 3.1 Local Integration

Build it locally first, then integrate the generated artifacts from `prebuilt/bundle-windows`.

- Add the generated headers to your include path and link your application against the generated
  import libraries.
- Link the `ffmpegkit` library together with every generated `FFmpeg` library.
- Ship the runtime DLLs from `bin` next to your application (or on `PATH`).

See [Using FFmpegKitNext on Windows](https://github.com/arthenica/ffmpeg-kit-next/wiki/Using-FFmpegKitNext-on-Windows)
for linking from `MinGW-w64` and `MSVC`.

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

    FFmpegKitConfig::setFontDirectoryList(std::list<std::string>{"C:\\Windows\\Fonts"}, std::map<std::string,std::string>());
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running the `Windows` test application developed under
the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.
