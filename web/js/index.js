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

// Public entry point for the FFmpegKitNext web package.
//
// BARREL MODULE - RE-EXPORTS ONLY. DO NOT DECLARE ANYTHING HERE.
//
// Every class lives in its own module under src/, mirroring the layout of
// web/src/*.cpp and flutter/flutter/lib/*.dart. This file exists solely to define
// the published surface. Keeping it declaration-free means no internal module ever
// needs to import it, so it can never take part in an import cycle - the failure
// mode that previously made the package throw at load time.
//
// Layering under src/ (dependencies point strictly upward, no cycles):
//
//   leaves (import nothing)  Constants, Level, Arguments, Log, ReturnCode,
//                            Statistics, StreamInformation, Chapter, Session,
//                            SessionRegistry
//   value types              MediaInformation
//   runtime conduit          FFmpegKitFactory  (leaves + SessionRegistry only)
//   sessions                 AbstractSession -> FFmpeg/FFprobe/MediaInformation
//   facades                  FFmpegKit, FFprobeKit, FFmpegKitConfig, ArchDetect,
//                            Packages, MediaInformationJsonParser, I/O,
//                            FFmpegKitVirtualDirectory, FFmpegKitVirtualFile,
//                            FFmpegKitLocalFile (factory only)
//   deprecated facade        FileSystem (factory + FFmpegKitVirtualDirectory and
//                            FFmpegKitVirtualFile, which it wraps)
//
// FFmpegKitFactory must never import a session class; it instantiates sessions
// through SessionRegistry, which each session module writes to at module scope.
// Those registrations are why this barrel re-exports all three session classes -
// see the note in src/SessionRegistry.js before adding "sideEffects" to package.json.

// ---- Enums -------------------------------------------------------------------
export {LogRedirectionStrategy, SessionState} from './src/Constants.js';
export {Level} from './src/Level.js';

// ---- Value types -------------------------------------------------------------
export {Chapter} from './src/Chapter.js';
export {Log} from './src/Log.js';
export {MediaInformation} from './src/MediaInformation.js';
export {MediaInformationJsonParser} from './src/MediaInformationJsonParser.js';
export {ReturnCode} from './src/ReturnCode.js';
export {Statistics} from './src/Statistics.js';
export {StreamInformation} from './src/StreamInformation.js';

// ---- Sessions ----------------------------------------------------------------
export {AbstractSession} from './src/AbstractSession.js';
export {FFmpegSession} from './src/FFmpegSession.js';
export {FFprobeSession} from './src/FFprobeSession.js';
export {MediaInformationSession} from './src/MediaInformationSession.js';
export {Session} from './src/Session.js';

// ---- Entry points ------------------------------------------------------------
export {ArchDetect} from './src/ArchDetect.js';
export {FFmpegKit} from './src/FFmpegKit.js';
export {FFmpegKitConfig} from './src/FFmpegKitConfig.js';
export {FFprobeKit} from './src/FFprobeKit.js';
export {Packages} from './src/Packages.js';

// ---- Virtual filesystem ------------------------------------------------------
export {FFmpegKitLocalFile} from './src/FFmpegKitLocalFile.js';
export {FFmpegKitVirtualDirectory} from './src/FFmpegKitVirtualDirectory.js';
export {FFmpegKitVirtualFile} from './src/FFmpegKitVirtualFile.js';

// ---- Deprecated web-only filesystem helpers ----------------------------------
export {mount, readFile, writeFile} from './src/FileSystem.js';

// ---- ffkitmem:/ffkitstream: in-memory I/O ------------------------------------
export {FFmpegKitInputBuffer} from './src/FFmpegKitInputBuffer.js';
export {FFmpegKitOutputBuffer} from './src/FFmpegKitOutputBuffer.js';
export {FFmpegKitStreamInput} from './src/FFmpegKitStreamInput.js';
export {FFmpegKitStreamOutput} from './src/FFmpegKitStreamOutput.js';
