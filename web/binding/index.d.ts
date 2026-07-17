// Type declarations for the FFmpegKitNext web (WebAssembly) binding layer.
//
// The runtime is plain ES modules (index.js and friends) served statically — there
// is no TypeScript build step. This file only describes the public API for editor
// IntelliSense and type-checking, mirroring how the React Native plugin ships
// `main: index.js` + `types: index.d.ts`.
//
// The public surface uses the same class names as the native platforms. Everything
// is asynchronous on web because the wasm module runs inside a Web Worker.

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

/** Receives a log entry as an FFmpeg/FFprobe session produces it. */
export type LogCallback = (log: Log) => void;

/** Receives a statistics entry as an FFmpeg session produces it. */
export type StatisticsCallback = (statistics: Statistics) => void;

// ---------------------------------------------------------------------------
// Enums (numeric values match the native ffmpegkit::SessionState / ffmpegkit::Level)
// ---------------------------------------------------------------------------

export declare const SessionState: {
  readonly CREATED: 0;
  readonly RUNNING: 1;
  readonly FAILED: 2;
  readonly COMPLETED: 3;
};

export declare const Level: {
  readonly AV_LOG_STDERR: -16;
  readonly AV_LOG_QUIET: -8;
  readonly AV_LOG_PANIC: 0;
  readonly AV_LOG_FATAL: 8;
  readonly AV_LOG_ERROR: 16;
  readonly AV_LOG_WARNING: 24;
  readonly AV_LOG_INFO: 32;
  readonly AV_LOG_VERBOSE: 40;
  readonly AV_LOG_DEBUG: 48;
  readonly AV_LOG_TRACE: 56;
};

// ---------------------------------------------------------------------------
// Value types
// ---------------------------------------------------------------------------

export declare class ReturnCode {
  static readonly SUCCESS: 0;
  static readonly CANCEL: 255;
  getValue(): number;
  isValueSuccess(): boolean;
  isValueError(): boolean;
  isValueCancel(): boolean;
}

export declare class Log {
  getSessionId(): number;
  getLevel(): number;
  getMessage(): string;
}

export declare class Statistics {
  getSessionId(): number;
  getVideoFrameNumber(): number;
  getVideoFps(): number;
  getVideoQuality(): number;
  getSize(): number;
  getTime(): number;
  getBitrate(): number;
  getSpeed(): number;
}

export declare class StreamInformation {
  getIndex(): number | null;
  getType(): string | null;
  getCodec(): string | null;
  getCodecLong(): string | null;
  getFormat(): string | null;
  getWidth(): number | null;
  getHeight(): number | null;
  getBitrate(): string | null;
  getSampleRate(): string | null;
  getChannelLayout(): string | null;
}

export declare class MediaInformation {
  getFilename(): string | null;
  getFormat(): string | null;
  getLongFormat(): string | null;
  getDuration(): string | null;
  getStartTime(): string | null;
  getSize(): string | null;
  getBitrate(): string | null;
  getStreams(): StreamInformation[];
}

// ---------------------------------------------------------------------------
// Sessions (produced by FFmpegKit/FFprobeKit — not constructed directly)
// ---------------------------------------------------------------------------

export declare class AbstractSession {
  /** The session id, assigned once the run has started (null before then). */
  getSessionId(): number | null;
  getCommand(): string;
  /** One of the `SessionState` values. */
  getState(): number;
  getReturnCode(): ReturnCode | null;
  getDuration(): number;
  getOutput(): string;
  getAllLogsAsString(): string;
  getLogs(): Log[];
  getFailStackTrace(): string | null;
  getLogCallback(): LogCallback | null;
  isFFmpeg(): boolean;
  isFFprobe(): boolean;
  isMediaInformation(): boolean;
}

export declare class FFmpegSession extends AbstractSession {
  getStatistics(): Statistics[];
  getAllStatistics(): Statistics[];
  getLastReceivedStatistics(): Statistics | null;
  getStatisticsCallback(): StatisticsCallback | null;
}

export declare class FFprobeSession extends AbstractSession {}

export declare class MediaInformationSession extends AbstractSession {
  getMediaInformation(): MediaInformation | null;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

export declare class FFmpegKit {
  /**
   * Runs an FFmpeg command on a worker thread. The promise resolves with the
   * populated session once the command completes; `logCallback` and
   * `statisticsCallback` fire live as it runs.
   */
  static executeAsync(
    command: string,
    logCallback?: LogCallback | null,
    statisticsCallback?: StatisticsCallback | null
  ): Promise<FFmpegSession>;

  /**
   * Runs an FFmpeg command synchronously inside the worker. Live callbacks fire in
   * one batch on completion (the module thread is blocked during a synchronous run);
   * prefer `executeAsync` for live progress.
   */
  static execute(
    command: string,
    logCallback?: LogCallback | null,
    statisticsCallback?: StatisticsCallback | null
  ): Promise<FFmpegSession>;

  /** Cancels the given session, or all running sessions when no id is passed. */
  static cancel(sessionId?: number | null): Promise<void>;

  /** All FFmpeg sessions still in the history. */
  static listSessions(): FFmpegSession[];
}

export declare class FFprobeKit {
  /** Runs an FFprobe command. */
  static execute(command: string): Promise<FFprobeSession>;

  /** Extracts media information for a path already present in the virtual filesystem. */
  static getMediaInformation(path: string): Promise<MediaInformationSession>;

  /** All FFprobe sessions still in the history. */
  static listFFprobeSessions(): FFprobeSession[];
}

export declare class FFmpegKitConfig {
  /** Registers a global log callback (pass null to clear). */
  static enableLogCallback(logCallback: LogCallback | null): void;

  /** Registers a global statistics callback (pass null to clear). */
  static enableStatisticsCallback(statisticsCallback: StatisticsCallback | null): void;

  /** Sets the active log level (a `Level` value). */
  static setLogLevel(level: number): Promise<void>;

  /** Points fontconfig at a directory containing a `fonts.conf` file. */
  static setFontconfigConfigurationPath(path: string): Promise<void>;

  /** Registers fonts in one directory for FFmpeg filters. */
  static setFontDirectory(
    fontDirectoryPath: string,
    fontNameMapping?: Record<string, string>
  ): Promise<void>;

  /** Registers fonts in directories for FFmpeg filters. */
  static setFontDirectoryList(
    fontDirectoryList: string[],
    fontNameMapping?: Record<string, string>
  ): Promise<void>;

  /** FFmpegKitNext version (available after `init()` resolves). */
  static getVersion(): string | null;
  /** Bundled FFmpeg version (available after `init()` resolves). */
  static getFFmpegVersion(): string | null;
  /** Build date (available after `init()` resolves). */
  static getBuildDate(): string | null;

  // ---- Session history --------------------------------------------------------
  static getSessions(): AbstractSession[];
  static getSession(sessionId: number): AbstractSession | null;
  static getLastSession(): AbstractSession | null;
  static getLastCompletedSession(): AbstractSession | null;
  static getSessionsByState(state: number): AbstractSession[];
  static getSessionHistorySize(): number;
  static setSessionHistorySize(sessionHistorySize: number): void;
  static clearSessions(): void;
}

// ---------------------------------------------------------------------------
// ffkitmem:/ffkitstream: in-memory I/O (avoid staging files in MEMFS)
// ---------------------------------------------------------------------------

/** Seekable in-memory input built from a byte array. Use getUrl() as an -i input. */
export declare class FFmpegKitInputBuffer {
  static fromByteArray(data: Uint8Array, extension?: string): Promise<FFmpegKitInputBuffer>;
  getUrl(): string;
  close(): Promise<void>;
}

/** Seekable in-memory output; read the bytes with toByteArray() after the command. */
export declare class FFmpegKitOutputBuffer {
  static create(
    extension?: string,
    options?: { initialCapacity?: number; maxCapacity?: number }
  ): Promise<FFmpegKitOutputBuffer>;
  getUrl(): string;
  toByteArray(): Promise<Uint8Array>;
  close(): Promise<void>;
}

/** Non-seekable streaming input; pump write() while a command consumes it. */
export declare class FFmpegKitStreamInput {
  static create(extension?: string, options?: { capacity?: number }): Promise<FFmpegKitStreamInput>;
  getUrl(): string;
  /** Non-blocking; resolves with the number of bytes accepted (may be short). */
  write(data: Uint8Array): Promise<number>;
  closeInput(): Promise<void>;
  close(): Promise<void>;
}

/** Non-seekable streaming output; pump read() while a command produces it. */
export declare class FFmpegKitStreamOutput {
  static create(extension?: string, options?: { capacity?: number }): Promise<FFmpegKitStreamOutput>;
  getUrl(): string;
  /** Non-blocking; bytes, an empty array at EOF, or null when nothing is ready yet. */
  read(maxBytes: number): Promise<Uint8Array | null>;
  close(): Promise<void>;
}

// ---------------------------------------------------------------------------
// Web-only helpers
// ---------------------------------------------------------------------------

/** Module version/build metadata reported once the wasm module is ready. */
export interface FFmpegKitInfo {
  version: string | null;
  ffmpegVersion: string | null;
  buildDate: string | null;
}

/**
 * Boots the wasm module (loaded in a Web Worker) and resolves once its bindings are
 * verified. Must be awaited once before using the API.
 */
export declare function init(): Promise<FFmpegKitInfo>;

/** Writes bytes into the module's virtual filesystem (MEMFS). */
export declare function writeFile(path: string, data: Uint8Array): Promise<void>;

/** Reads a file from the module's virtual filesystem, or null if it doesn't exist. */
export declare function readFile(path: string): Promise<Uint8Array | null>;

/** A named blob for a WORKERFS mount. */
export interface WorkerFsBlob {
  name: string;
  data: Blob;
}

export interface MountOptions {
  files?: File[];
  blobs?: WorkerFsBlob[];
}

/**
 * Mounts File/Blob inputs read-only via WORKERFS at `mountPoint`, so FFmpeg reads
 * them by path without copying into the wasm heap — preferred for large inputs.
 */
export declare function mount(mountPoint: string, options?: MountOptions): Promise<void>;
