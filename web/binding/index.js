// Public entry point for the FFmpegKitNext web package. App code imports only from
// here; it never sees the Worker, the raw embind Module, or FFmpegKitFactory.
//
// Native-named public API (identical to the other platforms):
export { FFmpegKit } from './FFmpegKit.js';
export { FFprobeKit } from './FFprobeKit.js';
export { FFmpegKitConfig } from './FFmpegKitConfig.js';
export {
  AbstractSession,
  FFmpegSession,
  FFprobeSession,
  MediaInformationSession,
} from './session.js';
export {
  ReturnCode,
  Log,
  Statistics,
  MediaInformation,
  StreamInformation,
} from './models.js';
export { SessionState, Level } from './enums.js';
export {
  FFmpegKitInputBuffer,
  FFmpegKitOutputBuffer,
  FFmpegKitStreamInput,
  FFmpegKitStreamOutput,
} from './io.js';

import { getFactory } from './FFmpegKitFactory.js';

// Web-only bootstrap: the wasm module loads asynchronously, so callers await this
// once before using the API. Resolves with { version, ffmpegVersion, buildDate }.
export function init() {
  return getFactory().ready();
}

// Web-only virtual-filesystem helpers. The module's MEMFS lives inside the worker;
// these move bytes across for inputs/outputs (no native-platform equivalent).
export function writeFile(path, data) {
  return getFactory().writeFile(path, data);
}
export function readFile(path) {
  return getFactory().readFile(path);
}

// Web-only: mount File/Blob inputs read-only via WORKERFS at `mountPoint`, so FFmpeg
// reads them by path without copying into the wasm heap — preferred for large inputs.
// `files` are File objects; `blobs` are `{ name, data: Blob }`.
export function mount(mountPoint, options = {}) {
  return getFactory().mount(mountPoint, options);
}
