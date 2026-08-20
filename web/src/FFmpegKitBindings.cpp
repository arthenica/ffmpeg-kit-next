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
 * / drainLogEvents / drainStatisticsEvents), and direct session-delete forwarding.
 * See the note near those bindings for why log/stat callbacks are surfaced
 * differently from the delete listener.
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

#include <chrono>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "ArchDetect.h"
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
#include "MediaInformationJsonParser.h"
#include "MediaInformationSession.h"
#include "Packages.h"
#include "ReturnCode.h"
#include "SessionDeleteListener.h"
#include "SessionState.h"
#include "Statistics.h"
#include "StreamInformation.h"
#include "json/Value.h"

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

val jsonValueToVal(const ffmpegkit::json::Value &value) {
    if (value.isNull()) {
        return val::null();
    }

    if (value.isBool()) {
        auto boolValue = value.getBool();
        return boolValue ? val(*boolValue) : val::null();
    }

    if (value.isInt()) {
        auto intValue = value.getInt();
        return intValue ? val(static_cast<double>(*intValue)) : val::null();
    }

    if (value.isDouble()) {
        auto doubleValue = value.getDouble();
        return doubleValue ? val(*doubleValue) : val::null();
    }

    if (value.isString()) {
        auto stringValue = value.getString();
        return stringValue ? val(*stringValue) : val::null();
    }

    if (value.isArray()) {
        val array = val::array();
        int index = 0;
        for (const auto &item : value.getArray()) {
            array.set(index++, jsonValueToVal(item));
        }
        return array;
    }

    if (value.isObject()) {
        val object = val::object();
        for (const auto &member : value.getObject()) {
            object.set(member.first, jsonValueToVal(member.second));
        }
        return object;
    }

    return val::null();
}

val optJsonValue(const std::shared_ptr<ffmpegkit::json::Value> &value) {
    return value ? jsonValueToVal(*value) : val::null();
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
val sessionIdsToArray(
    const std::shared_ptr<std::list<std::shared_ptr<T>>> &sessions) {
    val array = val::array();
    if (sessions) {
        int index = 0;
        for (const auto &session : *sessions) {
            if (session) {
                array.set(index++,
                          static_cast<double>(session->getSessionId()));
            }
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

val stringSetToArray(const std::shared_ptr<std::set<std::string>> &items) {
    val array = val::array();
    if (items) {
        int index = 0;
        for (const auto &item : *items) {
            array.set(index++, item);
        }
    }
    return array;
}

// ---- Enums on the JS boundary --------------------------------------------------
// NO bound function takes or returns a C++ enum. They all use plain ints, exactly as
// the Flutter and React Native platform channels do (their plugins convert
// int <-> enum on the native side too).
//
// embind exposes an unscoped enum as a value OBJECT with a non-enumerable `value`
// property, which breaks the boundary silently in both directions: a JS number
// arriving at such a parameter is converted with `value.value` -> undefined -> 0
// (AV_LOG_PANIC for Level, Created for SessionState), and a returned value object
// loses `value` when the worker structured-clones it to the main thread. Module.Level
// and Module.SessionState remain registered for inspection only.

int log_getLevel(Log &self) { return static_cast<int>(self.getLevel()); }

int session_getState(AbstractSession &self) {
    return static_cast<int>(self.getState());
}

int config_getLogLevel() {
    return static_cast<int>(FFmpegKitConfig::getLogLevel());
}

// av_log_set_level() accepts any int, so the value is passed through unvalidated -
// matching the Apple plugin, whose setLogLevel: also takes a plain int.
void config_setLogLevel(const int level) {
    FFmpegKitConfig::setLogLevel(static_cast<Level>(level));
}

// ---- Session accessors that return collections (bound as methods) ----------
//
// Only the non-waiting getters are bound. The getAll* family - getAllLogs,
// getAllLogsAsString, getAllStatistics, getOutput and their WithTimeout overloads - opens
// with AbstractSession::waitForAsynchronousMessagesInTransmit(), which sleeps the calling
// thread in 100 ms steps. The only JS caller is the worker's message handler, and that is
// the one thread running the worker event loop: blocking it stops cancel handling,
// ffkitstream: I/O servicing and the event drain that feeds live log and statistics
// callbacks. It also cannot succeed at its own job, because on web "messages in transmit"
// includes the events buffered in C++ that only the worker thread can drain (see
// config_pendingEventCount) - a blocking wait holds the very thread that would make the
// count fall.
//
// The JS layer still honours waitTimeout; the worker just does the waiting itself, as a
// poll that drains on every tick. See waitForMessagesInTransmit() in ffmpegkit.worker.js.
// Do not bind the waiting variants.
val session_getLogs(AbstractSession &self) { return listToArray(self.getLogs()); }
val ffmpegSession_getStatistics(FFmpegSession &self) { return listToArray(self.getStatistics()); }
std::string packages_getPackageName() { return Packages::getPackageName(); }
val packages_getExternalLibraries() { return stringSetToArray(Packages::getExternalLibraries()); }

// Session timestamps as epoch milliseconds (doubles). JS wraps them in Date; a value
// of 0 means the timestamp is not set yet (e.g. end time before completion).
double session_timeMillis(
    const std::chrono::time_point<std::chrono::system_clock> &tp) {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
            .count());
}
double session_getCreateTimeMillis(AbstractSession &self) { return session_timeMillis(self.getCreateTime()); }
double session_getStartTimeMillis(AbstractSession &self) { return session_timeMillis(self.getStartTime()); }
double session_getEndTimeMillis(AbstractSession &self) { return session_timeMillis(self.getEndTime()); }

val session_getArguments(AbstractSession &self) {
    val array = val::array();
    const auto arguments = self.getArguments();
    if (arguments) {
        int index = 0;
        for (const auto &argument : *arguments) {
            array.set(index++, argument);
        }
    }
    return array;
}

std::list<std::string> jsArrayToStringList(const val &array);

LogRedirectionStrategy logRedirectionStrategyFromInt(const int strategy) {
    switch (strategy) {
    case LogRedirectionStrategyAlwaysPrintLogs:
    case LogRedirectionStrategyPrintLogsWhenNoCallbacksDefined:
    case LogRedirectionStrategyPrintLogsWhenGlobalCallbackNotDefined:
    case LogRedirectionStrategyPrintLogsWhenSessionCallbackNotDefined:
    case LogRedirectionStrategyNeverPrintLogs:
        return static_cast<LogRedirectionStrategy>(strategy);
    default:
        return FFmpegKitConfig::getLogRedirectionStrategy();
    }
}

SessionState sessionStateFromInt(const int state) {
    switch (state) {
    case SessionStateCreated:
    case SessionStateRunning:
    case SessionStateFailed:
    case SessionStateCompleted:
        return static_cast<SessionState>(state);
    default:
        return SessionStateCompleted;
    }
}

std::shared_ptr<FFmpegSession>
ffmpegSession_create(const val arguments, const int logRedirectionStrategy) {
    return FFmpegSession::create(
        jsArrayToStringList(arguments), nullptr, nullptr, nullptr,
        logRedirectionStrategyFromInt(logRedirectionStrategy));
}

std::shared_ptr<FFprobeSession>
ffprobeSession_create(const val arguments, const int logRedirectionStrategy) {
    return FFprobeSession::create(
        jsArrayToStringList(arguments), nullptr, nullptr,
        logRedirectionStrategyFromInt(logRedirectionStrategy));
}

std::shared_ptr<MediaInformationSession>
mediaInformationSession_create(const val arguments) {
    return MediaInformationSession::create(jsArrayToStringList(arguments));
}

std::shared_ptr<FFmpegSession> config_getFFmpegSession(const long sessionId) {
    auto session = FFmpegKitConfig::getSession(sessionId);
    if (session == nullptr || !session->isFFmpeg()) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<FFmpegSession>(session);
}

std::shared_ptr<FFprobeSession> config_getFFprobeSession(const long sessionId) {
    auto session = FFmpegKitConfig::getSession(sessionId);
    if (session == nullptr || !session->isFFprobe()) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<FFprobeSession>(session);
}

std::shared_ptr<MediaInformationSession>
config_getMediaInformationSession(const long sessionId) {
    auto session = FFmpegKitConfig::getSession(sessionId);
    if (session == nullptr || !session->isMediaInformation()) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<MediaInformationSession>(session);
}

// ---- Execute -----------------------------------------------------------------
// Only the async forms are bound. The synchronous ones - FFmpegKitConfig::ffmpegExecute,
// ffprobeExecute and getMediaInformationExecute - run the command on the calling thread,
// and the only JS caller here is the worker's message handler. Blocking that handler for
// the length of a transcode stops the worker servicing its own event loop, which is
// where cancel requests arrive, where drainAndForward() delivers live log and statistics
// events, and where ioStreamWrite/ioStreamRead move bytes for ffkitstream: I/O. A
// streaming input would deadlock outright: FFmpeg blocks waiting for bytes that can only
// arrive on a postMessage the blocked handler can never process.
//
// The JS layer still offers the synchronous contract (FFmpegKitConfig.ffmpegExecute()
// resolves only once the run completes); it is built on top of these async forms, by
// starting the session and polling it to completion. See waitForSession() in
// ffmpegkit.worker.js. Do not bind the synchronous forms "for completeness".
void config_asyncFFmpegExecuteSession(
    const std::shared_ptr<FFmpegSession> session) {
    FFmpegKitConfig::asyncFFmpegExecute(session);
}

void config_asyncFFprobeExecuteSession(
    const std::shared_ptr<FFprobeSession> session) {
    FFmpegKitConfig::asyncFFprobeExecute(session);
}

void config_asyncGetMediaInformationExecuteSession(
    const std::shared_ptr<MediaInformationSession> session,
    const int waitTimeout) {
    FFmpegKitConfig::asyncGetMediaInformationExecute(session, waitTimeout);
}

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

val config_getSessionIds() {
    return sessionIdsToArray(FFmpegKitConfig::getSessions());
}

val config_getLastSessionId() {
    auto session = FFmpegKitConfig::getLastSession();
    if (!session) {
        return val::null();
    }
    return val(static_cast<double>(session->getSessionId()));
}

val config_getLastCompletedSessionId() {
    auto session = FFmpegKitConfig::getLastCompletedSession();
    if (!session) {
        return val::null();
    }
    return val(static_cast<double>(session->getSessionId()));
}

val config_getSessionsByStateIds(const int state) {
    return sessionIdsToArray(
        FFmpegKitConfig::getSessionsByState(sessionStateFromInt(state)));
}

val config_getFFmpegSessionIds() {
    return sessionIdsToArray(FFmpegKitConfig::getFFmpegSessions());
}

val config_getFFprobeSessionIds() {
    return sessionIdsToArray(FFmpegKitConfig::getFFprobeSessions());
}

val config_getMediaInformationSessionIds() {
    return sessionIdsToArray(FFmpegKitConfig::getMediaInformationSessions());
}

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
            result.push_back(item.as<std::string>());
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

// No config_setFontDirectory() counterpart on purpose. Native setFontDirectory() is
// setFontDirectoryList() with a one-element list, and FFmpegKitConfig.setFontDirectory()
// on the JS side already fans out to the list form, so a second binding would only be a
// second way to reach the same code - one the worker never takes.
void config_setFontDirectoryList(const val fontDirectoryList,
                                 const val fontNameMapping) {
    FFmpegKitConfig::setFontDirectoryList(
        jsArrayToStringList(fontDirectoryList),
        jsObjectToStringMap(fontNameMapping));
}

// ---- Arguments-array + async execute wrappers -------------------------------
// jsArrayToStringList (above) converts a JS string array to std::list<string>. The
// async wrappers pass a no-op complete callback because delivering it to JS would need
// cross-thread val proxying; the JS host observes completion by polling the session.

std::shared_ptr<FFmpegSession> ffmpegKit_executeWithArguments(const val arguments) {
    return FFmpegKit::executeWithArguments(jsArrayToStringList(arguments));
}
std::shared_ptr<FFmpegSession> ffmpegKit_executeWithArgumentsAsync(const val arguments) {
    return FFmpegKit::executeWithArgumentsAsync(
        jsArrayToStringList(arguments), [](std::shared_ptr<ffmpegkit::FFmpegSession>) {});
}
std::shared_ptr<FFprobeSession> ffprobeKit_executeWithArguments(const val arguments) {
    return FFprobeKit::executeWithArguments(jsArrayToStringList(arguments));
}
std::shared_ptr<FFprobeSession> ffprobeKit_executeAsync(const std::string command) {
    return FFprobeKit::executeAsync(
        command, [](std::shared_ptr<ffmpegkit::FFprobeSession>) {});
}
std::shared_ptr<FFprobeSession> ffprobeKit_executeWithArgumentsAsync(const val arguments) {
    return FFprobeKit::executeWithArgumentsAsync(
        jsArrayToStringList(arguments),
        [](std::shared_ptr<ffmpegkit::FFprobeSession>) {});
}
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformationWithTimeout(const std::string path, const int waitTimeout) {
    return FFprobeKit::getMediaInformation(path, waitTimeout);
}
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformationFromCommand(const std::string command) {
    return FFprobeKit::getMediaInformationFromCommand(command);
}
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformationAsync(const std::string path) {
    return FFprobeKit::getMediaInformationAsync(
        path, [](std::shared_ptr<ffmpegkit::MediaInformationSession>) {});
}

// Arguments-based media information: FFprobeKit has no from-command-arguments entry
// point, so compose it from the session factory + the FFmpegKitConfig execute primitive
// (exactly how the Flutter/RN plugins build it). The async variant returns immediately;
// the JS host polls the session to completion.
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformationFromCommandArguments(const val arguments,
                                                   const int waitTimeout) {
    auto session = MediaInformationSession::create(jsArrayToStringList(arguments));
    FFmpegKitConfig::getMediaInformationExecute(session, waitTimeout);
    return session;
}
std::shared_ptr<MediaInformationSession>
ffprobeKit_getMediaInformationFromCommandArgumentsAsync(const val arguments,
                                                        const int waitTimeout) {
    auto session = MediaInformationSession::create(jsArrayToStringList(arguments));
    FFmpegKitConfig::asyncGetMediaInformationExecute(session, waitTimeout);
    return session;
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
val mediaInformation_getTags(MediaInformation &self) { return optJsonValue(self.getTags()); }
val mediaInformation_getStreams(MediaInformation &self) { return vectorToArray(self.getStreams()); }
val mediaInformation_getChapters(MediaInformation &self) { return vectorToArray(self.getChapters()); }
val mediaInformation_getStringProperty(MediaInformation &self, const std::string key) {
    return optString(self.getStringProperty(key.c_str()));
}
val mediaInformation_getNumberProperty(MediaInformation &self, const std::string key) {
    return optInt64(self.getNumberProperty(key.c_str()));
}
val mediaInformation_getProperty(MediaInformation &self, const std::string key) {
    return optJsonValue(self.getProperty(key.c_str()));
}
val mediaInformation_getStringFormatProperty(MediaInformation &self, const std::string key) {
    return optString(self.getStringFormatProperty(key.c_str()));
}
val mediaInformation_getNumberFormatProperty(MediaInformation &self, const std::string key) {
    return optInt64(self.getNumberFormatProperty(key.c_str()));
}
val mediaInformation_getFormatProperty(MediaInformation &self, const std::string key) {
    return optJsonValue(self.getFormatProperty(key.c_str()));
}
val mediaInformation_getFormatProperties(MediaInformation &self) {
    return optJsonValue(self.getFormatProperties());
}
val mediaInformation_getAllProperties(MediaInformation &self) {
    return optJsonValue(self.getAllProperties());
}

std::shared_ptr<MediaInformation>
mediaInformationJsonParser_from(const std::string ffprobeJsonOutput) {
    return MediaInformationJsonParser::from(ffprobeJsonOutput);
}

std::shared_ptr<MediaInformation>
mediaInformationJsonParser_fromWithError(const std::string ffprobeJsonOutput) {
    return MediaInformationJsonParser::fromWithError(ffprobeJsonOutput);
}

val streamInformation_getIndex(StreamInformation &self) { return optInt64(self.getIndex()); }
val streamInformation_getType(StreamInformation &self) { return optString(self.getType()); }
val streamInformation_getCodec(StreamInformation &self) { return optString(self.getCodec()); }
val streamInformation_getCodecLong(StreamInformation &self) { return optString(self.getCodecLong()); }
val streamInformation_getFormat(StreamInformation &self) { return optString(self.getFormat()); }
val streamInformation_getWidth(StreamInformation &self) { return optInt64(self.getWidth()); }
val streamInformation_getHeight(StreamInformation &self) { return optInt64(self.getHeight()); }
val streamInformation_getBitrate(StreamInformation &self) { return optString(self.getBitrate()); }
val streamInformation_getSampleRate(StreamInformation &self) { return optString(self.getSampleRate()); }
val streamInformation_getSampleFormat(StreamInformation &self) { return optString(self.getSampleFormat()); }
val streamInformation_getChannelLayout(StreamInformation &self) { return optString(self.getChannelLayout()); }
val streamInformation_getSampleAspectRatio(StreamInformation &self) { return optString(self.getSampleAspectRatio()); }
val streamInformation_getDisplayAspectRatio(StreamInformation &self) { return optString(self.getDisplayAspectRatio()); }
val streamInformation_getAverageFrameRate(StreamInformation &self) { return optString(self.getAverageFrameRate()); }
val streamInformation_getRealFrameRate(StreamInformation &self) { return optString(self.getRealFrameRate()); }
val streamInformation_getTimeBase(StreamInformation &self) { return optString(self.getTimeBase()); }
val streamInformation_getCodecTimeBase(StreamInformation &self) { return optString(self.getCodecTimeBase()); }
val streamInformation_getTags(StreamInformation &self) { return optJsonValue(self.getTags()); }
val streamInformation_getStringProperty(StreamInformation &self, const std::string key) {
    return optString(self.getStringProperty(key.c_str()));
}
val streamInformation_getNumberProperty(StreamInformation &self, const std::string key) {
    return optInt64(self.getNumberProperty(key.c_str()));
}
val streamInformation_getProperty(StreamInformation &self, const std::string key) {
    return optJsonValue(self.getProperty(key.c_str()));
}
val streamInformation_getAllProperties(StreamInformation &self) {
    return optJsonValue(self.getAllProperties());
}

val chapter_getId(Chapter &self) { return optInt64(self.getId()); }
val chapter_getTimeBase(Chapter &self) { return optString(self.getTimeBase()); }
val chapter_getStart(Chapter &self) { return optInt64(self.getStart()); }
val chapter_getStartTime(Chapter &self) { return optString(self.getStartTime()); }
val chapter_getEnd(Chapter &self) { return optInt64(self.getEnd()); }
val chapter_getEndTime(Chapter &self) { return optString(self.getEndTime()); }
val chapter_getTags(Chapter &self) { return optJsonValue(self.getTags()); }
val chapter_getStringProperty(Chapter &self, const std::string key) {
    return optString(self.getStringProperty(key.c_str()));
}
val chapter_getNumberProperty(Chapter &self, const std::string key) {
    return optInt64(self.getNumberProperty(key.c_str()));
}
val chapter_getProperty(Chapter &self, const std::string key) {
    return optJsonValue(self.getProperty(key.c_str()));
}
val chapter_getAllProperties(Chapter &self) { return optJsonValue(self.getAllProperties()); }

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
// drainStatisticsEvents from its own thread, on its own schedule (the worker's
// native-session poll loop). No cross-thread val calls, no Asyncify, no proxying.
//
// This is the "live progress" path; getAllLogsAsString / getStatistics on the
// session remain the source of truth for the final, complete record.

std::mutex g_eventMutex;
std::vector<std::shared_ptr<Log>> g_logEvents;
std::vector<std::shared_ptr<Statistics>> g_statisticsEvents;
bool g_eventBufferingEnabled = false;

// Session deletion is forwarded directly because native history mutations are
// made from worker-owned entrypoints. If that changes to fire from FFmpeg pthreads,
// this should use the same buffer/drain shape as log/statistics events.
class JsSessionDeleteListener : public SessionDeleteListener {
  public:
    explicit JsSessionDeleteListener(val callback) : _callback{callback} {}

    void sessionDeleted(const long sessionId) override {
        _callback.call<void>("call", val::undefined(),
                             static_cast<double>(sessionId));
    }

  private:
    val _callback;
};

std::shared_ptr<SessionDeleteListener> g_sessionDeleteListener;

// These functions are the internal event bridge. They are bound as nameless
// module-level free functions (underscore-prefixed) rather than on a class,
// because they have no counterpart in the native FFmpegKitConfig API and must not
// appear on the public surface. They are called only from the worker host
// (FFmpegKitWorker); app code never reaches them. embind cannot truly hide a
// bound symbol, so "internal" means "kept off the public classes and out of the
// published typings".
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

// Number of buffered events for a session that have not been drained to JS yet.
//
// FFmpegKitConfig::messagesInTransmit() alone is not the whole picture on web. Native
// decrements its in-transit counter as soon as the callback returns, and here that
// callback only appends to the buffers above - so the native count reaches 0 while
// events are still waiting for the host worker to drain them. The worker adds this
// count to the native one so messagesInTransmit() keeps its cross-platform meaning:
// "messages not delivered to the JS callbacks yet".
int config_pendingEventCount(const long sessionId) {
    std::lock_guard<std::mutex> lock(g_eventMutex);
    int count = 0;
    for (const auto &log : g_logEvents) {
        if (log->getSessionId() == sessionId) {
            count++;
        }
    }
    for (const auto &statistics : g_statisticsEvents) {
        if (statistics->getSessionId() == sessionId) {
            count++;
        }
    }
    return count;
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

void config_setSessionDeletedCallback(val callback) {
    if (g_sessionDeleteListener != nullptr) {
        FFmpegKitConfig::removeSessionDeleteListener(g_sessionDeleteListener);
        g_sessionDeleteListener.reset();
    }

    if (callback.isNull() || callback.isUndefined()) {
        return;
    }

    g_sessionDeleteListener =
        std::make_shared<JsSessionDeleteListener>(callback);
    FFmpegKitConfig::addSessionDeleteListener(g_sessionDeleteListener);
}

} // namespace

EMSCRIPTEN_BINDINGS(ffmpegkit_bindings) {

    // ---- Enums --------------------------------------------------------------
    // All three are registered for inspection only. Nothing bound below accepts or
    // returns any of them - every enum crosses the boundary as a plain int (see the
    // enum note above).
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
        // Plain int, not ffmpegkit::Level - see the log-level note above.
        .function("getLevel", &log_getLevel)
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
        .function("getSampleFormat", &streamInformation_getSampleFormat)
        .function("getChannelLayout", &streamInformation_getChannelLayout)
        .function("getSampleAspectRatio", &streamInformation_getSampleAspectRatio)
        .function("getDisplayAspectRatio", &streamInformation_getDisplayAspectRatio)
        .function("getAverageFrameRate", &streamInformation_getAverageFrameRate)
        .function("getRealFrameRate", &streamInformation_getRealFrameRate)
        .function("getTimeBase", &streamInformation_getTimeBase)
        .function("getCodecTimeBase", &streamInformation_getCodecTimeBase)
        .function("getTags", &streamInformation_getTags)
        .function("getStringProperty", &streamInformation_getStringProperty)
        .function("getNumberProperty", &streamInformation_getNumberProperty)
        .function("getProperty", &streamInformation_getProperty)
        .function("getAllProperties", &streamInformation_getAllProperties);

    class_<Chapter>("Chapter")
        .smart_ptr<std::shared_ptr<Chapter>>("shared_ptr<Chapter>")
        .function("getId", &chapter_getId)
        .function("getTimeBase", &chapter_getTimeBase)
        .function("getStart", &chapter_getStart)
        .function("getStartTime", &chapter_getStartTime)
        .function("getEnd", &chapter_getEnd)
        .function("getEndTime", &chapter_getEndTime)
        .function("getTags", &chapter_getTags)
        .function("getStringProperty", &chapter_getStringProperty)
        .function("getNumberProperty", &chapter_getNumberProperty)
        .function("getProperty", &chapter_getProperty)
        .function("getAllProperties", &chapter_getAllProperties);

    class_<MediaInformation>("MediaInformation")
        .smart_ptr<std::shared_ptr<MediaInformation>>("shared_ptr<MediaInformation>")
        .function("getFilename", &mediaInformation_getFilename)
        .function("getFormat", &mediaInformation_getFormat)
        .function("getLongFormat", &mediaInformation_getLongFormat)
        .function("getDuration", &mediaInformation_getDuration)
        .function("getStartTime", &mediaInformation_getStartTime)
        .function("getSize", &mediaInformation_getSize)
        .function("getBitrate", &mediaInformation_getBitrate)
        .function("getTags", &mediaInformation_getTags)
        .function("getStreams", &mediaInformation_getStreams)
        .function("getChapters", &mediaInformation_getChapters)
        .function("getStringProperty", &mediaInformation_getStringProperty)
        .function("getNumberProperty", &mediaInformation_getNumberProperty)
        .function("getProperty", &mediaInformation_getProperty)
        .function("getStringFormatProperty", &mediaInformation_getStringFormatProperty)
        .function("getNumberFormatProperty", &mediaInformation_getNumberFormatProperty)
        .function("getFormatProperty", &mediaInformation_getFormatProperty)
        .function("getFormatProperties", &mediaInformation_getFormatProperties)
        .function("getAllProperties", &mediaInformation_getAllProperties);

    class_<MediaInformationJsonParser>("MediaInformationJsonParser")
        .class_function("from", &mediaInformationJsonParser_from)
        .class_function("fromWithError", &mediaInformationJsonParser_fromWithError);

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
        .function("getArguments", &session_getArguments)
        // Plain int, not ffmpegkit::SessionState - see the enum note above.
        .function("getState", &session_getState)
        .function("getReturnCode", &AbstractSession::getReturnCode)
        .function("getDuration", &AbstractSession::getDuration)
        .function("getCreateTimeMillis", &session_getCreateTimeMillis)
        .function("getStartTimeMillis", &session_getStartTimeMillis)
        .function("getEndTimeMillis", &session_getEndTimeMillis)
        // Non-waiting getters only - see the session-accessor note above.
        .function("getLogsAsString", &AbstractSession::getLogsAsString)
        .function("getFailStackTrace", &AbstractSession::getFailStackTrace)
        .function("isFFmpeg", &AbstractSession::isFFmpeg)
        .function("isFFprobe", &AbstractSession::isFFprobe)
        .function("isMediaInformation", &AbstractSession::isMediaInformation)
        .function("cancel", &AbstractSession::cancel)
        .function("getLogs", &session_getLogs);

    class_<FFmpegSession, base<AbstractSession>>("FFmpegSession")
        .smart_ptr<std::shared_ptr<FFmpegSession>>("shared_ptr<FFmpegSession>")
        .class_function("create", &ffmpegSession_create)
        .function("getStatistics", &ffmpegSession_getStatistics)
        .function("getLastReceivedStatistics", &FFmpegSession::getLastReceivedStatistics);

    class_<FFprobeSession, base<AbstractSession>>("FFprobeSession")
        .smart_ptr<std::shared_ptr<FFprobeSession>>("shared_ptr<FFprobeSession>")
        .class_function("create", &ffprobeSession_create);

    class_<MediaInformationSession, base<AbstractSession>>("MediaInformationSession")
        .smart_ptr<std::shared_ptr<MediaInformationSession>>("shared_ptr<MediaInformationSession>")
        .class_function("create", &mediaInformationSession_create)
        .function("getMediaInformation", &MediaInformationSession::getMediaInformation);

    // ---- Entry points -------------------------------------------------------
    class_<FFmpegKit>("FFmpegKit")
        .class_function("execute", &ffmpegKit_execute)
        .class_function("executeWithArguments", &ffmpegKit_executeWithArguments)
        .class_function("executeAsync", &ffmpegKit_executeAsync)
        .class_function("executeWithArgumentsAsync", &ffmpegKit_executeWithArgumentsAsync)
        .class_function("cancel", &ffmpegKit_cancel)
        .class_function("cancelSession", &ffmpegKit_cancelSession)
        .class_function("listSessions", &ffmpegKit_listSessions);

    class_<FFprobeKit>("FFprobeKit")
        .class_function("execute", &ffprobeKit_execute)
        .class_function("executeWithArguments", &ffprobeKit_executeWithArguments)
        .class_function("executeAsync", &ffprobeKit_executeAsync)
        .class_function("executeWithArgumentsAsync", &ffprobeKit_executeWithArgumentsAsync)
        .class_function("getMediaInformation", &ffprobeKit_getMediaInformation)
        .class_function("getMediaInformationWithTimeout", &ffprobeKit_getMediaInformationWithTimeout)
        .class_function("getMediaInformationFromCommand", &ffprobeKit_getMediaInformationFromCommand)
        .class_function("getMediaInformationAsync", &ffprobeKit_getMediaInformationAsync)
        .class_function("getMediaInformationFromCommandArguments", &ffprobeKit_getMediaInformationFromCommandArguments)
        .class_function("getMediaInformationFromCommandArgumentsAsync", &ffprobeKit_getMediaInformationFromCommandArgumentsAsync);

    class_<ArchDetect>("ArchDetect")
        .class_function("getArch", &ArchDetect::getArch);

    class_<Packages>("Packages")
        .class_function("getPackageName", &packages_getPackageName)
        .class_function("getExternalLibraries", &packages_getExternalLibraries);

    // Public FFmpegKitConfig surface — mirrors the native API only (no web-only
    // methods here, to keep the JS API identical to the other platforms).
    class_<FFmpegKitConfig>("FFmpegKitConfig")
        .class_function("enableRedirection", &FFmpegKitConfig::enableRedirection)
        .class_function("disableRedirection", &FFmpegKitConfig::disableRedirection)
        // Plain int in / plain int out, not ffmpegkit::Level - see the log-level
        // note above.
        .class_function("setLogLevel", &config_setLogLevel)
        .class_function("setEnvironmentVariable",
                        &FFmpegKitConfig::setEnvironmentVariable)
        .class_function("setFontconfigConfigurationPath",
                        &FFmpegKitConfig::setFontconfigConfigurationPath)
        .class_function("setFontDirectoryList", &config_setFontDirectoryList)
        // Async execute only - see the execute note above.
        .class_function("asyncFFmpegExecuteSession",
                        &config_asyncFFmpegExecuteSession)
        .class_function("asyncFFprobeExecuteSession",
                        &config_asyncFFprobeExecuteSession)
        .class_function("asyncGetMediaInformationExecuteSession",
                        &config_asyncGetMediaInformationExecuteSession)
        .class_function("getFFmpegSession", &config_getFFmpegSession)
        .class_function("getFFprobeSession", &config_getFFprobeSession)
        .class_function("getMediaInformationSession",
                        &config_getMediaInformationSession)
        .class_function("getSessionHistorySize",
                        &FFmpegKitConfig::getSessionHistorySize)
        .class_function("getSessionIds", &config_getSessionIds)
        .class_function("getLastSessionId", &config_getLastSessionId)
        .class_function("getLastCompletedSessionId",
                        &config_getLastCompletedSessionId)
        .class_function("getSessionsByStateIds",
                        &config_getSessionsByStateIds)
        .class_function("getFFmpegSessionIds", &config_getFFmpegSessionIds)
        .class_function("getFFprobeSessionIds", &config_getFFprobeSessionIds)
        .class_function("getMediaInformationSessionIds",
                        &config_getMediaInformationSessionIds)
        .class_function("setSessionHistorySize",
                        &FFmpegKitConfig::setSessionHistorySize)
        .class_function("clearSessions", &FFmpegKitConfig::clearSessions)
        .class_function("deleteSession", &FFmpegKitConfig::deleteSession)
        .class_function("messagesInTransmit",
                        &FFmpegKitConfig::messagesInTransmit)
        .class_function("getLogLevel", &config_getLogLevel)
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
    function("_ffmpegkitPendingEventCount", &config_pendingEventCount);
    function("_ffmpegkitSetSessionDeletedCallback",
             &config_setSessionDeletedCallback);
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
