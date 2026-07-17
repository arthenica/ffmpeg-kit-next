# FFmpegKitNext for Web

This folder contains the WebAssembly-specific FFmpegKitNext wrapper source.
It is built by `scripts/web/ffmpeg-kit.sh` with Emscripten and is intentionally
separate from the Linux wrapper source.

The performance-sensitive implementation language is C/C++ compiled to
WebAssembly. FFmpeg itself is C, and keeping the wrapper boundary in C/C++ avoids
moving session execution, protocol callbacks and buffer handling into a slower
JS/Dart loop. JavaScript, TypeScript or Dart should sit above this as the browser
worker and package API layer.

Build from the repository root:

```bash
./nix-web.sh --jobs=8
```

For an FFmpeg-core-only build without the current C++ wrapper:

```bash
./nix-web.sh --disable-pthreads --skip-ffmpeg-kit --jobs=8
```

The current C++ wrapper uses pthreads, so full `ffmpeg-kit` web builds require
SharedArrayBuffer-compatible COOP/COEP headers at runtime.

## Consumer final-link contract

The web build installs static archives only (`.a`); it does not run a final
`emcc` link step. The consuming package is responsible for linking the FFmpeg
and `ffmpeg-kit` archives into the final `.js`/`.wasm` module. Because linker
flags must match how the archives were compiled, the consumer link command must
include at least the following:

- `-pthread` — must match how the archives were built. The archives ship with
  pthreads enabled by default (`--enable-pthreads`), so the final link must also
  pass `-pthread`. If the archives were built with `--disable-pthreads`, omit it.
- `-fwasm-exceptions` — must match the archives. The C++ wrapper is compiled with
  the new WebAssembly exception-handling model (`-fwasm-exceptions`), so the
  final link must use the same model. Do not mix it with legacy `-fexceptions`.
- `-sALLOW_MEMORY_GROWTH` — allow the WebAssembly heap to grow at runtime.
- `-sSTACK_SIZE` — raise the stack well above the Emscripten default; FFmpeg and
  the fftools code paths are stack-hungry (for example `-sSTACK_SIZE=5MB`).
- `-sINITIAL_MEMORY` / `-sMAXIMUM_MEMORY` — size the initial and maximum heap for
  the media you intend to process.
- `-sMODULARIZE` / `-sEXPORT_ES6` / `-sEXPORT_NAME` — emit a modular ES6 factory
  with a stable export name so the module can be imported and instantiated by the
  package API layer.
- `-sFORCE_FILESYSTEM` — ensure the Emscripten filesystem is linked in. For large
  media, mount `WORKERFS` (workers) or `OPFS` rather than copying whole files into
  `MEMFS`.

When threads are enabled, the final application must also be served with the
`SharedArrayBuffer`-enabling response headers:

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`

These flags and headers are emscripten-version-sensitive; confirm them against the
`emcc --version` provided by the pinned web devShell.
