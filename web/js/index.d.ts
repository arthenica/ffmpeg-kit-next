/*
 * Copyright (c) 2026 Taner Sener
 *
 * This file is part of FFmpegKitNext.
 *
 * FFmpegKitNext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKitNext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General License for more details.
 *
 * You should have received a copy of the GNU Lesser General License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

// Type declarations for the FFmpegKitNext web (WebAssembly) binding layer.
//
// The runtime is plain ES modules (index.js and friends) served statically.
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

export interface StatisticsProperties {
    sessionId?: number;
    frame?: number;
    videoFrameNumber?: number;
    fps?: number;
    videoFps?: number;
    quality?: number;
    videoQuality?: number;
    size?: number;
    time?: number;
    bitrate?: number;
    speed?: number;
}

/** Receives the populated session once an asynchronous execution completes. */
export type FFmpegSessionCompleteCallback = (session: FFmpegSession) => void;

/** Receives the populated FFprobe session once an asynchronous execution completes. */
export type FFprobeSessionCompleteCallback = (session: FFprobeSession) => void;

/** Receives the populated media-information session once extraction completes. */
export type MediaInformationSessionCompleteCallback = (
    session: MediaInformationSession
) => void;

// ---------------------------------------------------------------------------
// Enums (numeric values match the native ffmpegkit::SessionState / ffmpegkit::Level)
// ---------------------------------------------------------------------------

export declare const SessionState: {
    readonly CREATED: 0;
    readonly RUNNING: 1;
    readonly FAILED: 2;
    readonly COMPLETED: 3;
};

/**
 * Log level constants. Levels are plain numbers throughout this API — `Log.getLevel()`
 * and `FFmpegKitConfig.getLogLevel()` both return one of the values below — so this
 * class is a constant holder and is not constructible.
 */
export declare class Level {
    static readonly AV_LOG_STDERR: -16;
    static readonly AV_LOG_QUIET: -8;
    static readonly AV_LOG_PANIC: 0;
    static readonly AV_LOG_FATAL: 8;
    static readonly AV_LOG_ERROR: 16;
    static readonly AV_LOG_WARNING: 24;
    static readonly AV_LOG_INFO: 32;
    static readonly AV_LOG_VERBOSE: 40;
    static readonly AV_LOG_DEBUG: 48;
    static readonly AV_LOG_TRACE: 56;

    static levelToString(level: number): string;
}

/** Controls whether a session's logs are also printed to the console. */
export declare const LogRedirectionStrategy: {
    readonly ALWAYS_PRINT_LOGS: 0;
    readonly PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED: 1;
    readonly PRINT_LOGS_WHEN_GLOBAL_CALLBACK_NOT_DEFINED: 2;
    readonly PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED: 3;
    readonly NEVER_PRINT_LOGS: 4;
};

/** Detects the running architecture. */
export declare class ArchDetect {
    /** Returns architecture name loaded. */
    static getArch(): Promise<string>;
}

/** Helper class to extract binary package information. */
export declare class Packages {
    /** Returns the FFmpegKit binary package name. */
    static getPackageName(): Promise<string>;

    /** Returns enabled external libraries by FFmpeg. */
    static getExternalLibraries(): Promise<string[]>;
}

// ---------------------------------------------------------------------------
// Value types
// ---------------------------------------------------------------------------

/** Return code produced by an FFmpegKit session. */
export declare class ReturnCode {
    static readonly SUCCESS: 0;
    static readonly CANCEL: 255;

    /**
     * Creates a return-code wrapper.
     *
     * @param value numeric return code produced by FFmpegKit
     */
    constructor(value: number);

    /**
     * Tests whether the supplied return code represents a successful execution.
     *
     * @returns true when the return code value is ReturnCode.SUCCESS
     */
    static isSuccess(returnCode: ReturnCode | null | undefined): boolean;

    /**
     * Tests whether the supplied return code represents a cancelled execution.
     *
     * @returns true when the return code value is ReturnCode.CANCEL
     */
    static isCancel(returnCode: ReturnCode | null | undefined): boolean;

    /** @returns numeric return-code value */
    getValue(): number;

    /** @returns true when this return-code value is ReturnCode.SUCCESS */
    isValueSuccess(): boolean;

    /** @returns true when this return-code value is neither success nor cancel */
    isValueError(): boolean;

    /** @returns true when this return-code value is ReturnCode.CANCEL */
    isValueCancel(): boolean;

    /** @returns decimal string representation of the return-code value */
    toString(): string;
}

/** Log entry produced by an FFmpegKit session. */
export declare class Log {
    /**
     * Creates a log entry.
     *
     * @param sessionId session that produced the log
     * @param level FFmpeg log level value
     * @param message log message text
     */
    constructor(sessionId: number, level: number, message: string);

    /** @returns id of the session that produced this log entry */
    getSessionId(): number;

    /** @returns FFmpeg log level value */
    getLevel(): number;

    /** @returns log message text */
    getMessage(): string;
}

export declare class Statistics {
    constructor(properties: StatisticsProperties);

    constructor(
        sessionId: number,
        videoFrameNumber: number,
        videoFps: number,
        videoQuality: number,
        size: number,
        time: number,
        bitrate: number,
        speed: number
    );

    getSessionId(): number;

    setSessionId(sessionId: number): void;

    getVideoFrameNumber(): number;

    setVideoFrameNumber(videoFrameNumber: number): void;

    getVideoFps(): number;

    setVideoFps(videoFps: number): void;

    getVideoQuality(): number;

    setVideoQuality(videoQuality: number): void;

    getSize(): number;

    setSize(size: number): void;

    getTime(): number;

    setTime(time: number): void;

    getBitrate(): number;

    setBitrate(bitrate: number): void;

    getSpeed(): number;

    setSpeed(speed: number): void;
}

export declare class StreamInformation {
    static readonly KEY_INDEX: string;
    static readonly KEY_TYPE: string;
    static readonly KEY_CODEC: string;
    static readonly KEY_CODEC_LONG: string;
    static readonly KEY_FORMAT: string;
    static readonly KEY_WIDTH: string;
    static readonly KEY_HEIGHT: string;
    static readonly KEY_BIT_RATE: string;
    static readonly KEY_SAMPLE_RATE: string;
    static readonly KEY_SAMPLE_FORMAT: string;
    static readonly KEY_CHANNEL_LAYOUT: string;
    static readonly KEY_SAMPLE_ASPECT_RATIO: string;
    static readonly KEY_DISPLAY_ASPECT_RATIO: string;
    static readonly KEY_AVERAGE_FRAME_RATE: string;
    static readonly KEY_REAL_FRAME_RATE: string;
    static readonly KEY_TIME_BASE: string;
    static readonly KEY_CODEC_TIME_BASE: string;
    static readonly KEY_TAGS: string;

    constructor(properties: Record<string, any> | null);

    getIndex(): number | null;

    getType(): string | null;

    getCodec(): string | null;

    getCodecLong(): string | null;

    getFormat(): string | null;

    getWidth(): number | null;

    getHeight(): number | null;

    getBitrate(): string | null;

    getSampleRate(): string | null;

    getSampleFormat(): string | null;

    getChannelLayout(): string | null;

    getSampleAspectRatio(): string | null;

    getDisplayAspectRatio(): string | null;

    getAverageFrameRate(): string | null;

    getRealFrameRate(): string | null;

    getTimeBase(): string | null;

    getCodecTimeBase(): string | null;

    getTags(): Record<string, any> | null;

    getStringProperty(key: string): string | null;

    getNumberProperty(key: string): number | null;

    getProperty(key: string): any;

    getAllProperties(): Record<string, any> | null;
}

export declare class Chapter {
    static readonly KEY_ID: string;
    static readonly KEY_TIME_BASE: string;
    static readonly KEY_START: string;
    static readonly KEY_START_TIME: string;
    static readonly KEY_END: string;
    static readonly KEY_END_TIME: string;
    static readonly KEY_TAGS: string;

    constructor(properties: Record<string, any> | null);

    getId(): number | null;

    getTimeBase(): string | null;

    getStart(): number | null;

    getStartTime(): string | null;

    getEnd(): number | null;

    getEndTime(): string | null;

    getTags(): Record<string, any> | null;

    getStringProperty(key: string): string | null;

    getNumberProperty(key: string): number | null;

    getProperty(key: string): any;

    getAllProperties(): Record<string, any> | null;
}

export declare class MediaInformation {
    static readonly KEY_FORMAT_PROPERTIES: string;
    static readonly KEY_FILENAME: string;
    static readonly KEY_FORMAT: string;
    static readonly KEY_FORMAT_LONG: string;
    static readonly KEY_START_TIME: string;
    static readonly KEY_DURATION: string;
    static readonly KEY_SIZE: string;
    static readonly KEY_BIT_RATE: string;
    static readonly KEY_TAGS: string;

    constructor(properties: Record<string, any> | null);

    getFilename(): string | null;

    getFormat(): string | null;

    getLongFormat(): string | null;

    getDuration(): string | null;

    getStartTime(): string | null;

    getSize(): string | null;

    getBitrate(): string | null;

    getTags(): Record<string, any> | null;

    getStreams(): StreamInformation[];

    getChapters(): Chapter[];

    getStringProperty(key: string): string | null;

    getNumberProperty(key: string): number | null;

    getProperty(key: string): any;

    getStringFormatProperty(key: string): string | null;

    getNumberFormatProperty(key: string): number | null;

    getFormatProperty(key: string): any;

    getFormatProperties(): Record<string, any> | null;

    getAllProperties(): Record<string, any> | null;
}

/** Parser that constructs MediaInformation from FFprobe JSON output. */
export declare class MediaInformationJsonParser {
    /**
     * Parses FFprobe JSON output into media information.
     *
     * Resolves null when parsing fails, and also when the input parses to nothing
     * (`{}`), matching the Flutter and React Native parsers. Use fromWithError() when
     * invalid JSON should reject the promise instead.
     *
     * @returns parsed media information, or null on a parse error or an empty result
     */
    static from(ffprobeJsonOutput: string): Promise<MediaInformation | null>;

    /**
     * Parses FFprobe JSON output into media information and rejects on parse errors.
     *
     * Unlike from(), an input that parses to nothing (`{}`) resolves with empty media
     * information rather than rejecting.
     *
     * @returns parsed media information
     */
    static fromWithError(ffprobeJsonOutput: string): Promise<MediaInformation>;
}

// ---------------------------------------------------------------------------
// Sessions (produced by FFmpegKit/FFprobeKit — not constructed directly)
// ---------------------------------------------------------------------------

export declare abstract class Session {
    /** The session id, assigned when the native session is created. */
    getSessionId(): number | null;

    getCommand(): string;

    /** Command arguments parsed from the command string. */
    getArguments(): string[];

    /** One of the `SessionState` values. */
    getState(): number;

    getReturnCode(): ReturnCode | null;

    /** Time taken to execute the session in milliseconds (0 until it is over). */
    getDuration(): number;

    /** Session create time, or null if unavailable. */
    getCreateTime(): Date | null;

    /** Session start time, or null before the run has started. */
    getStartTime(): Date | null;

    /** Session end time, or null before the run has completed. */
    getEndTime(): Date | null;

    /** Session output. Identical to getAllLogsAsString(), matching native. */
    getOutput(): Promise<string>;

    /**
     * Calls native getAllLogsAsStringWithTimeout(): waits for this session's messages in
     * transmit, up to `waitTimeout` milliseconds (native default 5000), then returns the
     * delivered record. Reports what the session already holds, without starting a
     * runtime, when no worker is running.
     */
    getAllLogsAsString(waitTimeout?: number): Promise<string>;

    /** Delivered logs as a concatenated string. Returns immediately, never waits. */
    getLogsAsString(): string;

    /** Delivered logs. Returns immediately, never waits. */
    getLogs(): Log[];

    /**
     * Calls native getAllLogsWithTimeout(): waits for this session's messages in transmit,
     * up to `waitTimeout` milliseconds (native default 5000), then returns the delivered
     * record.
     */
    getAllLogs(waitTimeout?: number): Promise<Log[]>;

    getFailStackTrace(): string | null;

    getLogCallback(): LogCallback | null;

    /** Session-specific log redirection strategy, or null when none is assigned. */
    getLogRedirectionStrategy(): number | null;

    /** Resolves true when native still has asynchronous messages queued for this session. */
    thereAreAsynchronousMessagesInTransmit(): Promise<boolean>;

    /** Cancels this session, or all sessions when this wrapper has no native id yet. */
    cancel(): Promise<void>;

    isFFmpeg(): boolean;

    isFFprobe(): boolean;

    isMediaInformation(): boolean;
}

export declare abstract class AbstractSession extends Session {
    static readonly DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT: 5000;

    static createFFmpegSession(
        argumentsArray: string[],
        logRedirectionStrategy?: number | null
    ): Promise<FFmpegSession>;

    static createFFmpegSessionFromMap(sessionMap: Record<string, any>): FFmpegSession;

    static createFFprobeSession(
        argumentsArray: string[],
        logRedirectionStrategy?: number | null
    ): Promise<FFprobeSession>;

    static createFFprobeSessionFromMap(sessionMap: Record<string, any>): FFprobeSession;

    static createMediaInformationSession(
        argumentsArray: string[]
    ): Promise<MediaInformationSession>;

    static createMediaInformationSessionFromMap(
        sessionMap: Record<string, any>
    ): MediaInformationSession;

    /** The session id, assigned when the native session is created. */
    getSessionId(): number | null;

    getCommand(): string;

    /** Command arguments parsed from the command string. */
    getArguments(): string[];

    /** One of the `SessionState` values. */
    getState(): number;

    getReturnCode(): ReturnCode | null;

    /** Time taken to execute the session in milliseconds (0 until it is over). */
    getDuration(): number;

    /** Session create time, or null if unavailable. */
    getCreateTime(): Date | null;

    /** Session start time, or null before the run has started. */
    getStartTime(): Date | null;

    /** Session end time, or null before the run has completed. */
    getEndTime(): Date | null;

    /** Session output. Identical to getAllLogsAsString(), matching native. */
    getOutput(): Promise<string>;

    /**
     * Calls native getAllLogsAsStringWithTimeout(): waits for this session's messages in
     * transmit, up to `waitTimeout` milliseconds (native default 5000), then returns the
     * delivered record. Reports what the session already holds, without starting a
     * runtime, when no worker is running.
     */
    getAllLogsAsString(waitTimeout?: number): Promise<string>;

    /** Delivered logs as a concatenated string. Returns immediately, never waits. */
    getLogsAsString(): string;

    /** Delivered logs. Returns immediately, never waits. */
    getLogs(): Log[];

    /**
     * Calls native getAllLogsWithTimeout(): waits for this session's messages in transmit,
     * up to `waitTimeout` milliseconds (native default 5000), then returns the delivered
     * record.
     */
    getAllLogs(waitTimeout?: number): Promise<Log[]>;

    getFailStackTrace(): string | null;

    getLogCallback(): LogCallback | null;

    /** Session-specific log redirection strategy, or null when none is assigned. */
    getLogRedirectionStrategy(): number | null;

    /** Complete callback invoked once an asynchronous run finishes (null for sync runs). */
    getCompleteCallback(): ((session: Session) => void) | null;

    /** Resolves true when native still has asynchronous messages queued for this session. */
    thereAreAsynchronousMessagesInTransmit(): Promise<boolean>;

    /** Cancels this session, or all sessions when this wrapper has no native id yet. */
    cancel(): Promise<void>;

    isFFmpeg(): boolean;

    isFFprobe(): boolean;

    isMediaInformation(): boolean;
}

export declare class FFmpegSession extends AbstractSession implements Session {
    static create(
        argumentsArray: string[],
        completeCallback?: FFmpegSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        statisticsCallback?: StatisticsCallback | null,
        logRedirectionStrategy?: number | null
    ): Promise<FFmpegSession>;

    /** Delivered statistics. Returns immediately, never waits. */
    getStatistics(): Statistics[];

    /**
     * Calls native getAllStatisticsWithTimeout(): waits for this session's messages in
     * transmit, up to `waitTimeout` milliseconds (native default 5000), then returns the
     * delivered record.
     */
    getAllStatistics(waitTimeout?: number): Promise<Statistics[]>;

    getLastReceivedStatistics(): Statistics | null;

    getStatisticsCallback(): StatisticsCallback | null;

    getCompleteCallback(): FFmpegSessionCompleteCallback | null;
}

export declare class FFprobeSession extends AbstractSession implements Session {
    static create(
        argumentsArray: string[],
        completeCallback?: FFprobeSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        logRedirectionStrategy?: number | null
    ): Promise<FFprobeSession>;

    getCompleteCallback(): FFprobeSessionCompleteCallback | null;
}

export declare class MediaInformationSession extends AbstractSession implements Session {
    static create(
        argumentsArray: string[],
        completeCallback?: MediaInformationSessionCompleteCallback | null,
        logCallback?: LogCallback | null
    ): Promise<MediaInformationSession>;

    getMediaInformation(): MediaInformation | null;

    setMediaInformation(mediaInformation: MediaInformation | null): void;

    getCompleteCallback(): MediaInformationSessionCompleteCallback | null;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

/**
 * Main entry point for running FFmpeg commands in the web worker.
 *
 * Commands execute inside the FFmpegKit worker. Methods that accept a command
 * string parse it into arguments using FFmpegKitConfig.parseArguments(); use
 * the argument-array methods when arguments must be passed losslessly.
 */
export declare class FFmpegKit {
    /**
     * Runs an FFmpeg command to completion.
     *
     * On web the public promise waits for completion, while native FFmpeg runs
     * asynchronously inside the worker so cancel and live-progress messages can
     * still be processed.
     *
     * @param command Command string to parse and execute.
     * @returns A populated FFmpegSession after the command reaches COMPLETED or FAILED.
     */
    static execute(command: string): Promise<FFmpegSession>;

    /**
     * Runs an FFmpeg command to completion using pre-split arguments.
     *
     * On web the public promise waits for completion, while native FFmpeg runs
     * asynchronously inside the worker so cancel and live-progress messages can
     * still be processed.
     *
     * @param commandArguments Command arguments passed without string parsing.
     * @returns A populated FFmpegSession after the command reaches COMPLETED or FAILED.
     */
    static executeWithArguments(commandArguments: string[]): Promise<FFmpegSession>;

    /**
     * Starts an asynchronous FFmpeg command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion. Use completeCallback to observe the final
     * populated session. logCallback and statisticsCallback receive live events
     * while the command runs.
     *
     * @param command Command string to parse and execute.
     * @param completeCallback Optional callback invoked once execution finishes.
     * @param logCallback Optional per-session log callback.
     * @param statisticsCallback Optional per-session statistics callback.
     * @returns The session after the request is posted to the worker.
     */
    static executeAsync(
        command: string,
        completeCallback?: FFmpegSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        statisticsCallback?: StatisticsCallback | null
    ): Promise<FFmpegSession>;

    /**
     * Starts an asynchronous FFmpeg command using pre-split arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param commandArguments Command arguments passed without string parsing.
     * @param completeCallback Optional callback invoked once execution finishes.
     * @param logCallback Optional per-session log callback.
     * @param statisticsCallback Optional per-session statistics callback.
     * @returns The session after the request is posted to the worker.
     */
    static executeWithArgumentsAsync(
        commandArguments: string[],
        completeCallback?: FFmpegSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        statisticsCallback?: StatisticsCallback | null
    ): Promise<FFmpegSession>;

    /**
     * Cancels a running session, or all ongoing sessions when no session id is
     * provided.
     *
     * On web, cancel requests are processed by the worker event loop and can
     * target both execute/executeWithArguments and executeAsync/executeWithArgumentsAsync
     * while native FFmpeg is still running.
     *
     * @returns A promise that resolves after the cancel request is sent.
     */
    static cancel(sessionId?: number | null): Promise<void>;

    /** @returns All FFmpeg sessions still in the native session history. */
    static listSessions(): Promise<FFmpegSession[]>;
}

/**
 * Main entry point for running FFprobe commands and extracting media information.
 *
 * Commands execute inside the FFmpegKit worker. Methods that accept a command
 * string parse it into arguments using FFmpegKitConfig.parseArguments(); use
 * the argument-array methods when arguments must be passed losslessly.
 */
export declare class FFprobeKit {
    /**
     * Runs an FFprobe command to completion.
     *
     * @param command Command string to parse and execute.
     * @returns A populated FFprobeSession after the command reaches COMPLETED or FAILED.
     */
    static execute(command: string): Promise<FFprobeSession>;

    /**
     * Runs an FFprobe command to completion using pre-split arguments.
     *
     * @param commandArguments Command arguments passed without string parsing.
     * @returns A populated FFprobeSession after the command reaches COMPLETED or FAILED.
     */
    static executeWithArguments(commandArguments: string[]): Promise<FFprobeSession>;

    /**
     * Starts an asynchronous FFprobe command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion. Use completeCallback to observe the final
     * populated session. logCallback receives live log events while the command
     * runs.
     *
     * @param command Command string to parse and execute.
     * @param completeCallback Optional callback invoked once execution finishes.
     * @param logCallback Optional per-session log callback.
     * @returns The session after the request is posted to the worker.
     */
    static executeAsync(
        command: string,
        completeCallback?: FFprobeSessionCompleteCallback | null,
        logCallback?: LogCallback | null
    ): Promise<FFprobeSession>;

    /**
     * Starts an asynchronous FFprobe command using pre-split arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param commandArguments Command arguments passed without string parsing.
     * @param completeCallback Optional callback invoked once execution finishes.
     * @param logCallback Optional per-session log callback.
     * @returns The session after the request is posted to the worker.
     */
    static executeWithArgumentsAsync(
        commandArguments: string[],
        completeCallback?: FFprobeSessionCompleteCallback | null,
        logCallback?: LogCallback | null
    ): Promise<FFprobeSession>;

    /**
     * Extracts media information for a path already present in the virtual filesystem.
     *
     * @param path Input path in MEMFS, WORKERFS, or another path visible to FFprobe.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns A MediaInformationSession populated with parsed media information when available.
     */
    static getMediaInformation(
        path: string,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /**
     * Starts asynchronous media-information extraction for a path.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion. Use completeCallback to observe the final
     * populated MediaInformationSession.
     *
     * @param path Input path in MEMFS, WORKERFS, or another path visible to FFprobe.
     * @param completeCallback Optional callback invoked once extraction finishes.
     * @param logCallback Optional per-session log callback.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns The session after the request is posted to the worker.
     */
    static getMediaInformationAsync(
        path: string,
        completeCallback?: MediaInformationSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /**
     * Extracts media information using a custom command that must emit JSON.
     *
     * @param command FFprobe command string that emits media information JSON.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns A MediaInformationSession populated with parsed media information when available.
     */
    static getMediaInformationFromCommand(
        command: string,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /**
     * Extracts media information using pre-split command arguments that must emit JSON.
     *
     * @param commandArguments FFprobe command arguments passed without string parsing.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns A MediaInformationSession populated with parsed media information when available.
     */
    static getMediaInformationFromCommandArguments(
        commandArguments: string[],
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /**
     * Starts asynchronous media-information extraction using a custom JSON command.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param command FFprobe command string that emits media information JSON.
     * @param completeCallback Optional callback invoked once extraction finishes.
     * @param logCallback Optional per-session log callback.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns The session after the request is posted to the worker.
     */
    static getMediaInformationFromCommandAsync(
        command: string,
        completeCallback?: MediaInformationSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /**
     * Starts asynchronous media-information extraction using pre-split JSON command arguments.
     *
     * The promise resolves with the session after the request is posted to the worker;
     * it does not wait for completion.
     *
     * @param commandArguments FFprobe command arguments passed without string parsing.
     * @param completeCallback Optional callback invoked once extraction finishes.
     * @param logCallback Optional per-session log callback.
     * @param waitTimeout Optional timeout in milliseconds while waiting for media information.
     * @returns The session after the request is posted to the worker.
     */
    static getMediaInformationFromCommandArgumentsAsync(
        commandArguments: string[],
        completeCallback?: MediaInformationSessionCompleteCallback | null,
        logCallback?: LogCallback | null,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    /** @returns All FFprobe sessions still in the native session history. */
    static listFFprobeSessions(): Promise<FFprobeSession[]>;

    /** @returns All media-information sessions still in the native session history. */
    static listMediaInformationSessions(): Promise<MediaInformationSession[]>;
}

export declare class FFmpegKitConfig {
    /**
     * Initialises the wasm module in its worker. Calling this is optional; operations that
     * need wasm initialise lazily when it has not already been called.
     *
     * Pass false before the first initialization to suppress the native load confirmation.
     */
    static init(printLoadConfirmation?: boolean): Promise<void>;

    /**
     * Terminates the wasm worker and drops runtime-owned session state. In-flight operations
     * are aborted, and later operations lazily start a fresh worker. User-registered callbacks
     * and global JS configuration are preserved.
     */
    static uninit(): Promise<void>;

    /** Registers a global log callback (pass null or nothing to clear). */
    static enableLogCallback(logCallback?: LogCallback | null): void;

    /** Registers a global statistics callback (pass null or nothing to clear). */
    static enableStatisticsCallback(statisticsCallback?: StatisticsCallback | null): void;

    /** Registers a global FFmpeg async completion callback (pass null or nothing to clear). */
    static enableFFmpegSessionCompleteCallback(
        completeCallback?: FFmpegSessionCompleteCallback | null
    ): void;

    static getFFmpegSessionCompleteCallback(): FFmpegSessionCompleteCallback | null;

    /** Registers a global FFprobe async completion callback (pass null or nothing to clear). */
    static enableFFprobeSessionCompleteCallback(
        completeCallback?: FFprobeSessionCompleteCallback | null
    ): void;

    static getFFprobeSessionCompleteCallback(): FFprobeSessionCompleteCallback | null;

    /** Registers a global media-information async completion callback (pass null or nothing to clear). */
    static enableMediaInformationSessionCompleteCallback(
        completeCallback?: MediaInformationSessionCompleteCallback | null
    ): void;

    static getMediaInformationSessionCompleteCallback(): MediaInformationSessionCompleteCallback | null;

    /** Global default log redirection strategy (a `LogRedirectionStrategy` value). */
    static getLogRedirectionStrategy(): number;

    static setLogRedirectionStrategy(logRedirectionStrategy: number): void;

    /** Returns the active log level. */
    static getLogLevel(): number;

    /**
     * Sets the active log level.
     *
     * The JS cache updates immediately and log callbacks/printing use the updated level.
     * On web, native FFmpeg/FFprobe reads the configured level when a run starts, so
     * stored native session logs may still follow the level captured for that run.
     */
    static setLogLevel(level: number): Promise<void>;

    /**
     * Enables dispatch of JS log callback events. Stored session logs are still
     * collected while dispatch is disabled.
     */
    static enableLogs(): Promise<void>;

    /**
     * Disables dispatch of JS log callback events. Stored session logs are still
     * collected.
     */
    static disableLogs(): Promise<void>;

    /**
     * Enables dispatch of JS statistics callback events. Stored session statistics
     * are still collected while dispatch is disabled.
     */
    static enableStatistics(): Promise<void>;

    /**
     * Disables dispatch of JS statistics callback events. Stored session
     * statistics are still collected.
     */
    static disableStatistics(): Promise<void>;

    /** Enables native log/statistics redirection to JS callbacks and session storage. */
    static enableRedirection(): Promise<void>;

    /**
     * Disables native log/statistics redirection; JS log/statistics callbacks stop
     * receiving events.
     */
    static disableRedirection(): Promise<void>;

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

    /** Returns FFmpegKitNext web package version without initializing the wasm worker. */
    static getVersion(): Promise<string>;

    /** Returns bundled FFmpeg version, initializing lazily when needed. */
    static getFFmpegVersion(): Promise<string>;

    /** Returns native library build date, initializing lazily when needed. */
    static getBuildDate(): Promise<string>;

    /** Sets an environment variable. Rejects when the native setenv() call fails. */
    static setEnvironmentVariable(variableName: string, variableValue: string): Promise<void>;

    /** Returns the platform name for parity with the other bridges. */
    static getPlatform(): Promise<string>;

    // ---- Session history --------------------------------------------------------
    // Every getter below reads the native session history. Where this binding still
    // holds the live object for a session id — the instance an execute call handed back
    // — that same instance is refreshed from the snapshot and returned, rather than a
    // second wrapper around the same session; that is what preserves the logs and
    // statistics a running session has accumulated from the event stream. The Flutter
    // and React Native plugins always mint a fresh object here, so portable code should
    // treat the result as a snapshot and not depend on `getSession(id) === session`
    // (true on web, false there) or on the reverse.
    static getSessions(): Promise<Session[]>;

    static getSession(sessionId: number): Promise<Session | null>;

    static getLastSession(): Promise<Session | null>;

    static getLastCompletedSession(): Promise<Session | null>;

    /** Rejects when `state` is not one of the `SessionState` values. */
    static getSessionsByState(state: number): Promise<Session[]>;

    static getSessionHistorySize(): Promise<number>;

    static setSessionHistorySize(sessionHistorySize: number): Promise<void>;

    static clearSessions(): Promise<void>;

    static deleteSession(sessionId: number): Promise<void>;

    static getFFmpegSessions(): Promise<FFmpegSession[]>;

    static getFFprobeSessions(): Promise<FFprobeSession[]>;

    static getMediaInformationSessions(): Promise<MediaInformationSession[]>;

    /**
     * Messages for this session that have not reached the JS callbacks yet. Counts both
     * the native in-transit messages and the events buffered in wasm awaiting a drain.
     */
    static messagesInTransmit(sessionId: number): Promise<number>;

    static sessionStateToString(state: number): string;

    // ---- Low-level session execute primitives -----------------------------------
    // Run an already-created session. Sync forms resolve on COMPLETION; async forms
    // resolve on START (completion arrives via the session's complete callback).
    //
    // Each resolves with the same session instance that was passed in, populated from
    // the run. The Flutter and React Native equivalents resolve with no value, so code
    // that must stay portable across platforms should keep using the session it already
    // holds rather than the resolved value.
    static ffmpegExecute(ffmpegSession: FFmpegSession): Promise<FFmpegSession>;

    static asyncFFmpegExecute(ffmpegSession: FFmpegSession): Promise<FFmpegSession>;

    static ffprobeExecute(ffprobeSession: FFprobeSession): Promise<FFprobeSession>;

    static asyncFFprobeExecute(ffprobeSession: FFprobeSession): Promise<FFprobeSession>;

    static getMediaInformationExecute(
        mediaInformationSession: MediaInformationSession,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    static asyncGetMediaInformationExecute(
        mediaInformationSession: MediaInformationSession,
        waitTimeout?: number | null
    ): Promise<MediaInformationSession>;

    // ---- Argument helpers -------------------------------------------------------
    /** Splits a command string into arguments, honouring single/double quotes. */
    static parseArguments(command: string): string[];

    /** Joins arguments into a space-separated string (lossy for args with spaces). */
    static argumentsToString(commandArguments: string[] | null): string;
}

// ---------------------------------------------------------------------------
// ffkitmem:/ffkitstream: in-memory I/O (avoid staging files in MEMFS)
// ---------------------------------------------------------------------------

/**
 * Seekable in-memory input built from a byte array.
 *
 * Use getUrl() as an FFmpeg input URL. Call close() when the buffer is no longer
 * needed to release the worker-side resources.
 */
export declare class FFmpegKitInputBuffer {
    /**
     * Registers byte-array data as a seekable input buffer.
     *
     * @param data bytes to expose through the generated ffkitmem: URL. Copied before
     * worker posting; the caller's buffer is not detached.
     * @param extension optional container hint, e.g. "mp4"
     * @returns buffer wrapper containing the generated URL
     */
    static fromByteArray(data: Uint8Array, extension?: string): Promise<FFmpegKitInputBuffer>;

    /** @returns ffkitmem: URL to use as an FFmpeg input */
    getUrl(): string;

    /** @returns size of the input buffer in bytes */
    getSize(): number;

    /** @returns promise that resolves after the worker releases the buffer */
    close(): Promise<void>;
}

/**
 * Seekable in-memory output buffer.
 *
 * Use getUrl() as an FFmpeg output URL, then call toByteArray() after the command
 * completes to read a copy of the produced bytes. Once close() has been called every
 * other method on this object rejects.
 */
export declare class FFmpegKitOutputBuffer {
    /**
     * Creates a seekable output buffer. Positional parameters, matching the React Native
     * plugin's FFmpegKitOutputBuffer.create().
     *
     * @param extension optional output format hint, e.g. "mp4"
     * @param initialCapacity optional starting size in bytes; must be paired with maxCapacity
     * @param maxCapacity optional upper bound in bytes; must be paired with initialCapacity
     * @returns buffer wrapper containing the generated URL
     */
    static create(
        extension?: string,
        initialCapacity?: number | null,
        maxCapacity?: number | null
    ): Promise<FFmpegKitOutputBuffer>;

    /** @returns ffkitmem: URL to use as an FFmpeg output */
    getUrl(): string;

    /** @returns number of bytes currently held by the output buffer */
    getSize(): Promise<number>;

    /** @returns copy of the bytes written so far */
    toByteArray(): Promise<Uint8Array>;

    /** @returns promise that resolves after the worker releases the buffer */
    close(): Promise<void>;
}

/**
 * Non-seekable streaming input for ffkitstream: URLs.
 *
 * Pass getUrl() as an FFmpeg input and call write() while a command is consuming
 * the stream. closeInput() signals end-of-input. Once close() has been called every
 * other method on this object rejects - a released handle is never reported as
 * backpressure.
 */
export declare class FFmpegKitStreamInput {
    /**
     * Creates a streaming input handle. Positional parameters, matching the React Native
     * plugin's FFmpegKitStreamInput.create().
     *
     * @param extension Optional format hint, for example "mp4".
     * @param capacity Optional ring-buffer capacity in bytes.
     * @returns A streaming input whose URL can be passed to FFmpeg.
     */
    static create(extension?: string, capacity?: number | null): Promise<FFmpegKitStreamInput>;

    /** @returns The ffkitstream: URL to use as an FFmpeg input. */
    getUrl(): string;

    /**
     * Attempts to append bytes to the stream input.
     *
     * This call is non-blocking. It resolves with the number of bytes
     * accepted into the internal ring buffer. The value can be smaller than
     * data.byteLength, including 0, when the buffer is full. Retry the remaining
     * bytes after FFmpeg has consumed more data. The input bytes are copied before
     * worker posting, so this call does not detach the caller's buffer.
     *
     * @returns Number of bytes accepted into the stream buffer.
     */
    write(data: Uint8Array): Promise<number>;

    /** Signals EOF to FFmpeg once already written bytes have been consumed. */
    closeInput(): Promise<void>;

    /** Releases the streaming input resources. */
    close(): Promise<void>;
}

/**
 * Non-seekable streaming output for ffkitstream: URLs.
 *
 * Pass getUrl() as an FFmpeg output target and call read() while a command is
 * producing data. Once close() has been called every other method on this object
 * rejects - a released handle is never reported as "nothing ready yet".
 */
export declare class FFmpegKitStreamOutput {
    /**
     * Creates a streaming output handle. Positional parameters, matching the React Native
     * plugin's FFmpegKitStreamOutput.create().
     *
     * @param extension Optional format hint, for example "mp4".
     * @param capacity Optional ring-buffer capacity in bytes.
     * @returns A streaming output whose URL can be passed to FFmpeg.
     */
    static create(extension?: string, capacity?: number | null): Promise<FFmpegKitStreamOutput>;

    /** @returns The ffkitstream: URL to use as an FFmpeg output target. */
    getUrl(): string;

    /**
     * Attempts to read produced bytes from the stream output.
     *
     * This call is non-blocking.
     *
     * @returns A non-empty Uint8Array when data is available, an empty Uint8Array
     * at EOF/closed, or null when no data is ready yet and the caller should retry.
     */
    read(maxBytes: number): Promise<Uint8Array | null>;

    /** Releases the streaming output resources. */
    close(): Promise<void>;
}

// ---------------------------------------------------------------------------
// Web-only helpers
// ---------------------------------------------------------------------------

/**
 * Writes bytes into the module's virtual filesystem (MEMFS). Data is copied before
 * worker posting; the caller's buffer is not detached.
 */
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
