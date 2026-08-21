# FFmpegKitNext for Web

### 1. Features
- Supports
    - Modern browsers with `WebAssembly`, `SharedArrayBuffer` and module `Web Worker` support
    - `wasm32` architecture (`wasm32-unknown-emscripten`), compiled with `Emscripten`
    - `WebAssembly SIMD`, with `pthreads` enabled by default
- Runs the `FFmpeg` and `ffmpeg-kit` core inside a `Web Worker`, off the main (UI) thread
- Native-compatible JavaScript API with TypeScript declarations, shipped as ES modules
- Custom `FFmpegKit` protocols: `ffkitmem:` for finite in-memory input/output and `ffkitstream:` for memory-backed streaming input/output
- Virtual filesystem helpers: `writeFile`/`readFile` (`MEMFS`) and `mount` (`WORKERFS`, for large inputs without a
  heap copy)
- Supports both web linkage modes: dynamic builds load FFmpeg side modules at runtime, while static builds link the FFmpeg libraries into one main WebAssembly module

### 2 Building

Web builds use the [Nix package manager](https://github.com/arthenica/ffmpeg-kit-next/wiki/Nix). You must install it on a supported host first to build the binaries. Then run the
`nix-web.sh` wrapper from the project root.

NixOS is not required. The scripts require the Nix package manager on a supported host, and NixOS is not a supported Web build host.

Note that, `FFmpegKitNext` does not publish binaries and building it yourself is the only way to use it.

Use `--list-profiles` to see the local Nix profiles available on your machine.

```sh
./nix-web.sh --list-profiles
```

The current web profile is `web-wasm32-emscripten`.

```sh
./nix-web.sh -p web-wasm32-emscripten
```

This command compiles the native `FFmpeg` and `ffmpeg-kit` sources to WebAssembly with Emscripten and packages
them, together with the JavaScript/TypeScript API and the worker runtime, into a local npm package.

The build downloads `FFmpeg` and enabled external libraries when they are not already available locally. Nix
provides Emscripten (emsdk), CMake and the build tools.

#### 2.1 Prerequisites

Web builds require the following tools.

- **Nix package manager** — the `web-wasm32-emscripten` profile supplies the Emscripten and CMake.

#### 2.2 Build Variants and External Libraries

`FFmpeg` includes built-in encoders for some popular formats. Some formats/codecs require external libraries to be
enabled in the native build. For example, `mp3` encoding needs `lame` or `shine`, `h264` needs `x264`, and
`vp8`/`vp9` needs `libvpx`.

Use the `--enable-lib-<library name>` flag to support additional external or system libraries. Use `--enable-gpl` to
allow GPL-licensed libraries.

```sh
./nix-web.sh -p web-wasm32-emscripten --enable-lib-fontconfig --enable-lib-freetype
```

Run `--help` to see all available build options.

Dynamic linkage is the default. Add `--static` to build static archives and link the FFmpeg libraries into one main
WebAssembly module.

```sh
./nix-web.sh -p web-wasm32-emscripten --static
```

The C++ wrapper uses `pthreads`, so full `ffmpeg-kit` web builds keep `--enable-pthreads`.

#### 2.3 Build Output

All libraries created can be found under the `prebuilt` directory. The web build produces a local npm package
under `prebuilt/bundle-web-wasm32/ffmpeg-kit-next/`:

```
prebuilt/bundle-web-wasm32/ffmpeg-kit-next/
├── package.json         # name: ffmpeg-kit-next-web
├── dist/                # ES module API + TypeScript declarations
└── lib/                 # libffmpegkit.js / libffmpegkit.wasm + FFmpeg modules
```

`dist/` is the JavaScript/TypeScript API layer and `lib/` holds the linked `libffmpegkit.wasm` module together
with the `FFmpeg` modules it loads at runtime. The worker and the `.wasm` assets are resolved relative to the
module via `import.meta.url`, so the whole package folder must be served as static assets and `lib/` must be
deployed in full.

#### 2.4 Platform Support

`ffmpeg-kit-next-web` requires a browser with `WebAssembly`, module `Web Worker` and — because `pthreads` are
enabled — `SharedArrayBuffer` support. `SharedArrayBuffer` is only exposed to cross-origin isolated pages, so the
hosting page must be served with both of the following response headers:

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`

Without them the module fails to instantiate.

### 3. Using

`ffmpeg-kit-next-web` is not published to `npm`. Build `FFmpegKitNext` locally for the web target, then integrate
the generated package from this repository using a local file dependency.

#### 3.1 Local Integration

Build the package locally first, then depend on the generated folder.

```json
{
  "dependencies": {
    "ffmpeg-kit-next-web": "file:<path-to-repo>/prebuilt/bundle-web-wasm32/ffmpeg-kit-next"
  }
}
```

Adjust the path to match where this repository is located relative to your application. Do not install this
package from the npm registry — the package and the wasm module are expected to come from your local build.

App code imports only from the package entry point; the `Worker`, the raw wasm module and the internal factory
are never exposed.

```js
import { FFmpegKit, FFprobeKit, FFmpegKitConfig } from 'ffmpeg-kit-next-web';
```

#### 3.2 JavaScript API

1. Execute FFmpeg commands.

    ```js
    import { FFmpegKit, ReturnCode } from 'ffmpeg-kit-next-web';

    FFmpegKit.execute('-i file1.mp4 -c:v mpeg4 file2.mp4').then((session) => {
      const returnCode = session.getReturnCode();

      if (ReturnCode.isSuccess(returnCode)) {

        // SUCCESS

      } else if (ReturnCode.isCancel(returnCode)) {

        // CANCEL

      } else {

        // ERROR

      }
    });
    ```

2. Each `execute` call creates a new session. Access every detail about your execution from the session created.

    ```js
    FFmpegKit.execute('-i file1.mp4 -c:v mpeg4 file2.mp4').then(async (session) => {

      // Unique session id created for this execution
      const sessionId = session.getSessionId();

      // Command arguments as a single string
      const command = session.getCommand();

      // Command arguments
      const commandArguments = session.getArguments();

      // State of the execution. Shows whether it is still running or completed
      const state = session.getState();

      // Return code for completed sessions. Will be null if session is still running or FFmpegKit fails to run it
      const returnCode = session.getReturnCode();

      const createTime = session.getCreateTime();
      const startTime = session.getStartTime();
      const endTime = session.getEndTime();
      const duration = session.getDuration();

      // Console output generated for this execution
      const output = await session.getOutput();

      // The stack trace if FFmpegKit fails to run a command
      const failStackTrace = session.getFailStackTrace();

      // The list of logs delivered for this execution
      const logs = session.getLogs();

      // The list of statistics delivered for this execution (only available on FFmpegSession)
      const statistics = session.getStatistics();

    });
    ```

   The getters above report what this binding already holds, so they return immediately. Use the `getAll` variants
   when you need the complete native record, including messages that are still in transit — they wait for those
   messages (5000 ms by default) before answering.

    ```js
    const allLogs = await session.getAllLogs();
    const allLogsAsString = await session.getAllLogsAsString(3000); // custom timeout in milliseconds
    const allStatistics = await session.getAllStatistics();
    ```

3. Execute `FFmpeg` commands by providing session specific `execute`/`log`/`session` callbacks. Unlike `execute`,
   `executeAsync` resolves as soon as the execution **starts** — the awaited promise gives you the running
   session, not the finished one. This matches the Flutter and React Native plugins.

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
    FFprobeKit.execute(ffprobeCommand).then((session) => {

      // CALLED WHEN SESSION IS EXECUTED

    });
    ```

5. Get media information for a file.

    ```js
    FFprobeKit.getMediaInformation('file1.mp4').then(async (session) => {
      const information = session.getMediaInformation();

      if (information === null) {

        // CHECK THE FOLLOWING ATTRIBUTES ON ERROR
        const state = FFmpegKitConfig.sessionStateToString(session.getState());
        const returnCode = session.getReturnCode();
        const failStackTrace = session.getFailStackTrace();
        const duration = session.getDuration();
        const output = await session.getOutput();
      }
    });
    ```

6. Stop ongoing FFmpeg operations.

   On web both `execute` and `executeAsync` run native FFmpeg asynchronously inside the worker, so cancel requests
   are processed while native FFmpeg is still running. The two differ only in when their promises resolve.

  - Stop all sessions
    ```js
    FFmpegKit.cancel();
    ```
  - Stop a specific session
    ```js
    FFmpegKit.cancel(sessionId);
    ```

7. (Web) Make input files available to the module's virtual filesystem, and read outputs back out of it. Write
   bytes into `MEMFS`, or mount large `File`/`Blob` inputs read-only through `WORKERFS` (no heap copy).

    ```js
    import { writeFile, readFile, mount } from 'ffmpeg-kit-next-web';

    // Small inputs: copy bytes into MEMFS.
    await writeFile('file1.mp4', new Uint8Array(await file.arrayBuffer()));

    // Large inputs: mount a File read-only; FFmpeg reads it by path.
    await mount('/mnt', { files: [file] }); // then use `/mnt/<file.name>` as -i

    // Outputs: read the produced bytes back. Resolves null when the path does not exist.
    const bytes = await readFile('file2.mp4');
    const url = URL.createObjectURL(new Blob([bytes], { type: 'video/mp4' }));
    ```

8. (Web) Use in-memory (`ffkitmem:`) or streaming (`ffkitstream:`) I/O to avoid staging files in `MEMFS`. Pass
   `getUrl()` as an `-i` input or as an output target.

    ```js
    import { FFmpegKit, FFmpegKitInputBuffer, FFmpegKitOutputBuffer } from 'ffmpeg-kit-next-web';

    const input = await FFmpegKitInputBuffer.fromByteArray(bytes, 'mp4');
    const output = await FFmpegKitOutputBuffer.create('mp4');

    await FFmpegKit.execute(`-i ${input.getUrl()} -c:v mpeg4 ${output.getUrl()}`);

    const result = await output.toByteArray();
    await input.close();
    await output.close();
    ```

   `FFmpegKitStreamInput` and `FFmpegKitStreamOutput` cover the incremental case. Both are non-blocking: `write`
   resolves with the number of bytes accepted (possibly fewer than offered, or 0, when the ring buffer is full),
   and `read` resolves null when nothing is ready yet and an empty array at end of stream. Pump them while the
   command runs, which means pairing them with `executeAsync`.

    ```js
    import { FFmpegKitStreamInput } from 'ffmpeg-kit-next-web';

    const stream = await FFmpegKitStreamInput.create('mp4');
    await FFmpegKit.executeAsync(`-i ${stream.getUrl()} -c:v mpeg4 file2.mp4`);

    let offset = 0;
    while (offset < bytes.length) {
      offset += await stream.write(bytes.subarray(offset));
    }
    await stream.closeInput();
    ```

9. Get previous `FFmpeg`, `FFprobe` and `MediaInformation` sessions from the session history.

    ```js
    FFmpegKit.listSessions().then(sessionList => {
      sessionList.forEach(session => {
        const sessionId = session.getSessionId();
      });
    });

    FFprobeKit.listFFprobeSessions().then(sessionList => {
      sessionList.forEach(session => {
        const sessionId = session.getSessionId();
      });
    });

    FFprobeKit.listMediaInformationSessions().then(sessionList => {
      sessionList.forEach(session => {
        const sessionId = session.getSessionId();
      });
    });
    ```

   `FFmpegKitConfig.getSessions()`, `getSession(sessionId)`, `getLastSession()`, `getLastCompletedSession()` and
   `getSessionsByState(state)` read the same history and are asynchronous as well.

10. Enable global callbacks.

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

   Calling any of these with no argument clears the callback. `FFmpegKitConfig.setLogLevel()` updates the
   JavaScript-side cache immediately, but native `FFmpeg`/`FFprobe` reads the configured level when a run starts,
   so changing it mid-run may not affect native filtering for that already-running command.

11. Register custom font directories.

    ```js
    FFmpegKitConfig.setFontDirectoryList(['<folder with fonts>']);
    ```

12. Boot and tear down the wasm runtime explicitly. Initialization is optional: any API that needs the
    module boots it lazily on first use. Call `init` only when you want to choose the boot moment yourself, or to
    suppress the native load confirmation — in which case it must run before any other `FFmpegKit` call.

    ```js
    await FFmpegKitConfig.init();

    // To suppress the native "Loaded ffmpeg-kit-next..." line:
    // await FFmpegKitConfig.init(false);

    // Terminates the worker and releases the wasm heap. A later call boots a fresh runtime.
    await FFmpegKitConfig.uninit();
    ```

### 4. Test Application

You can see how `FFmpegKitNext` is used inside an application by running the `Web` test application developed
under the [FFmpegKitNext Test](https://github.com/arthenica/ffmpeg-kit-next-test) project.
