// Public enums, mirroring the native FFmpegKitNext API. Values match the C++
// ffmpegkit::SessionState and ffmpegkit::Level enums so log levels compare
// directly against the raw integers the module reports.

export const SessionState = Object.freeze({
  CREATED: 0,
  RUNNING: 1,
  FAILED: 2,
  COMPLETED: 3,
});

export const Level = Object.freeze({
  AV_LOG_STDERR: -16,
  AV_LOG_QUIET: -8,
  AV_LOG_PANIC: 0,
  AV_LOG_FATAL: 8,
  AV_LOG_ERROR: 16,
  AV_LOG_WARNING: 24,
  AV_LOG_INFO: 32,
  AV_LOG_VERBOSE: 40,
  AV_LOG_DEBUG: 48,
  AV_LOG_TRACE: 56,
});
