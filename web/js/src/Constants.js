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

// Enum-like constants shared across the binding layer.
//
// LEAF MODULE - MUST NOT IMPORT ANYTHING.
//
// Everything read at module-evaluation time (rather than inside a function body)
// belongs here. A module with no imports is never mid-evaluation when another
// module reads from it, so its exports are always initialized. Constants declared
// next to the classes that use them can land in the temporal dead zone when the
// import graph is entered from a different edge; keeping them here makes that
// impossible by construction.

// FFmpegKitNext web package version, reported by FFmpegKitConfig.getVersion().
// Keep in sync with web/package.json and web/package.dist.json.
export const FFMPEG_KIT_VERSION = '8.1.1';

export const SessionState = Object.freeze({
    CREATED: 0,
    RUNNING: 1,
    FAILED: 2,
    COMPLETED: 3,
});

// Controls whether a session's logs are also printed to the console, on top of being
// delivered to the log callbacks. Values match the native ffmpegkit::LogRedirectionStrategy
// (and the Flutter/React Native plugins). FFmpegKitFactory applies the active strategy for
// each incoming log; see its log handler.
export const LogRedirectionStrategy = Object.freeze({
    ALWAYS_PRINT_LOGS: 0,
    PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED: 1,
    PRINT_LOGS_WHEN_GLOBAL_CALLBACK_NOT_DEFINED: 2,
    PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED: 3,
    NEVER_PRINT_LOGS: 4,
});

// Global default log redirection strategy; matches the native default.
export const DEFAULT_LOG_REDIRECTION_STRATEGY =
    LogRedirectionStrategy.PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED;

// Session discriminator used by the worker's serialized session maps and by
// SessionRegistry. Internal: not part of the published surface.
export const SessionType = Object.freeze({
    FFMPEG: 1,
    FFPROBE: 2,
    MEDIA_INFORMATION: 3,
});
