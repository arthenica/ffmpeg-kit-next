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

/*
 * embind bindings that expose the FFmpegKitNext C++ API to JavaScript. Compiled
 * into libffmpegkit and linked into the FFmpegKitModule main module. Registration
 * runs from static initializers; see the anchor at the bottom for how it is kept
 * alive under MAIN_MODULE=2 dead-code elimination.
 *
 * Scope: synchronous execution, sessions, value types, enums and config, plus
 * live log/statistics delivery via the buffer-and-drain pair (enableEventBuffering
 * / drainLogEvents / drainStatisticsEvents) — see the note near those bindings for
 * why the callbacks are surfaced this way rather than as JS function callbacks.
 *
 * Also bound: the ffkitmem:/ffkitstream: I/O classes (FFmpegKitInputBuffer,
 * FFmpegKitOutputBuffer, FFmpegKitStreamInput, FFmpegKitStreamOutput). The streams
 * block on condition variables in FFmpegKitConfig, which compile to Atomics.wait —
 * legal only off the main browser thread. The intended topology is executeAsync()
 * (which runs FFmpeg on its own pthread and returns immediately), leaving the host
 * worker free to pump write()/read() against the shared-memory ring. Callers stuck
 * on the main thread must instead poll with timeoutMs == 0 (non-blocking).
 */

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Chapter.h"
#include "FFmpegKit.h"
#include "FFmpegKitConfig.h"
#include "FFmpegKitInputBuffer.h"
#include "FFmpegKitOutputBuffer.h"
#include "FFmpegKitStreamInput.h"
#include "FFmpegKitStreamOutput.h"
#include "FFmpegSession.h"
#include "FFprobeKit.h"
#include "FFprobeSession.h"
#include "Level.h"
#include "Log.h"
#include "LogRedirectionStrategy.h"
#include "MediaInformation.h"
#include "MediaInformationSession.h"
#include "ReturnCode.h"
#include "SessionState.h"
#include "Statistics.h"
#include "StreamInformation.h"

using namespace emscripten;
using namespace ffmpegkit;

namespace {

// ---------------------------------------------------------------------------
// Small conversion helpers. The C++ API returns std::shared_ptr<std::string> /
// std::shared_ptr<int64_t> for "nullable" values and std::list / std::vector of
// shared_ptr for collections; embind does not marshal those directly, so we
// convert them to JS strings/numbers/arrays (or null) here.
// ---------------------------------------------------------------------------

val optString(const std::shared_ptr<std::string> &value) {
    return value ? val(*value) : val::null();
}

val optInt64(const std::shared_ptr<std::int64_t> &value) {
    return value ? val(static_cast<double>(*value)) : val::null();
}

template <typename T>
val listToArray(const std::shared_ptr<std::list<std::shared_ptr<T>>> &items) {
    val array = val::array();
    if (items) {
        int index = 0;
        for (const auto &item : *items) {
            array.set(index++, item);
        }
    }
    return array;
}

template <typename T>
val vectorToArray(const std::shared_ptr<std::vector<std::shared_ptr<T>>> &items) {
    val array = val::array();
    if (items) {
        int index = 0;
        for (const auto &item : *items) {
            array.set(index++, item);
        }
    }
    return array;
}

// ---- Session accessors that return collections (bound as methods) ----------

val session_getAllLogs(AbstractSession &self) { return listToArray(self.getAllLogs()); }
val session_getLogs(AbstractSession &self) { return listToArray(self.getLogs()); }
val ffmpegSession_getStatistics(FFmpegSession &self) { return listToArray(self.getStatistics()); }
val ffmpegSession_getAllStatistics(FFmpegSession &self) { return listToArray(self.getAllStatistics()); }

// ---- FFmpegKit / FFprobeKit statics -----------------------------------------
// Free wrappers avoid embind overload-resolution issues (execute/cancel are
// overloaded on the C++ side) and keep the async overloads out of v1.

std::shared_ptr<FFmpegSession> ffmpegKit_execute(const std::string command) {
    return FFmpegKit::execute(command);
}

// Starts the command on a worker thread and returns the session immediately, so
// the JS host thread stays free to service emscripten's on-demand pthread
// creation (FFmpeg spawns more threads than the prewarmed pool). The JS side
// observes completion by polling the session state; the completion callback is a
// no-op here because delivering it to JS would require cross-thread val proxying.
std::shared_ptr<FFmpegSession> ffmpegKit_executeAsync(const std::string command) {
    return FFmpegKit::executeAsync(
        command, [](std::shared_ptr<ffmpegkit::FFmpegSession>) {});
}
void ffmpegKit_cancel() { FFmpegKit::cancel(); }
void ffmpegKit_cancelSession(const long sessionId) { FFmpegKit::cancel(sessionId); }
val ffmpegKit_listSessions() { return listToArray(FFmpegKit::listSessions()); }

std::shared_ptr<FFprobeSession> ffprobeKit_execute(const std::string command) {
    return FFprobeKit::execute(command);
}
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformation(const std::string path) {
    return FFprobeKit::getMediaInformation(path);
}

std::list<std::string> jsArrayToStringList(const val &array) {
    std::list<std::string> result;
    if (array.isNull() || array.isUndefined()) {
        return result;
    }

    const int length = array["length"].as<int>();
    for (int i = 0; i < length; i++) {
        val item = array[i];
        if (!item.isNull() && !item.isUndefined()) {
            auto value = item.as<std::string>();
            if (!value.empty()) {
                result.push_back(value);
            }
        }
    }

    return result;
}

std::map<std::string, std::string> jsObjectToStringMap(const val &object) {
    std::map<std::string, std::string> result;
    if (object.isNull() || object.isUndefined()) {
        return result;
    }

    val keys = val::global("Object").call<val>("keys", object);
    const int length = keys["length"].as<int>();
    for (int i = 0; i < length; i++) {
        auto key = keys[i].as<std::string>();
        val item = object[key];
        if (!key.empty() && !item.isNull() && !item.isUndefined()) {
            auto value = item.as<std::string>();
            if (!value.empty()) {
                result[key] = value;
            }
        }
    }

    return result;
}

void config_setFontDirectory(const std::string fontDirectoryPath,
                             const val fontNameMapping) {
    FFmpegKitConfig::setFontDirectory(fontDirectoryPath,
                                      jsObjectToStringMap(fontNameMapping));
}

void config_setFontDirectoryList(const val fontDirectoryList,
                                 const val fontNameMapping) {
    FFmpegKitConfig::setFontDirectoryList(
        jsArrayToStringList(fontDirectoryList),
        jsObjectToStringMap(fontNameMapping));
}

// ---- MediaInformation / StreamInformation accessors -------------------------
// Bound as methods; convert the nullable shared_ptr<string>/<int64_t> results.

val mediaInformation_getFilename(MediaInformation &self) { return optString(self.getFilename()); }
val mediaInformation_getFormat(MediaInformation &self) { return optString(self.getFormat()); }
val mediaInformation_getLongFormat(MediaInformation &self) { return optString(self.getLongFormat()); }
val mediaInformation_getDuration(MediaInformation &self) { return optString(self.getDuration()); }
val mediaInformation_getStartTime(MediaInformation &self) { return optString(self.getStartTime()); }
val mediaInformation_getSize(MediaInformation &self) { return optString(self.getSize()); }
val mediaInformation_getBitrate(MediaInformation &self) { return optString(self.getBitrate()); }
val mediaInformation_getStreams(MediaInformation &self) { return vectorToArray(self.getStreams()); }
val mediaInformation_getChapters(MediaInformation &self) { return vectorToArray(self.getChapters()); }

val streamInformation_getIndex(StreamInformation &self) { return optInt64(self.getIndex()); }
val streamInformation_getType(StreamInformation &self) { return optString(self.getType()); }
val streamInformation_getCodec(StreamInformation &self) { return optString(self.getCodec()); }
val streamInformation_getCodecLong(StreamInformation &self) { return optString(self.getCodecLong()); }
val streamInformation_getFormat(StreamInformation &self) { return optString(self.getFormat()); }
val streamInformation_getWidth(StreamInformation &self) { return optInt64(self.getWidth()); }
val streamInformation_getHeight(StreamInformation &self) { return optInt64(self.getHeight()); }
val streamInformation_getBitrate(StreamInformation &self) { return optString(self.getBitrate()); }
val streamInformation_getSampleRate(StreamInformation &self) { return optString(self.getSampleRate()); }
val streamInformation_getChannelLayout(StreamInformation &self) { return optString(self.getChannelLayout()); }

// ---- Byte marshaling --------------------------------------------------------
// embind's default std::vector<uint8_t> conversion marshals element-by-element,
// which dominates transfer time for media payloads. These helpers do a single
// bulk copy through a heap-backed typed-array view instead.

// Copies a JS byte source (Uint8Array or any TypedArray/array-like with a numeric
// "length") into a std::vector<uint8_t> with one TypedArray.set into a heap view.
std::vector<uint8_t> toByteVector(const val &data) {
    const size_t length = data["length"].as<size_t>();
    std::vector<uint8_t> bytes(length);
    if (length > 0) {
        val view = val(typed_memory_view(length, bytes.data()));
        view.call<void>("set", data);
    }
    return bytes;
}

// Copies raw bytes out into a fresh, JS-owned Uint8Array. The heap view aliases
// C++ memory that is freed on return, so the copy (via TypedArray.set) must happen
// before this returns — which it does, synchronously, with no allocation between.
val toUint8Array(const std::vector<uint8_t> &data) {
    val result = val::global("Uint8Array").new_(data.size());
    if (!data.empty()) {
        val view = val(typed_memory_view(data.size(), data.data()));
        result.call<void>("set", view);
    }
    return result;
}

// ---- ffkitmem: / ffkitstream: I/O -------------------------------------------
// Private constructors + overloaded factories/writers, so (like execute/cancel)
// each is exposed through a fixed-arity free wrapper. timeoutMs: -1 blocks (only
// legal off the main thread), 0 is non-blocking, > 0 is a timed wait.

std::shared_ptr<FFmpegKitInputBuffer>
inputBuffer_fromByteArray(const val &data, const std::string &extension) {
    std::vector<uint8_t> bytes = toByteVector(data);
    return FFmpegKitInputBuffer::fromBytes(bytes.data(), bytes.size(), extension);
}

std::shared_ptr<FFmpegKitOutputBuffer>
outputBuffer_create(const std::string &extension) {
    return FFmpegKitOutputBuffer::create(extension);
}
std::shared_ptr<FFmpegKitOutputBuffer>
outputBuffer_createWithCapacity(const std::string &extension,
                                const long initialCapacity,
                                const long maxCapacity) {
    return FFmpegKitOutputBuffer::create(extension, initialCapacity, maxCapacity);
}
val outputBuffer_toByteArray(FFmpegKitOutputBuffer &self) {
    return toUint8Array(*self.toByteArray());
}

std::shared_ptr<FFmpegKitStreamInput>
streamInput_create(const std::string &extension) {
    return FFmpegKitStreamInput::create(extension);
}
std::shared_ptr<FFmpegKitStreamInput>
streamInput_createWithCapacity(const std::string &extension,
                               const long capacity) {
    return FFmpegKitStreamInput::create(extension, capacity);
}
// Returns the number of bytes accepted into the ring (may be a short write when
// timeoutMs elapses with the ring full).
int streamInput_write(FFmpegKitStreamInput &self, const val &data,
                      const int timeoutMs) {
    std::vector<uint8_t> bytes = toByteVector(data);
    return self.write(bytes.data(), bytes.size(), timeoutMs);
}

std::shared_ptr<FFmpegKitStreamOutput>
streamOutput_create(const std::string &extension) {
    return FFmpegKitStreamOutput::create(extension);
}
std::shared_ptr<FFmpegKitStreamOutput>
streamOutput_createWithCapacity(const std::string &extension,
                                const long capacity) {
    return FFmpegKitStreamOutput::create(extension, capacity);
}
// Tri-state result: null == timed out (retry), empty Uint8Array == EOF/closed,
// non-empty == data read.
val streamOutput_read(FFmpegKitStreamOutput &self, const int maxBytes,
                      const int timeoutMs) {
    auto result = self.read(maxBytes, timeoutMs);
    if (result == nullptr) {
        return val::null();
    }
    return toUint8Array(*result);
}

// ---- Live log / statistics event buffering ----------------------------------
// The native LogCallback/StatisticsCallback std::functions fire on FFmpegKit's
// dedicated callback pthread (see enableRedirection in FFmpegKitConfig.cpp), which
// is a different Web Worker than the one hosting the module. embind vals cannot be
// invoked across threads, so instead of pushing to JS from the callback thread we
// buffer the (immutable, thread-safe) shared_ptr<Log>/<Statistics> here under a
// mutex. The host worker owns the JS and pulls batches with drainLogEvents /
// drainStatisticsEvents from its own thread, on its own schedule (typically the
// executeAsync poll loop). No cross-thread val calls, no Asyncify, no proxying.
//
// This is the "live progress" path; getAllLogsAsString / getStatistics on the
// session remain the source of truth for the final, complete record.

std::mutex g_eventMutex;
std::vector<std::shared_ptr<Log>> g_logEvents;
std::vector<std::shared_ptr<Statistics>> g_statisticsEvents;
bool g_eventBufferingEnabled = false;

// These three functions are the internal event bridge. They are bound as nameless
// module-level free functions (underscore-prefixed) rather than on a class, because
// they have no counterpart in the native FFmpegKitConfig API and must not appear on
// the public surface. They are called only from the worker host (FFmpegKitWorker);
// app code never reaches them. embind cannot truly hide a bound symbol, so
// "internal" means "kept off the public classes and out of the published typings".
//
// Installation is worker-triggered (not a static initializer): assigning to
// FFmpegKitConfig's std::function callback globals from another TU's static-init
// phase would risk the static-initialization-order fiasco. The worker calls the
// installer once at module load, before any execute, so no early logs are missed.

// Registers the global callbacks that append incoming events to the buffers.
// Idempotent: safe to call once per module load. Enabling a global log callback
// does not affect session log/statistics storage (process_log/process_statistics
// store to the session before invoking callbacks), so getAllLogsAsString and
// getStatistics keep working unchanged.
void config_enableEventBuffering() {
    {
        std::lock_guard<std::mutex> lock(g_eventMutex);
        if (g_eventBufferingEnabled) {
            return;
        }
        g_eventBufferingEnabled = true;
    }
    FFmpegKitConfig::enableLogCallback([](const std::shared_ptr<Log> log) {
        std::lock_guard<std::mutex> lock(g_eventMutex);
        g_logEvents.push_back(log);
    });
    FFmpegKitConfig::enableStatisticsCallback(
        [](const std::shared_ptr<Statistics> statistics) {
            std::lock_guard<std::mutex> lock(g_eventMutex);
            g_statisticsEvents.push_back(statistics);
        });
}

// Atomically swaps out the pending log batch and returns it as a JS array of Log
// objects. Called from the host worker thread (which owns JS).
val config_drainLogEvents() {
    std::vector<std::shared_ptr<Log>> batch;
    {
        std::lock_guard<std::mutex> lock(g_eventMutex);
        batch.swap(g_logEvents);
    }
    val array = val::array();
    for (std::size_t i = 0; i < batch.size(); ++i) {
        array.set(static_cast<int>(i), batch[i]);
    }
    return array;
}

val config_drainStatisticsEvents() {
    std::vector<std::shared_ptr<Statistics>> batch;
    {
        std::lock_guard<std::mutex> lock(g_eventMutex);
        batch.swap(g_statisticsEvents);
    }
    val array = val::array();
    for (std::size_t i = 0; i < batch.size(); ++i) {
        array.set(static_cast<int>(i), batch[i]);
    }
    return array;
}

val chapter_getId(Chapter &self) { return optInt64(self.getId()); }
val chapter_getStart(Chapter &self) { return optInt64(self.getStart()); }
val chapter_getStartTime(Chapter &self) { return optString(self.getStartTime()); }
val chapter_getEnd(Chapter &self) { return optInt64(self.getEnd()); }
val chapter_getEndTime(Chapter &self) { return optString(self.getEndTime()); }

} // namespace

EMSCRIPTEN_BINDINGS(ffmpegkit_bindings) {

    // ---- Enums --------------------------------------------------------------
    enum_<SessionState>("SessionState")
        .value("Created", SessionStateCreated)
        .value("Running", SessionStateRunning)
        .value("Failed", SessionStateFailed)
        .value("Completed", SessionStateCompleted);

    enum_<Level>("Level")
        .value("AVLogStdErr", LevelAVLogStdErr)
        .value("AVLogQuiet", LevelAVLogQuiet)
        .value("AVLogPanic", LevelAVLogPanic)
        .value("AVLogFatal", LevelAVLogFatal)
        .value("AVLogError", LevelAVLogError)
        .value("AVLogWarning", LevelAVLogWarning)
        .value("AVLogInfo", LevelAVLogInfo)
        .value("AVLogVerbose", LevelAVLogVerbose)
        .value("AVLogDebug", LevelAVLogDebug)
        .value("AVLogTrace", LevelAVLogTrace);

    enum_<LogRedirectionStrategy>("LogRedirectionStrategy")
        .value("AlwaysPrintLogs", LogRedirectionStrategyAlwaysPrintLogs)
        .value("PrintLogsWhenNoCallbacksDefined", LogRedirectionStrategyPrintLogsWhenNoCallbacksDefined)
        .value("PrintLogsWhenGlobalCallbackNotDefined", LogRedirectionStrategyPrintLogsWhenGlobalCallbackNotDefined)
        .value("PrintLogsWhenSessionCallbackNotDefined", LogRedirectionStrategyPrintLogsWhenSessionCallbackNotDefined)
        .value("NeverPrintLogs", LogRedirectionStrategyNeverPrintLogs);

    // ---- Value types --------------------------------------------------------
    class_<ReturnCode>("ReturnCode")
        .smart_ptr<std::shared_ptr<ReturnCode>>("shared_ptr<ReturnCode>")
        .function("getValue", &ReturnCode::getValue)
        .function("isValueSuccess", &ReturnCode::isValueSuccess)
        .function("isValueError", &ReturnCode::isValueError)
        .function("isValueCancel", &ReturnCode::isValueCancel);

    class_<Log>("Log")
        .smart_ptr<std::shared_ptr<Log>>("shared_ptr<Log>")
        .function("getSessionId", &Log::getSessionId)
        .function("getLevel", &Log::getLevel)
        .function("getMessage", &Log::getMessage);

    class_<Statistics>("Statistics")
        .smart_ptr<std::shared_ptr<Statistics>>("shared_ptr<Statistics>")
        .function("getSessionId", &Statistics::getSessionId)
        .function("getVideoFrameNumber", &Statistics::getVideoFrameNumber)
        .function("getVideoFps", &Statistics::getVideoFps)
        .function("getVideoQuality", &Statistics::getVideoQuality)
        .function("getSize", &Statistics::getSize)
        .function("getTime", &Statistics::getTime)
        .function("getBitrate", &Statistics::getBitrate)
        .function("getSpeed", &Statistics::getSpeed);

    class_<StreamInformation>("StreamInformation")
        .smart_ptr<std::shared_ptr<StreamInformation>>("shared_ptr<StreamInformation>")
        .function("getIndex", &streamInformation_getIndex)
        .function("getType", &streamInformation_getType)
        .function("getCodec", &streamInformation_getCodec)
        .function("getCodecLong", &streamInformation_getCodecLong)
        .function("getFormat", &streamInformation_getFormat)
        .function("getWidth", &streamInformation_getWidth)
        .function("getHeight", &streamInformation_getHeight)
        .function("getBitrate", &streamInformation_getBitrate)
        .function("getSampleRate", &streamInformation_getSampleRate)
        .function("getChannelLayout", &streamInformation_getChannelLayout);

    class_<Chapter>("Chapter")
        .smart_ptr<std::shared_ptr<Chapter>>("shared_ptr<Chapter>")
        .function("getId", &chapter_getId)
        .function("getStart", &chapter_getStart)
        .function("getStartTime", &chapter_getStartTime)
        .function("getEnd", &chapter_getEnd)
        .function("getEndTime", &chapter_getEndTime);

    class_<MediaInformation>("MediaInformation")
        .smart_ptr<std::shared_ptr<MediaInformation>>("shared_ptr<MediaInformation>")
        .function("getFilename", &mediaInformation_getFilename)
        .function("getFormat", &mediaInformation_getFormat)
        .function("getLongFormat", &mediaInformation_getLongFormat)
        .function("getDuration", &mediaInformation_getDuration)
        .function("getStartTime", &mediaInformation_getStartTime)
        .function("getSize", &mediaInformation_getSize)
        .function("getBitrate", &mediaInformation_getBitrate)
        .function("getStreams", &mediaInformation_getStreams)
        .function("getChapters", &mediaInformation_getChapters);

    // ---- ffkitmem: / ffkitstream: I/O ---------------------------------------
    // Seekable in-memory input: FFmpegKitInputBuffer.fromByteArray(bytes, ext).
    class_<FFmpegKitInputBuffer>("FFmpegKitInputBuffer")
        .smart_ptr<std::shared_ptr<FFmpegKitInputBuffer>>(
            "shared_ptr<FFmpegKitInputBuffer>")
        .class_function("fromByteArray", &inputBuffer_fromByteArray)
        .function("getUrl", &FFmpegKitInputBuffer::getUrl)
        .function("getSize", &FFmpegKitInputBuffer::getSize)
        .function("close", &FFmpegKitInputBuffer::close);

    // Seekable in-memory output: read back with toByteArray() after the command.
    class_<FFmpegKitOutputBuffer>("FFmpegKitOutputBuffer")
        .smart_ptr<std::shared_ptr<FFmpegKitOutputBuffer>>(
            "shared_ptr<FFmpegKitOutputBuffer>")
        .class_function("create", &outputBuffer_create)
        .class_function("createWithCapacity", &outputBuffer_createWithCapacity)
        .function("getUrl", &FFmpegKitOutputBuffer::getUrl)
        .function("getSize", &FFmpegKitOutputBuffer::getSize)
        .function("toByteArray", &outputBuffer_toByteArray)
        .function("close", &FFmpegKitOutputBuffer::close);

    // Non-seekable streaming input: pump write() from the host worker while
    // FFmpeg drains the ring on its own pthread; closeInput() signals EOF.
    class_<FFmpegKitStreamInput>("FFmpegKitStreamInput")
        .smart_ptr<std::shared_ptr<FFmpegKitStreamInput>>(
            "shared_ptr<FFmpegKitStreamInput>")
        .class_function("create", &streamInput_create)
        .class_function("createWithCapacity", &streamInput_createWithCapacity)
        .function("getUrl", &FFmpegKitStreamInput::getUrl)
        .function("write", &streamInput_write)
        .function("closeInput", &FFmpegKitStreamInput::closeInput)
        .function("close", &FFmpegKitStreamInput::close);

    // Non-seekable streaming output: pump read() from the host worker while
    // FFmpeg fills the ring on its own pthread.
    class_<FFmpegKitStreamOutput>("FFmpegKitStreamOutput")
        .smart_ptr<std::shared_ptr<FFmpegKitStreamOutput>>(
            "shared_ptr<FFmpegKitStreamOutput>")
        .class_function("create", &streamOutput_create)
        .class_function("createWithCapacity", &streamOutput_createWithCapacity)
        .function("getUrl", &FFmpegKitStreamOutput::getUrl)
        .function("read", &streamOutput_read)
        .function("close", &FFmpegKitStreamOutput::close);

    // ---- Sessions -----------------------------------------------------------
    // Common accessors live on AbstractSession; subclasses inherit them in JS
    // via base<AbstractSession>.
    class_<AbstractSession>("AbstractSession")
        .smart_ptr<std::shared_ptr<AbstractSession>>("shared_ptr<AbstractSession>")
        .function("getSessionId", &AbstractSession::getSessionId)
        .function("getCommand", &AbstractSession::getCommand)
        .function("getState", &AbstractSession::getState)
        .function("getReturnCode", &AbstractSession::getReturnCode)
        .function("getDuration", &AbstractSession::getDuration)
        .function("getOutput", &AbstractSession::getOutput)
        .function("getAllLogsAsString", &AbstractSession::getAllLogsAsString)
        .function("getLogsAsString", &AbstractSession::getLogsAsString)
        .function("getFailStackTrace", &AbstractSession::getFailStackTrace)
        .function("isFFmpeg", &AbstractSession::isFFmpeg)
        .function("isFFprobe", &AbstractSession::isFFprobe)
        .function("isMediaInformation", &AbstractSession::isMediaInformation)
        .function("cancel", &AbstractSession::cancel)
        .function("getAllLogs", &session_getAllLogs)
        .function("getLogs", &session_getLogs);

    class_<FFmpegSession, base<AbstractSession>>("FFmpegSession")
        .smart_ptr<std::shared_ptr<FFmpegSession>>("shared_ptr<FFmpegSession>")
        .function("getStatistics", &ffmpegSession_getStatistics)
        .function("getAllStatistics", &ffmpegSession_getAllStatistics)
        .function("getLastReceivedStatistics", &FFmpegSession::getLastReceivedStatistics);

    class_<FFprobeSession, base<AbstractSession>>("FFprobeSession")
        .smart_ptr<std::shared_ptr<FFprobeSession>>("shared_ptr<FFprobeSession>");

    class_<MediaInformationSession, base<AbstractSession>>("MediaInformationSession")
        .smart_ptr<std::shared_ptr<MediaInformationSession>>("shared_ptr<MediaInformationSession>")
        .function("getMediaInformation", &MediaInformationSession::getMediaInformation);

    // ---- Entry points -------------------------------------------------------
    class_<FFmpegKit>("FFmpegKit")
        .class_function("execute", &ffmpegKit_execute)
        .class_function("executeAsync", &ffmpegKit_executeAsync)
        .class_function("cancel", &ffmpegKit_cancel)
        .class_function("cancelSession", &ffmpegKit_cancelSession)
        .class_function("listSessions", &ffmpegKit_listSessions);

    class_<FFprobeKit>("FFprobeKit")
        .class_function("execute", &ffprobeKit_execute)
        .class_function("getMediaInformation", &ffprobeKit_getMediaInformation);

    // Public FFmpegKitConfig surface — mirrors the native API only (no web-only
    // methods here, to keep the JS API identical to the other platforms).
    class_<FFmpegKitConfig>("FFmpegKitConfig")
        .class_function("setLogLevel", &FFmpegKitConfig::setLogLevel)
        .class_function("setFontconfigConfigurationPath",
                        &FFmpegKitConfig::setFontconfigConfigurationPath)
        .class_function("setFontDirectory", &config_setFontDirectory)
        .class_function("setFontDirectoryList", &config_setFontDirectoryList)
        .class_function("getLogLevel", &FFmpegKitConfig::getLogLevel)
        .class_function("getVersion", &FFmpegKitConfig::getVersion)
        .class_function("getFFmpegVersion", &FFmpegKitConfig::getFFmpegVersion)
        .class_function("getBuildDate", &FFmpegKitConfig::getBuildDate);

    // Internal event bridge — nameless free functions on Module, not public API.
    // FFmpeg fires log/statistics std::function callbacks on its dedicated callback
    // pthread, and embind vals cannot be invoked cross-thread; the installer queues
    // events into a mutex-guarded buffer and the worker host (FFmpegKitWorker) pulls
    // batches with the drain functions from its own thread (e.g. the executeAsync
    // poll loop). Consumed only by the worker glue, never by app code.
    function("_ffmpegkitEnableEventBuffering", &config_enableEventBuffering);
    function("_ffmpegkitDrainLogEvents", &config_drainLogEvents);
    function("_ffmpegkitDrainStatisticsEvents", &config_drainStatisticsEvents);
}

/*
 * DCE anchor. Under -sMAIN_MODULE=2 the linker drops object files whose symbols
 * are never referenced, which would silently strip the EMSCRIPTEN_BINDINGS static
 * initializer above. Keeping one exported symbol in this translation unit forces
 * the object (and therefore the registration) to be retained. The build links
 * libffmpegkit whole; if a future change stops honoring that, reference this from
 * the main-module link or add it to -sEXPORTED_FUNCTIONS.
 */
extern "C" EMSCRIPTEN_KEEPALIVE void ffmpegkit_bindings_anchor(void) {}
