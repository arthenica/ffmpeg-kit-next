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
 * Implementation of the flat C API declared in api/ffmpegkit_c.h.
 *
 * This is the only translation unit that sits on the ABI boundary. It converts
 * between the C representation (opaque handles, ints, UTF-8 strings, function
 * pointers) and the C++ implementation classes in ffmpegkit::internal, and it
 * makes sure no C++ exception ever leaves the library.
 */

#include "ffmpegkit_c.h"

#include "internal/ArchDetect.h"
#include "internal/Chapter.h"
#include "internal/FFmpegKit.h"
#include "internal/FFmpegKitConfig.h"
#include "internal/FFmpegKitProtocolUrl.h"
#include "internal/FFmpegSession.h"
#include "internal/FFprobeKit.h"
#include "internal/FFprobeSession.h"
#include "internal/Log.h"
#include "internal/MediaInformation.h"
#include "internal/MediaInformationJsonParser.h"
#include "internal/MediaInformationSession.h"
#include "internal/Packages.h"
#include "internal/ReturnCode.h"
#include "internal/Statistics.h"
#include "internal/StreamInformation.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace internal = ffmpegkit::internal;

/* ------------------------------------------------------------------------ */
/* Handle definitions                                                        */
/* ------------------------------------------------------------------------ */

struct FFKSession {
    std::shared_ptr<internal::Session> value;
};
struct FFKLog {
    std::shared_ptr<internal::Log> value;
};
struct FFKStatistics {
    std::shared_ptr<internal::Statistics> value;
};
struct FFKMediaInformation {
    std::shared_ptr<internal::MediaInformation> value;
};
struct FFKStreamInformation {
    std::shared_ptr<internal::StreamInformation> value;
};
struct FFKChapter {
    std::shared_ptr<internal::Chapter> value;
};

struct FFKStringList {
    std::vector<std::string> items;
};
struct FFKSessionList {
    std::vector<std::shared_ptr<internal::Session>> items;
};
struct FFKLogList {
    std::vector<std::shared_ptr<internal::Log>> items;
};
struct FFKStatisticsList {
    std::vector<std::shared_ptr<internal::Statistics>> items;
};
struct FFKStreamInformationList {
    std::vector<std::shared_ptr<internal::StreamInformation>> items;
};
struct FFKChapterList {
    std::vector<std::shared_ptr<internal::Chapter>> items;
};

namespace {

/* ------------------------------------------------------------------------ */
/* Error slot                                                                */
/* ------------------------------------------------------------------------ */

/*
 * Errors are reported per thread. A thread's slot is cleared when it enters an
 * entry point and filled when that entry point catches something, so a caller
 * can always attribute an error to its own last call.
 */
thread_local std::string threadError;
thread_local bool threadErrorSet = false;

void setError(const char *message) {
    threadError = message == nullptr ? "Unknown error." : message;
    threadErrorSet = true;
}

/* ------------------------------------------------------------------------ */
/* Allocation                                                                */
/* ------------------------------------------------------------------------ */

/*
 * Strings and byte buffers are allocated with the library's own malloc and
 * released by ffk_string_free()/ffk_bytes_free(), which are compiled into the
 * library as well. The consumer's CRT is never involved.
 */
char *duplicateString(const char *value, const size_t size) {
    char *copy = static_cast<char *>(std::malloc(size + 1));
    if (copy == nullptr) {
        throw std::bad_alloc();
    }
    std::memcpy(copy, value, size);
    copy[size] = '\0';
    return copy;
}

char *duplicateString(const std::string &value) {
    return duplicateString(value.c_str(), value.size());
}

/** Returns a copy of the string, or NULL when the pointer is empty. */
char *duplicateString(const std::shared_ptr<std::string> &value) {
    return value == nullptr ? nullptr : duplicateString(*value);
}

uint8_t *duplicateBytes(const std::vector<uint8_t> &value) {
    // malloc(0) may legitimately return NULL, which callers read as "absent"
    uint8_t *copy = static_cast<uint8_t *>(std::malloc(value.empty() ? 1 : value.size()));
    if (copy == nullptr) {
        throw std::bad_alloc();
    }
    if (!value.empty()) {
        std::memcpy(copy, value.data(), value.size());
    }
    return copy;
}

/* ------------------------------------------------------------------------ */
/* Exception barrier                                                         */
/* ------------------------------------------------------------------------ */

/**
 * Runs an entry point body with the calling thread's error slot cleared, and
 * converts anything it throws into an error message plus a neutral result.
 */
template <typename Body> auto guard(Body &&body) -> decltype(body()) {
    threadErrorSet = false;
    try {
        return body();
    } catch (const std::exception &exception) {
        setError(exception.what());
    } catch (...) {
        setError("Unknown error raised across the FFmpegKit C API boundary.");
    }
    return decltype(body())();
}

/* ------------------------------------------------------------------------ */
/* Conversion helpers                                                        */
/* ------------------------------------------------------------------------ */

std::list<std::string> toArgumentList(const char *const *arguments,
                                      const size_t argumentCount) {
    std::list<std::string> list;
    for (size_t index = 0; index < argumentCount; index++) {
        const char *argument = arguments == nullptr ? nullptr : arguments[index];
        list.push_back(argument == nullptr ? std::string() : std::string(argument));
    }
    return list;
}

std::map<std::string, std::string> toStringMap(const char *const *keys,
                                               const char *const *values,
                                               const size_t count) {
    std::map<std::string, std::string> mapping;
    for (size_t index = 0; index < count; index++) {
        const char *key = keys == nullptr ? nullptr : keys[index];
        const char *value = values == nullptr ? nullptr : values[index];
        if (key == nullptr) {
            continue;
        }
        mapping[key] = value == nullptr ? std::string() : std::string(value);
    }
    return mapping;
}

int64_t toEpochMilliseconds(
    const std::chrono::time_point<std::chrono::system_clock> &value) {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            value.time_since_epoch())
            .count());
}

/** Serializes a value into JSON text, or returns NULL when there is none. */
char *toJson(const std::shared_ptr<ffmpegkit::json::Value> &value) {
    if (value == nullptr) {
        return nullptr;
    }
    return duplicateString(ffmpegkit::json::detail::serialize(*value));
}

template <typename ListType, typename ElementType>
ListType *makeList(const std::shared_ptr<std::list<ElementType>> &source) {
    std::unique_ptr<ListType> list(new ListType());
    if (source != nullptr) {
        list->items.assign(source->begin(), source->end());
    }
    return list.release();
}

template <typename ListType, typename ElementType>
ListType *makeList(const std::shared_ptr<std::vector<ElementType>> &source) {
    std::unique_ptr<ListType> list(new ListType());
    if (source != nullptr) {
        list->items.assign(source->begin(), source->end());
    }
    return list.release();
}

/** Wraps a shared pointer into a handle, returning NULL for an empty pointer. */
template <typename HandleType, typename ValueType>
HandleType *makeHandle(const std::shared_ptr<ValueType> &value) {
    if (value == nullptr) {
        return nullptr;
    }
    std::unique_ptr<HandleType> handle(new HandleType());
    handle->value = value;
    return handle.release();
}

template <typename SessionType>
std::shared_ptr<SessionType> sessionAs(const FFKSession *session) {
    if (session == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<SessionType>(session->value);
}

/* ------------------------------------------------------------------------ */
/* Callback adapters                                                         */
/* ------------------------------------------------------------------------ */

/*
 * A consumer cookie is held in a shared_ptr whose deleter is the consumer's own
 * free function, so the cookie outlives every std::function copy the library
 * makes and is released by the allocator that created it.
 */
std::shared_ptr<void> adoptUserData(void *userData, ffk_free_cb freeUserData) {
    if (freeUserData == nullptr) {
        // Nothing to release: the consumer keeps ownership of the cookie.
        return std::shared_ptr<void>(userData, [](void *) {});
    }
    return std::shared_ptr<void>(userData, freeUserData);
}

/*
 * The adapters below are the callables stored inside the internal
 * std::function objects. Keeping them as named types lets the getters recover
 * the original C function pointer and cookie with std::function::target().
 */

struct LogCallbackAdapter {
    ffk_log_cb callback;
    void *userData;
    std::shared_ptr<void> userDataOwner;

    void operator()(const std::shared_ptr<internal::Log> log) const {
        std::unique_ptr<FFKLog> handle(new FFKLog());
        handle->value = log;
        try {
            callback(handle.get(), userData);
        } catch (...) {
            // A consumer callback must not unwind back into the library
        }
    }
};

struct StatisticsCallbackAdapter {
    ffk_statistics_cb callback;
    void *userData;
    std::shared_ptr<void> userDataOwner;

    void operator()(const std::shared_ptr<internal::Statistics> statistics) const {
        std::unique_ptr<FFKStatistics> handle(new FFKStatistics());
        handle->value = statistics;
        try {
            callback(handle.get(), userData);
        } catch (...) {
        }
    }
};

template <typename SessionType> struct SessionCallbackAdapter {
    ffk_session_cb callback;
    void *userData;
    std::shared_ptr<void> userDataOwner;

    void operator()(const std::shared_ptr<SessionType> session) const {
        std::unique_ptr<FFKSession> handle(new FFKSession());
        handle->value = session;
        try {
            callback(handle.get(), userData);
        } catch (...) {
        }
    }
};

using FFmpegSessionCallbackAdapter =
    SessionCallbackAdapter<internal::FFmpegSession>;
using FFprobeSessionCallbackAdapter =
    SessionCallbackAdapter<internal::FFprobeSession>;
using MediaInformationSessionCallbackAdapter =
    SessionCallbackAdapter<internal::MediaInformationSession>;

internal::LogCallback makeLogCallback(ffk_log_cb callback, void *userData,
                                      ffk_free_cb freeUserData) {
    if (callback == nullptr) {
        // Releasing an unused cookie right away keeps the contract simple
        adoptUserData(userData, freeUserData);
        return internal::LogCallback();
    }
    LogCallbackAdapter adapter;
    adapter.callback = callback;
    adapter.userData = userData;
    adapter.userDataOwner = adoptUserData(userData, freeUserData);
    return internal::LogCallback(adapter);
}

internal::StatisticsCallback makeStatisticsCallback(ffk_statistics_cb callback,
                                                    void *userData,
                                                    ffk_free_cb freeUserData) {
    if (callback == nullptr) {
        adoptUserData(userData, freeUserData);
        return internal::StatisticsCallback();
    }
    StatisticsCallbackAdapter adapter;
    adapter.callback = callback;
    adapter.userData = userData;
    adapter.userDataOwner = adoptUserData(userData, freeUserData);
    return internal::StatisticsCallback(adapter);
}

template <typename SessionType, typename CallbackType>
CallbackType makeSessionCallback(ffk_session_cb callback, void *userData,
                                 ffk_free_cb freeUserData) {
    if (callback == nullptr) {
        adoptUserData(userData, freeUserData);
        return CallbackType();
    }
    SessionCallbackAdapter<SessionType> adapter;
    adapter.callback = callback;
    adapter.userData = userData;
    adapter.userDataOwner = adoptUserData(userData, freeUserData);
    return CallbackType(adapter);
}

/**
 * Reads the C function pointer and cookie back out of a std::function, when
 * that function wraps one of the adapters above.
 */
template <typename AdapterType, typename FunctionType, typename CallbackType>
int readCallback(const FunctionType &function, CallbackType *callback,
                 void **userData) {
    if (!function) {
        return 0;
    }
    const AdapterType *adapter = function.template target<AdapterType>();
    if (adapter == nullptr) {
        return 0;
    }
    if (callback != nullptr) {
        *callback = adapter->callback;
    }
    if (userData != nullptr) {
        *userData = adapter->userData;
    }
    return 1;
}

/* ------------------------------------------------------------------------ */
/* Session delete listeners                                                  */
/* ------------------------------------------------------------------------ */

/*
 * SessionDeleteListener is an abstract C++ class, so the C API registers a
 * function pointer instead and hands back an opaque token. The adapters are
 * kept here so that a token can be resolved back to the listener that has to
 * be unregistered.
 */
class SessionDeleteListenerAdapter : public internal::SessionDeleteListener {
  public:
    SessionDeleteListenerAdapter(ffk_session_delete_cb callback, void *userData,
                                 ffk_free_cb freeUserData)
        : _callback{callback}, _userData{userData},
          _userDataOwner{adoptUserData(userData, freeUserData)} {}

    void sessionDeleted(const long sessionId) override {
        try {
            _callback(sessionId, _userData);
        } catch (...) {
        }
    }

  private:
    ffk_session_delete_cb _callback;
    void *_userData;
    std::shared_ptr<void> _userDataOwner;
};

std::mutex &sessionDeleteListenerMutex() {
    static std::mutex mutex;
    return mutex;
}

std::map<long, std::shared_ptr<SessionDeleteListenerAdapter>> &
sessionDeleteListeners() {
    static std::map<long, std::shared_ptr<SessionDeleteListenerAdapter>> listeners;
    return listeners;
}

long nextSessionDeleteListenerToken() {
    static long token = 0;
    return ++token;
}

} // namespace

/* ------------------------------------------------------------------------ */
/* Errors and memory                                                         */
/* ------------------------------------------------------------------------ */

int ffk_has_error(void) { return threadErrorSet ? 1 : 0; }

char *ffk_take_error(void) {
    if (!threadErrorSet) {
        return nullptr;
    }
    threadErrorSet = false;
    // A failure to copy the message must not itself raise
    char *message = static_cast<char *>(std::malloc(threadError.size() + 1));
    if (message == nullptr) {
        return nullptr;
    }
    std::memcpy(message, threadError.c_str(), threadError.size() + 1);
    return message;
}

void ffk_clear_error(void) { threadErrorSet = false; }

void ffk_string_free(char *value) { std::free(value); }

void ffk_bytes_free(uint8_t *data) { std::free(data); }

/* ------------------------------------------------------------------------ */
/* Lists                                                                     */
/* ------------------------------------------------------------------------ */

#define FFK_DEFINE_VALUE_LIST(prefix, ListType)                                \
    size_t prefix##_size(const ListType *list) {                               \
        return list == nullptr ? 0 : list->items.size();                       \
    }                                                                          \
    void prefix##_free(ListType *list) { delete list; }

#define FFK_DEFINE_HANDLE_LIST(prefix, ListType, HandleType)                   \
    FFK_DEFINE_VALUE_LIST(prefix, ListType)                                    \
    HandleType *prefix##_get(const ListType *list, const size_t index) {       \
        return guard([&]() -> HandleType * {                                   \
            if (list == nullptr || index >= list->items.size()) {              \
                return nullptr;                                                \
            }                                                                  \
            return makeHandle<HandleType>(list->items[index]);                 \
        });                                                                    \
    }

FFK_DEFINE_VALUE_LIST(ffk_string_list, FFKStringList)
FFK_DEFINE_HANDLE_LIST(ffk_session_list, FFKSessionList, FFKSession)
FFK_DEFINE_HANDLE_LIST(ffk_log_list, FFKLogList, FFKLog)
FFK_DEFINE_HANDLE_LIST(ffk_statistics_list, FFKStatisticsList, FFKStatistics)
FFK_DEFINE_HANDLE_LIST(ffk_stream_information_list, FFKStreamInformationList,
                       FFKStreamInformation)
FFK_DEFINE_HANDLE_LIST(ffk_chapter_list, FFKChapterList, FFKChapter)

char *ffk_string_list_get(const FFKStringList *list, const size_t index) {
    return guard([&]() -> char * {
        if (list == nullptr || index >= list->items.size()) {
            return nullptr;
        }
        return duplicateString(list->items[index]);
    });
}

/* ------------------------------------------------------------------------ */
/* Log                                                                       */
/* ------------------------------------------------------------------------ */

FFKLog *ffk_log_create(const long sessionId, const int level,
                       const char *message) {
    return guard([&]() -> FFKLog * {
        return makeHandle<FFKLog>(std::make_shared<internal::Log>(
            sessionId, static_cast<internal::Level>(level),
            message == nullptr ? "" : message));
    });
}

void ffk_log_free(FFKLog *log) { delete log; }

long ffk_log_get_session_id(const FFKLog *log) {
    return guard([&]() -> long {
        return log == nullptr ? 0 : log->value->getSessionId();
    });
}

int ffk_log_get_level(const FFKLog *log) {
    return guard([&]() -> int {
        return log == nullptr ? 0 : static_cast<int>(log->value->getLevel());
    });
}

char *ffk_log_get_message(const FFKLog *log) {
    return guard([&]() -> char * {
        return log == nullptr ? nullptr : duplicateString(log->value->getMessage());
    });
}

/* ------------------------------------------------------------------------ */
/* Statistics                                                                */
/* ------------------------------------------------------------------------ */

FFKStatistics *ffk_statistics_create(const long sessionId,
                                     const int videoFrameNumber,
                                     const float videoFps,
                                     const float videoQuality,
                                     const int64_t size, const double time,
                                     const double bitrate, const double speed) {
    return guard([&]() -> FFKStatistics * {
        return makeHandle<FFKStatistics>(std::make_shared<internal::Statistics>(
            sessionId, videoFrameNumber, videoFps, videoQuality, size, time,
            bitrate, speed));
    });
}

void ffk_statistics_free(FFKStatistics *statistics) { delete statistics; }

#define FFK_DEFINE_STATISTICS_GETTER(suffix, ReturnType, method)               \
    ReturnType ffk_statistics_get_##suffix(const FFKStatistics *statistics) {  \
        return guard([&]() -> ReturnType {                                     \
            return statistics == nullptr ? ReturnType()                        \
                                         : statistics->value->method();        \
        });                                                                    \
    }

FFK_DEFINE_STATISTICS_GETTER(session_id, long, getSessionId)
FFK_DEFINE_STATISTICS_GETTER(video_frame_number, int, getVideoFrameNumber)
FFK_DEFINE_STATISTICS_GETTER(video_fps, float, getVideoFps)
FFK_DEFINE_STATISTICS_GETTER(video_quality, float, getVideoQuality)
FFK_DEFINE_STATISTICS_GETTER(size, int64_t, getSize)
FFK_DEFINE_STATISTICS_GETTER(time, double, getTime)
FFK_DEFINE_STATISTICS_GETTER(bitrate, double, getBitrate)
FFK_DEFINE_STATISTICS_GETTER(speed, double, getSpeed)

/* ------------------------------------------------------------------------ */
/* Session                                                                   */
/* ------------------------------------------------------------------------ */

FFKSession *ffk_abstract_session_create(const char *const *arguments,
                                        const size_t argumentCount,
                                        ffk_log_cb logCallback,
                                        void *logUserData, ffk_free_cb logFree,
                                        const int logRedirectionStrategy) {
    return guard([&]() -> FFKSession * {
        const auto log = makeLogCallback(logCallback, logUserData, logFree);
        const auto strategy =
            logRedirectionStrategy < 0
                ? internal::FFmpegKitConfig::getLogRedirectionStrategy()
                : static_cast<internal::LogRedirectionStrategy>(
                      logRedirectionStrategy);
        return makeHandle<FFKSession>(std::make_shared<internal::AbstractSession>(
            toArgumentList(arguments, argumentCount), log, strategy));
    });
}

FFKSession *ffk_ffmpeg_session_create(
    const char *const *arguments, const size_t argumentCount,
    ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree, ffk_statistics_cb statisticsCallback,
    void *statisticsUserData, ffk_free_cb statisticsFree,
    const int logRedirectionStrategy) {
    return guard([&]() -> FFKSession * {
        const auto argumentList = toArgumentList(arguments, argumentCount);
        const auto complete =
            makeSessionCallback<internal::FFmpegSession,
                                internal::FFmpegSessionCompleteCallback>(
                completeCallback, completeUserData, completeFree);
        const auto log = makeLogCallback(logCallback, logUserData, logFree);
        const auto statistics = makeStatisticsCallback(
            statisticsCallback, statisticsUserData, statisticsFree);

        // A negative strategy means "whatever FFmpegKitConfig is set to",
        // which is what the C++ overload without the parameter does
        if (logRedirectionStrategy < 0) {
            return makeHandle<FFKSession>(internal::FFmpegSession::create(
                argumentList, complete, log, statistics));
        }
        return makeHandle<FFKSession>(internal::FFmpegSession::create(
            argumentList, complete, log, statistics,
            static_cast<internal::LogRedirectionStrategy>(
                logRedirectionStrategy)));
    });
}

FFKSession *ffk_ffprobe_session_create(
    const char *const *arguments, const size_t argumentCount,
    ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree, const int logRedirectionStrategy) {
    return guard([&]() -> FFKSession * {
        const auto argumentList = toArgumentList(arguments, argumentCount);
        const auto complete =
            makeSessionCallback<internal::FFprobeSession,
                                internal::FFprobeSessionCompleteCallback>(
                completeCallback, completeUserData, completeFree);
        const auto log = makeLogCallback(logCallback, logUserData, logFree);

        if (logRedirectionStrategy < 0) {
            return makeHandle<FFKSession>(
                internal::FFprobeSession::create(argumentList, complete, log));
        }
        return makeHandle<FFKSession>(internal::FFprobeSession::create(
            argumentList, complete, log,
            static_cast<internal::LogRedirectionStrategy>(
                logRedirectionStrategy)));
    });
}

FFKSession *ffk_media_information_session_create(
    const char *const *arguments, const size_t argumentCount,
    ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree) {
    return guard([&]() -> FFKSession * {
        const auto argumentList = toArgumentList(arguments, argumentCount);
        const auto complete =
            makeSessionCallback<internal::MediaInformationSession,
                                internal::MediaInformationSessionCompleteCallback>(
                completeCallback, completeUserData, completeFree);
        const auto log = makeLogCallback(logCallback, logUserData, logFree);
        return makeHandle<FFKSession>(internal::MediaInformationSession::create(
            argumentList, complete, log));
    });
}

void ffk_session_free(FFKSession *session) { delete session; }

FFKSession *ffk_session_retain(const FFKSession *session) {
    return guard([&]() -> FFKSession * {
        return session == nullptr ? nullptr : makeHandle<FFKSession>(session->value);
    });
}

long ffk_session_get_session_id(const FFKSession *session) {
    return guard([&]() -> long {
        return session == nullptr ? 0 : session->value->getSessionId();
    });
}

int64_t ffk_session_get_create_time(const FFKSession *session) {
    return guard([&]() -> int64_t {
        return session == nullptr
                   ? 0
                   : toEpochMilliseconds(session->value->getCreateTime());
    });
}

int64_t ffk_session_get_start_time(const FFKSession *session) {
    return guard([&]() -> int64_t {
        return session == nullptr
                   ? 0
                   : toEpochMilliseconds(session->value->getStartTime());
    });
}

int64_t ffk_session_get_end_time(const FFKSession *session) {
    return guard([&]() -> int64_t {
        return session == nullptr
                   ? 0
                   : toEpochMilliseconds(session->value->getEndTime());
    });
}

long ffk_session_get_duration(const FFKSession *session) {
    return guard([&]() -> long {
        return session == nullptr ? 0 : session->value->getDuration();
    });
}

FFKStringList *ffk_session_get_arguments(const FFKSession *session) {
    return guard([&]() -> FFKStringList * {
        if (session == nullptr) {
            return nullptr;
        }
        return makeList<FFKStringList>(session->value->getArguments());
    });
}

char *ffk_session_get_command(const FFKSession *session) {
    return guard([&]() -> char * {
        return session == nullptr ? nullptr
                                  : duplicateString(session->value->getCommand());
    });
}

FFKLogList *ffk_session_get_all_logs_with_timeout(const FFKSession *session,
                                                  const int waitTimeout) {
    return guard([&]() -> FFKLogList * {
        if (session == nullptr) {
            return nullptr;
        }
        return makeList<FFKLogList>(
            session->value->getAllLogsWithTimeout(waitTimeout));
    });
}

FFKLogList *ffk_session_get_all_logs(const FFKSession *session) {
    return guard([&]() -> FFKLogList * {
        if (session == nullptr) {
            return nullptr;
        }
        return makeList<FFKLogList>(session->value->getAllLogs());
    });
}

FFKLogList *ffk_session_get_logs(const FFKSession *session) {
    return guard([&]() -> FFKLogList * {
        if (session == nullptr) {
            return nullptr;
        }
        return makeList<FFKLogList>(session->value->getLogs());
    });
}

char *ffk_session_get_all_logs_as_string_with_timeout(const FFKSession *session,
                                                      const int waitTimeout) {
    return guard([&]() -> char * {
        return session == nullptr
                   ? nullptr
                   : duplicateString(
                         session->value->getAllLogsAsStringWithTimeout(
                             waitTimeout));
    });
}

char *ffk_session_get_all_logs_as_string(const FFKSession *session) {
    return guard([&]() -> char * {
        return session == nullptr
                   ? nullptr
                   : duplicateString(session->value->getAllLogsAsString());
    });
}

char *ffk_session_get_logs_as_string(const FFKSession *session) {
    return guard([&]() -> char * {
        return session == nullptr
                   ? nullptr
                   : duplicateString(session->value->getLogsAsString());
    });
}

char *ffk_session_get_output(const FFKSession *session) {
    return guard([&]() -> char * {
        return session == nullptr ? nullptr
                                  : duplicateString(session->value->getOutput());
    });
}

int ffk_session_get_state(const FFKSession *session) {
    return guard([&]() -> int {
        return session == nullptr ? 0
                                  : static_cast<int>(session->value->getState());
    });
}

int ffk_session_get_return_code(const FFKSession *session, int *value) {
    return guard([&]() -> int {
        if (session == nullptr) {
            return 0;
        }
        const auto returnCode = session->value->getReturnCode();
        if (returnCode == nullptr) {
            return 0;
        }
        if (value != nullptr) {
            *value = returnCode->getValue();
        }
        return 1;
    });
}

char *ffk_session_get_fail_stack_trace(const FFKSession *session) {
    return guard([&]() -> char * {
        return session == nullptr
                   ? nullptr
                   : duplicateString(session->value->getFailStackTrace());
    });
}

int ffk_session_get_log_redirection_strategy(const FFKSession *session) {
    return guard([&]() -> int {
        return session == nullptr
                   ? 0
                   : static_cast<int>(session->value->getLogRedirectionStrategy());
    });
}

int ffk_session_there_are_asynchronous_messages_in_transmit(
    const FFKSession *session) {
    return guard([&]() -> int {
        return session != nullptr &&
                       session->value->thereAreAsynchronousMessagesInTransmit()
                   ? 1
                   : 0;
    });
}

void ffk_session_wait_for_asynchronous_messages_in_transmit(
    const FFKSession *session, const int timeout) {
    guard([&]() {
        const auto abstractSession = sessionAs<internal::AbstractSession>(session);
        if (abstractSession != nullptr) {
            abstractSession->waitForAsynchronousMessagesInTransmit(timeout);
        }
    });
}

void ffk_session_add_log(FFKSession *session, const long logSessionId,
                         const int level, const char *message) {
    guard([&]() {
        if (session == nullptr) {
            return;
        }
        session->value->addLog(std::make_shared<internal::Log>(
            logSessionId, static_cast<internal::Level>(level),
            message == nullptr ? "" : message));
    });
}

void ffk_session_start_running(FFKSession *session) {
    guard([&]() {
        if (session != nullptr) {
            session->value->startRunning();
        }
    });
}

void ffk_session_complete(FFKSession *session, const int returnCode) {
    guard([&]() {
        if (session != nullptr) {
            session->value->complete(
                std::make_shared<internal::ReturnCode>(returnCode));
        }
    });
}

void ffk_session_fail(FFKSession *session, const char *error) {
    guard([&]() {
        if (session != nullptr) {
            session->value->fail(error == nullptr ? "" : error);
        }
    });
}

int ffk_session_is_ffmpeg(const FFKSession *session) {
    return guard([&]() -> int {
        return session != nullptr && session->value->isFFmpeg() ? 1 : 0;
    });
}

int ffk_session_is_ffprobe(const FFKSession *session) {
    return guard([&]() -> int {
        return session != nullptr && session->value->isFFprobe() ? 1 : 0;
    });
}

int ffk_session_is_media_information(const FFKSession *session) {
    return guard([&]() -> int {
        return session != nullptr && session->value->isMediaInformation() ? 1 : 0;
    });
}

void ffk_session_cancel(FFKSession *session) {
    guard([&]() {
        if (session != nullptr) {
            session->value->cancel();
        }
    });
}

int ffk_session_get_log_callback(const FFKSession *session,
                                 ffk_log_cb *callback, void **userData) {
    return guard([&]() -> int {
        if (session == nullptr) {
            return 0;
        }
        return readCallback<LogCallbackAdapter>(session->value->getLogCallback(),
                                                callback, userData);
    });
}

int ffk_session_get_complete_callback(const FFKSession *session,
                                      ffk_session_cb *callback,
                                      void **userData) {
    return guard([&]() -> int {
        if (session == nullptr) {
            return 0;
        }
        if (const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session)) {
            return readCallback<FFmpegSessionCallbackAdapter>(
                ffmpegSession->getCompleteCallback(), callback, userData);
        }
        if (const auto ffprobeSession = sessionAs<internal::FFprobeSession>(session)) {
            return readCallback<FFprobeSessionCallbackAdapter>(
                ffprobeSession->getCompleteCallback(), callback, userData);
        }
        if (const auto mediaInformationSession =
                sessionAs<internal::MediaInformationSession>(session)) {
            return readCallback<MediaInformationSessionCallbackAdapter>(
                mediaInformationSession->getCompleteCallback(), callback,
                userData);
        }
        return 0;
    });
}

/* ---- FFmpeg session specific -------------------------------------------- */

FFKStatisticsList *
ffk_ffmpeg_session_get_all_statistics_with_timeout(FFKSession *session,
                                                   const int waitTimeout) {
    return guard([&]() -> FFKStatisticsList * {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr) {
            return nullptr;
        }
        return makeList<FFKStatisticsList>(
            ffmpegSession->getAllStatisticsWithTimeout(waitTimeout));
    });
}

FFKStatisticsList *ffk_ffmpeg_session_get_all_statistics(FFKSession *session) {
    return guard([&]() -> FFKStatisticsList * {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr) {
            return nullptr;
        }
        return makeList<FFKStatisticsList>(ffmpegSession->getAllStatistics());
    });
}

FFKStatisticsList *ffk_ffmpeg_session_get_statistics(FFKSession *session) {
    return guard([&]() -> FFKStatisticsList * {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr) {
            return nullptr;
        }
        return makeList<FFKStatisticsList>(ffmpegSession->getStatistics());
    });
}

FFKStatistics *
ffk_ffmpeg_session_get_last_received_statistics(FFKSession *session) {
    return guard([&]() -> FFKStatistics * {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr) {
            return nullptr;
        }
        return makeHandle<FFKStatistics>(
            ffmpegSession->getLastReceivedStatistics());
    });
}

void ffk_ffmpeg_session_add_statistics(FFKSession *session,
                                       const FFKStatistics *statistics) {
    guard([&]() {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr || statistics == nullptr) {
            return;
        }
        ffmpegSession->addStatistics(statistics->value);
    });
}

int ffk_ffmpeg_session_get_statistics_callback(const FFKSession *session,
                                               ffk_statistics_cb *callback,
                                               void **userData) {
    return guard([&]() -> int {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession == nullptr) {
            return 0;
        }
        return readCallback<StatisticsCallbackAdapter>(
            ffmpegSession->getStatisticsCallback(), callback, userData);
    });
}

/* ---- media information session specific --------------------------------- */

FFKMediaInformation *
ffk_media_information_session_get_media_information(FFKSession *session) {
    return guard([&]() -> FFKMediaInformation * {
        const auto mediaInformationSession =
            sessionAs<internal::MediaInformationSession>(session);
        if (mediaInformationSession == nullptr) {
            return nullptr;
        }
        return makeHandle<FFKMediaInformation>(
            mediaInformationSession->getMediaInformation());
    });
}

void ffk_media_information_session_set_media_information(
    FFKSession *session, FFKMediaInformation *mediaInformation) {
    guard([&]() {
        const auto mediaInformationSession =
            sessionAs<internal::MediaInformationSession>(session);
        if (mediaInformationSession == nullptr) {
            return;
        }
        mediaInformationSession->setMediaInformation(
            mediaInformation == nullptr
                ? std::shared_ptr<internal::MediaInformation>()
                : mediaInformation->value);
    });
}

/* ------------------------------------------------------------------------ */
/* Chapter, stream information and media information properties              */
/* ------------------------------------------------------------------------ */

/*
 * The three metadata classes expose the same four generic property accessors.
 * The convenience getters on top of them (getFilename, getCodec, getStart and
 * so on) are pure key lookups and stay in the header-only facade.
 */
#define FFK_DEFINE_PROPERTY_ACCESSORS(prefix, HandleType)                      \
    void prefix##_free(HandleType *handle) { delete handle; }                  \
                                                                               \
    int prefix##_get_number_property(HandleType *handle, const char *key,      \
                                     int64_t *value) {                         \
        return guard([&]() -> int {                                            \
            if (handle == nullptr || key == nullptr) {                         \
                return 0;                                                      \
            }                                                                  \
            const auto property = handle->value->getNumberProperty(key);       \
            if (property == nullptr) {                                         \
                return 0;                                                      \
            }                                                                  \
            if (value != nullptr) {                                            \
                *value = *property;                                            \
            }                                                                  \
            return 1;                                                          \
        });                                                                    \
    }                                                                          \
                                                                               \
    char *prefix##_get_string_property(HandleType *handle, const char *key) {  \
        return guard([&]() -> char * {                                         \
            if (handle == nullptr || key == nullptr) {                         \
                return nullptr;                                                \
            }                                                                  \
            return duplicateString(handle->value->getStringProperty(key));     \
        });                                                                    \
    }                                                                          \
                                                                               \
    char *prefix##_get_property_json(HandleType *handle, const char *key) {    \
        return guard([&]() -> char * {                                         \
            if (handle == nullptr || key == nullptr) {                         \
                return nullptr;                                                \
            }                                                                  \
            return toJson(handle->value->getProperty(key));                    \
        });                                                                    \
    }                                                                          \
                                                                               \
    char *prefix##_get_all_properties_json(HandleType *handle) {               \
        return guard([&]() -> char * {                                         \
            if (handle == nullptr) {                                           \
                return nullptr;                                                \
            }                                                                  \
            return toJson(handle->value->getAllProperties());                  \
        });                                                                    \
    }

namespace {

/** Parses JSON text into a value, treating unparsable text as absent. */
std::shared_ptr<ffmpegkit::json::Value> parseJson(const char *json) {
    if (json == nullptr) {
        return nullptr;
    }
    auto value = std::make_shared<ffmpegkit::json::Value>();
    if (!ffmpegkit::json::detail::parse(json, *value)) {
        return nullptr;
    }
    if (value->isNull()) {
        return nullptr;
    }
    return value;
}

} // namespace

FFKChapter *ffk_chapter_create(const char *valueJson) {
    return guard([&]() -> FFKChapter * {
        return makeHandle<FFKChapter>(
            std::make_shared<internal::Chapter>(parseJson(valueJson)));
    });
}

FFKStreamInformation *ffk_stream_information_create(const char *valueJson) {
    return guard([&]() -> FFKStreamInformation * {
        return makeHandle<FFKStreamInformation>(
            std::make_shared<internal::StreamInformation>(parseJson(valueJson)));
    });
}

FFKMediaInformation *ffk_media_information_create(
    const char *valueJson, const char *const *streamJson,
    const size_t streamCount, const char *const *chapterJson,
    const size_t chapterCount) {
    return guard([&]() -> FFKMediaInformation * {
        auto streams = std::make_shared<
            std::vector<std::shared_ptr<internal::StreamInformation>>>();
        for (size_t index = 0; index < streamCount; index++) {
            streams->push_back(std::make_shared<internal::StreamInformation>(
                parseJson(streamJson == nullptr ? nullptr : streamJson[index])));
        }
        auto chapters =
            std::make_shared<std::vector<std::shared_ptr<internal::Chapter>>>();
        for (size_t index = 0; index < chapterCount; index++) {
            chapters->push_back(std::make_shared<internal::Chapter>(parseJson(
                chapterJson == nullptr ? nullptr : chapterJson[index])));
        }
        return makeHandle<FFKMediaInformation>(
            std::make_shared<internal::MediaInformation>(parseJson(valueJson),
                                                         streams, chapters));
    });
}

FFK_DEFINE_PROPERTY_ACCESSORS(ffk_chapter, FFKChapter)
FFK_DEFINE_PROPERTY_ACCESSORS(ffk_stream_information, FFKStreamInformation)
FFK_DEFINE_PROPERTY_ACCESSORS(ffk_media_information, FFKMediaInformation)

FFKStreamInformationList *
ffk_media_information_get_streams(FFKMediaInformation *mediaInformation) {
    return guard([&]() -> FFKStreamInformationList * {
        if (mediaInformation == nullptr) {
            return nullptr;
        }
        return makeList<FFKStreamInformationList>(
            mediaInformation->value->getStreams());
    });
}

FFKChapterList *
ffk_media_information_get_chapters(FFKMediaInformation *mediaInformation) {
    return guard([&]() -> FFKChapterList * {
        if (mediaInformation == nullptr) {
            return nullptr;
        }
        return makeList<FFKChapterList>(mediaInformation->value->getChapters());
    });
}

int ffk_media_information_get_number_format_property(
    FFKMediaInformation *mediaInformation, const char *key, int64_t *value) {
    return guard([&]() -> int {
        if (mediaInformation == nullptr || key == nullptr) {
            return 0;
        }
        const auto property =
            mediaInformation->value->getNumberFormatProperty(key);
        if (property == nullptr) {
            return 0;
        }
        if (value != nullptr) {
            *value = *property;
        }
        return 1;
    });
}

char *ffk_media_information_get_string_format_property(
    FFKMediaInformation *mediaInformation, const char *key) {
    return guard([&]() -> char * {
        if (mediaInformation == nullptr || key == nullptr) {
            return nullptr;
        }
        return duplicateString(
            mediaInformation->value->getStringFormatProperty(key));
    });
}

char *ffk_media_information_get_format_property_json(
    FFKMediaInformation *mediaInformation, const char *key) {
    return guard([&]() -> char * {
        if (mediaInformation == nullptr || key == nullptr) {
            return nullptr;
        }
        return toJson(mediaInformation->value->getFormatProperty(key));
    });
}

char *ffk_media_information_get_format_properties_json(
    FFKMediaInformation *mediaInformation) {
    return guard([&]() -> char * {
        if (mediaInformation == nullptr) {
            return nullptr;
        }
        return toJson(mediaInformation->value->getFormatProperties());
    });
}

FFKMediaInformation *
ffk_media_information_parser_from(const char *ffprobeJsonOutput) {
    return guard([&]() -> FFKMediaInformation * {
        return makeHandle<FFKMediaInformation>(
            internal::MediaInformationJsonParser::from(
                ffprobeJsonOutput == nullptr ? "" : ffprobeJsonOutput));
    });
}

FFKMediaInformation *
ffk_media_information_parser_from_with_error(const char *ffprobeJsonOutput) {
    return guard([&]() -> FFKMediaInformation * {
        return makeHandle<FFKMediaInformation>(
            internal::MediaInformationJsonParser::fromWithError(
                ffprobeJsonOutput == nullptr ? "" : ffprobeJsonOutput));
    });
}

/* ------------------------------------------------------------------------ */
/* FFmpegKit                                                                 */
/* ------------------------------------------------------------------------ */

FFKSession *ffk_ffmpegkit_execute_with_arguments(const char *const *arguments,
                                                 const size_t argumentCount) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFmpegKit::executeWithArguments(
            toArgumentList(arguments, argumentCount)));
    });
}

FFKSession *ffk_ffmpegkit_execute_with_arguments_async(
    const char *const *arguments, const size_t argumentCount,
    ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree, ffk_statistics_cb statisticsCallback,
    void *statisticsUserData, ffk_free_cb statisticsFree) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFmpegKit::executeWithArgumentsAsync(
                toArgumentList(arguments, argumentCount),
                makeSessionCallback<internal::FFmpegSession,
                                    internal::FFmpegSessionCompleteCallback>(
                    completeCallback, completeUserData, completeFree),
                makeLogCallback(logCallback, logUserData, logFree),
                makeStatisticsCallback(statisticsCallback, statisticsUserData,
                                       statisticsFree)));
    });
}

FFKSession *ffk_ffmpegkit_execute(const char *command) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFmpegKit::execute(command == nullptr ? "" : command));
    });
}

FFKSession *ffk_ffmpegkit_execute_async(
    const char *command, ffk_session_cb completeCallback,
    void *completeUserData, ffk_free_cb completeFree, ffk_log_cb logCallback,
    void *logUserData, ffk_free_cb logFree,
    ffk_statistics_cb statisticsCallback, void *statisticsUserData,
    ffk_free_cb statisticsFree) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFmpegKit::executeAsync(
            command == nullptr ? "" : command,
            makeSessionCallback<internal::FFmpegSession,
                                internal::FFmpegSessionCompleteCallback>(
                completeCallback, completeUserData, completeFree),
            makeLogCallback(logCallback, logUserData, logFree),
            makeStatisticsCallback(statisticsCallback, statisticsUserData,
                                   statisticsFree)));
    });
}

void ffk_ffmpegkit_cancel(void) { guard([&]() { internal::FFmpegKit::cancel(); }); }

void ffk_ffmpegkit_cancel_session(const long sessionId) {
    guard([&]() { internal::FFmpegKit::cancel(sessionId); });
}

FFKSessionList *ffk_ffmpegkit_list_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(internal::FFmpegKit::listSessions());
    });
}

/* ------------------------------------------------------------------------ */
/* FFprobeKit                                                                */
/* ------------------------------------------------------------------------ */

FFKSession *ffk_ffprobekit_execute_with_arguments(const char *const *arguments,
                                                  const size_t argumentCount) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFprobeKit::executeWithArguments(
            toArgumentList(arguments, argumentCount)));
    });
}

FFKSession *ffk_ffprobekit_execute_with_arguments_async(
    const char *const *arguments, const size_t argumentCount,
    ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFprobeKit::executeWithArgumentsAsync(
                toArgumentList(arguments, argumentCount),
                makeSessionCallback<internal::FFprobeSession,
                                    internal::FFprobeSessionCompleteCallback>(
                    completeCallback, completeUserData, completeFree),
                makeLogCallback(logCallback, logUserData, logFree)));
    });
}

FFKSession *ffk_ffprobekit_execute(const char *command) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFprobeKit::execute(command == nullptr ? "" : command));
    });
}

FFKSession *ffk_ffprobekit_execute_async(const char *command,
                                         ffk_session_cb completeCallback,
                                         void *completeUserData,
                                         ffk_free_cb completeFree,
                                         ffk_log_cb logCallback,
                                         void *logUserData,
                                         ffk_free_cb logFree) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFprobeKit::executeAsync(
            command == nullptr ? "" : command,
            makeSessionCallback<internal::FFprobeSession,
                                internal::FFprobeSessionCompleteCallback>(
                completeCallback, completeUserData, completeFree),
            makeLogCallback(logCallback, logUserData, logFree)));
    });
}

FFKSession *ffk_ffprobekit_get_media_information(const char *path,
                                                 const int waitTimeout) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFprobeKit::getMediaInformation(
            path == nullptr ? "" : path, waitTimeout));
    });
}

FFKSession *ffk_ffprobekit_get_media_information_async(
    const char *path, ffk_session_cb completeCallback, void *completeUserData,
    ffk_free_cb completeFree, ffk_log_cb logCallback, void *logUserData,
    ffk_free_cb logFree, const int waitTimeout) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFprobeKit::getMediaInformationAsync(
                path == nullptr ? "" : path,
                makeSessionCallback<
                    internal::MediaInformationSession,
                    internal::MediaInformationSessionCompleteCallback>(
                    completeCallback, completeUserData, completeFree),
                makeLogCallback(logCallback, logUserData, logFree),
                waitTimeout));
    });
}

FFKSession *
ffk_ffprobekit_get_media_information_from_command(const char *command) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFprobeKit::getMediaInformationFromCommand(
                command == nullptr ? "" : command));
    });
}

FFKSessionList *ffk_ffprobekit_list_ffprobe_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFprobeKit::listFFprobeSessions());
    });
}

FFKSessionList *ffk_ffprobekit_list_media_information_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFprobeKit::listMediaInformationSessions());
    });
}

/* ------------------------------------------------------------------------ */
/* FFmpegKitConfig                                                           */
/* ------------------------------------------------------------------------ */

void ffk_config_enable_redirection(void) {
    guard([&]() { internal::FFmpegKitConfig::enableRedirection(); });
}

void ffk_config_disable_redirection(void) {
    guard([&]() { internal::FFmpegKitConfig::disableRedirection(); });
}

int ffk_config_set_fontconfig_configuration_path(const char *path) {
    return guard([&]() -> int {
        return internal::FFmpegKitConfig::setFontconfigConfigurationPath(
            path == nullptr ? "" : path);
    });
}

void ffk_config_set_font_directory(const char *fontDirectoryPath,
                                   const char *const *mappingKeys,
                                   const char *const *mappingValues,
                                   const size_t mappingCount) {
    guard([&]() {
        internal::FFmpegKitConfig::setFontDirectory(
            fontDirectoryPath == nullptr ? "" : fontDirectoryPath,
            toStringMap(mappingKeys, mappingValues, mappingCount));
    });
}

void ffk_config_set_font_directory_list(const char *const *fontDirectories,
                                        const size_t fontDirectoryCount,
                                        const char *const *mappingKeys,
                                        const char *const *mappingValues,
                                        const size_t mappingCount) {
    guard([&]() {
        internal::FFmpegKitConfig::setFontDirectoryList(
            toArgumentList(fontDirectories, fontDirectoryCount),
            toStringMap(mappingKeys, mappingValues, mappingCount));
    });
}

char *ffk_config_register_new_ffmpeg_pipe(void) {
    return guard([&]() -> char * {
        // Named pipes are not supported on Windows: the C++ method is a
        // deprecated no-op that returns nullptr, and so is this one
        return duplicateString(
            internal::FFmpegKitConfig::registerNewFFmpegPipe());
    });
}

void ffk_config_close_ffmpeg_pipe(const char *ffmpegPipePath) {
    guard([&]() {
        internal::FFmpegKitConfig::closeFFmpegPipe(
            ffmpegPipePath == nullptr ? "" : ffmpegPipePath);
    });
}

long ffk_config_register_ffmpegkit_input_buffer(const uint8_t *data,
                                                const size_t size) {
    return guard([&]() -> long {
        return internal::FFmpegKitConfig::registerFFmpegKitInputBuffer(data, size);
    });
}

long ffk_config_register_ffmpegkit_output_buffer(const long initialCapacity,
                                                 const long maxCapacity) {
    return guard([&]() -> long {
        return internal::FFmpegKitConfig::registerFFmpegKitOutputBuffer(
            initialCapacity, maxCapacity);
    });
}

long ffk_config_get_ffmpegkit_buffer_size(const long bufferId) {
    return guard([&]() -> long {
        return internal::FFmpegKitConfig::getFFmpegKitBufferSize(bufferId);
    });
}

int ffk_config_get_ffmpegkit_output_buffer(const long bufferId, uint8_t **data,
                                           size_t *size) {
    return guard([&]() -> int {
        const auto buffer =
            internal::FFmpegKitConfig::getFFmpegKitOutputBuffer(bufferId);
        if (buffer == nullptr) {
            return 0;
        }
        if (data != nullptr) {
            *data = duplicateBytes(*buffer);
        }
        if (size != nullptr) {
            *size = buffer->size();
        }
        return 1;
    });
}

void ffk_config_unregister_ffmpegkit_buffer(const long bufferId) {
    guard([&]() { internal::FFmpegKitConfig::unregisterFFmpegKitBuffer(bufferId); });
}

long ffk_config_register_ffmpegkit_stream(const long capacity, const int type) {
    return guard([&]() -> long {
        return internal::FFmpegKitConfig::registerFFmpegKitStream(capacity, type);
    });
}

int ffk_config_write_ffmpegkit_stream(const long streamId, const uint8_t *data,
                                      const size_t length,
                                      const int timeoutMs) {
    return guard([&]() -> int {
        return internal::FFmpegKitConfig::writeFFmpegKitStream(streamId, data,
                                                               length, timeoutMs);
    });
}

int ffk_config_read_ffmpegkit_stream(const long streamId, const int maxBytes,
                                     const int timeoutMs, uint8_t **data,
                                     size_t *size) {
    return guard([&]() -> int {
        const auto buffer = internal::FFmpegKitConfig::readFFmpegKitStream(
            streamId, maxBytes, timeoutMs);
        if (buffer == nullptr) {
            return 0;
        }
        if (data != nullptr) {
            *data = duplicateBytes(*buffer);
        }
        if (size != nullptr) {
            *size = buffer->size();
        }
        return 1;
    });
}

void ffk_config_close_ffmpegkit_stream_input(const long streamId) {
    guard([&]() {
        internal::FFmpegKitConfig::closeFFmpegKitStreamInput(streamId);
    });
}

void ffk_config_unregister_ffmpegkit_stream(const long streamId) {
    guard([&]() { internal::FFmpegKitConfig::unregisterFFmpegKitStream(streamId); });
}

char *ffk_protocol_build_url(const char *protocol, const long id,
                             const char *extension) {
    return guard([&]() -> char * {
        return duplicateString(internal::buildFFmpegKitUrl(
            protocol == nullptr ? "" : protocol, id,
            extension == nullptr ? "" : extension));
    });
}

char *ffk_config_get_ffmpeg_version(void) {
    return guard([&]() -> char * {
        return duplicateString(internal::FFmpegKitConfig::getFFmpegVersion());
    });
}

char *ffk_config_get_version(void) {
    return guard([&]() -> char * {
        return duplicateString(internal::FFmpegKitConfig::getVersion());
    });
}

int ffk_config_is_lts_build(void) {
    return guard([&]() -> int {
        return internal::FFmpegKitConfig::isLTSBuild() ? 1 : 0;
    });
}

char *ffk_config_get_build_date(void) {
    return guard([&]() -> char * {
        return duplicateString(internal::FFmpegKitConfig::getBuildDate());
    });
}

int ffk_config_set_environment_variable(const char *variableName,
                                        const char *variableValue) {
    return guard([&]() -> int {
        return internal::FFmpegKitConfig::setEnvironmentVariable(
            variableName == nullptr ? "" : variableName,
            variableValue == nullptr ? "" : variableValue);
    });
}

void ffk_config_ignore_signal(const int signal) {
    guard([&]() {
        internal::FFmpegKitConfig::ignoreSignal(
            static_cast<internal::Signal>(signal));
    });
}

void ffk_config_ffmpeg_execute(FFKSession *session) {
    guard([&]() {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession != nullptr) {
            internal::FFmpegKitConfig::ffmpegExecute(ffmpegSession);
        }
    });
}

void ffk_config_ffprobe_execute(FFKSession *session) {
    guard([&]() {
        const auto ffprobeSession = sessionAs<internal::FFprobeSession>(session);
        if (ffprobeSession != nullptr) {
            internal::FFmpegKitConfig::ffprobeExecute(ffprobeSession);
        }
    });
}

void ffk_config_get_media_information_execute(FFKSession *session,
                                              const int waitTimeout) {
    guard([&]() {
        const auto mediaInformationSession =
            sessionAs<internal::MediaInformationSession>(session);
        if (mediaInformationSession != nullptr) {
            internal::FFmpegKitConfig::getMediaInformationExecute(
                mediaInformationSession, waitTimeout);
        }
    });
}

void ffk_config_async_ffmpeg_execute(FFKSession *session) {
    guard([&]() {
        const auto ffmpegSession = sessionAs<internal::FFmpegSession>(session);
        if (ffmpegSession != nullptr) {
            internal::FFmpegKitConfig::asyncFFmpegExecute(ffmpegSession);
        }
    });
}

void ffk_config_async_ffprobe_execute(FFKSession *session) {
    guard([&]() {
        const auto ffprobeSession = sessionAs<internal::FFprobeSession>(session);
        if (ffprobeSession != nullptr) {
            internal::FFmpegKitConfig::asyncFFprobeExecute(ffprobeSession);
        }
    });
}

void ffk_config_async_get_media_information_execute(FFKSession *session,
                                                    const int waitTimeout) {
    guard([&]() {
        const auto mediaInformationSession =
            sessionAs<internal::MediaInformationSession>(session);
        if (mediaInformationSession != nullptr) {
            internal::FFmpegKitConfig::asyncGetMediaInformationExecute(
                mediaInformationSession, waitTimeout);
        }
    });
}

void ffk_config_enable_log_callback(ffk_log_cb callback, void *userData,
                                    ffk_free_cb freeUserData) {
    guard([&]() {
        internal::FFmpegKitConfig::enableLogCallback(
            makeLogCallback(callback, userData, freeUserData));
    });
}

void ffk_config_enable_statistics_callback(ffk_statistics_cb callback,
                                           void *userData,
                                           ffk_free_cb freeUserData) {
    guard([&]() {
        internal::FFmpegKitConfig::enableStatisticsCallback(
            makeStatisticsCallback(callback, userData, freeUserData));
    });
}

void ffk_config_enable_ffmpeg_session_complete_callback(
    ffk_session_cb callback, void *userData, ffk_free_cb freeUserData) {
    guard([&]() {
        internal::FFmpegKitConfig::enableFFmpegSessionCompleteCallback(
            makeSessionCallback<internal::FFmpegSession,
                                internal::FFmpegSessionCompleteCallback>(
                callback, userData, freeUserData));
    });
}

int ffk_config_get_ffmpeg_session_complete_callback(ffk_session_cb *callback,
                                                    void **userData) {
    return guard([&]() -> int {
        return readCallback<FFmpegSessionCallbackAdapter>(
            internal::FFmpegKitConfig::getFFmpegSessionCompleteCallback(),
            callback, userData);
    });
}

void ffk_config_enable_ffprobe_session_complete_callback(
    ffk_session_cb callback, void *userData, ffk_free_cb freeUserData) {
    guard([&]() {
        internal::FFmpegKitConfig::enableFFprobeSessionCompleteCallback(
            makeSessionCallback<internal::FFprobeSession,
                                internal::FFprobeSessionCompleteCallback>(
                callback, userData, freeUserData));
    });
}

int ffk_config_get_ffprobe_session_complete_callback(ffk_session_cb *callback,
                                                     void **userData) {
    return guard([&]() -> int {
        return readCallback<FFprobeSessionCallbackAdapter>(
            internal::FFmpegKitConfig::getFFprobeSessionCompleteCallback(),
            callback, userData);
    });
}

void ffk_config_enable_media_information_session_complete_callback(
    ffk_session_cb callback, void *userData, ffk_free_cb freeUserData) {
    guard([&]() {
        internal::FFmpegKitConfig::enableMediaInformationSessionCompleteCallback(
            makeSessionCallback<
                internal::MediaInformationSession,
                internal::MediaInformationSessionCompleteCallback>(
                callback, userData, freeUserData));
    });
}

int ffk_config_get_media_information_session_complete_callback(
    ffk_session_cb *callback, void **userData) {
    return guard([&]() -> int {
        return readCallback<MediaInformationSessionCallbackAdapter>(
            internal::FFmpegKitConfig::
                getMediaInformationSessionCompleteCallback(),
            callback, userData);
    });
}

int ffk_config_get_log_level(void) {
    return guard([&]() -> int {
        return static_cast<int>(internal::FFmpegKitConfig::getLogLevel());
    });
}

void ffk_config_set_log_level(const int level) {
    guard([&]() {
        internal::FFmpegKitConfig::setLogLevel(
            static_cast<internal::Level>(level));
    });
}

char *ffk_config_log_level_to_string(const int level) {
    return guard([&]() -> char * {
        return duplicateString(internal::FFmpegKitConfig::logLevelToString(
            static_cast<internal::Level>(level)));
    });
}

int ffk_config_get_session_history_size(void) {
    return guard(
        [&]() -> int { return internal::FFmpegKitConfig::getSessionHistorySize(); });
}

void ffk_config_set_session_history_size(const int sessionHistorySize) {
    guard([&]() {
        internal::FFmpegKitConfig::setSessionHistorySize(sessionHistorySize);
    });
}

FFKSession *ffk_config_get_session(const long sessionId) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFmpegKitConfig::getSession(sessionId));
    });
}

void ffk_config_delete_session(const long sessionId) {
    guard([&]() { internal::FFmpegKitConfig::deleteSession(sessionId); });
}

long ffk_config_add_session_delete_listener(ffk_session_delete_cb callback,
                                            void *userData,
                                            ffk_free_cb freeUserData) {
    return guard([&]() -> long {
        if (callback == nullptr) {
            adoptUserData(userData, freeUserData);
            return 0;
        }
        auto listener = std::make_shared<SessionDeleteListenerAdapter>(
            callback, userData, freeUserData);
        internal::FFmpegKitConfig::addSessionDeleteListener(listener);

        std::lock_guard<std::mutex> lock(sessionDeleteListenerMutex());
        const long token = nextSessionDeleteListenerToken();
        sessionDeleteListeners()[token] = listener;
        return token;
    });
}

void ffk_config_remove_session_delete_listener(const long token) {
    guard([&]() {
        std::shared_ptr<SessionDeleteListenerAdapter> listener;
        {
            std::lock_guard<std::mutex> lock(sessionDeleteListenerMutex());
            auto entry = sessionDeleteListeners().find(token);
            if (entry == sessionDeleteListeners().end()) {
                return;
            }
            listener = entry->second;
            sessionDeleteListeners().erase(entry);
        }
        internal::FFmpegKitConfig::removeSessionDeleteListener(listener);
    });
}

FFKSession *ffk_config_get_last_session(void) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(internal::FFmpegKitConfig::getLastSession());
    });
}

FFKSession *ffk_config_get_last_completed_session(void) {
    return guard([&]() -> FFKSession * {
        return makeHandle<FFKSession>(
            internal::FFmpegKitConfig::getLastCompletedSession());
    });
}

FFKSessionList *ffk_config_get_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(internal::FFmpegKitConfig::getSessions());
    });
}

void ffk_config_clear_sessions(void) {
    guard([&]() { internal::FFmpegKitConfig::clearSessions(); });
}

FFKSessionList *ffk_config_get_ffmpeg_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFmpegKitConfig::getFFmpegSessions());
    });
}

FFKSessionList *ffk_config_get_ffprobe_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFmpegKitConfig::getFFprobeSessions());
    });
}

FFKSessionList *ffk_config_get_media_information_sessions(void) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFmpegKitConfig::getMediaInformationSessions());
    });
}

FFKSessionList *ffk_config_get_sessions_by_state(const int state) {
    return guard([&]() -> FFKSessionList * {
        return makeList<FFKSessionList>(
            internal::FFmpegKitConfig::getSessionsByState(
                static_cast<internal::SessionState>(state)));
    });
}

int ffk_config_get_log_redirection_strategy(void) {
    return guard([&]() -> int {
        return static_cast<int>(
            internal::FFmpegKitConfig::getLogRedirectionStrategy());
    });
}

void ffk_config_set_log_redirection_strategy(const int strategy) {
    guard([&]() {
        internal::FFmpegKitConfig::setLogRedirectionStrategy(
            static_cast<internal::LogRedirectionStrategy>(strategy));
    });
}

int ffk_config_messages_in_transmit(const long sessionId) {
    return guard([&]() -> int {
        return internal::FFmpegKitConfig::messagesInTransmit(sessionId);
    });
}

char *ffk_config_session_state_to_string(const int state) {
    return guard([&]() -> char * {
        return duplicateString(internal::FFmpegKitConfig::sessionStateToString(
            static_cast<internal::SessionState>(state)));
    });
}

FFKStringList *ffk_config_parse_arguments(const char *command) {
    return guard([&]() -> FFKStringList * {
        std::unique_ptr<FFKStringList> list(new FFKStringList());
        const auto arguments = internal::FFmpegKitConfig::parseArguments(
            command == nullptr ? "" : command);
        list->items.assign(arguments.begin(), arguments.end());
        return list.release();
    });
}

char *ffk_config_arguments_to_string(const char *const *arguments,
                                     const size_t argumentCount) {
    return guard([&]() -> char * {
        auto list = std::make_shared<std::list<std::string>>(
            toArgumentList(arguments, argumentCount));
        return duplicateString(
            internal::FFmpegKitConfig::argumentsToString(list));
    });
}

/* ------------------------------------------------------------------------ */
/* Packages and architecture                                                 */
/* ------------------------------------------------------------------------ */

char *ffk_packages_get_package_name(void) {
    return guard([&]() -> char * {
        return duplicateString(internal::Packages::getPackageName());
    });
}

FFKStringList *ffk_packages_get_external_libraries(void) {
    return guard([&]() -> FFKStringList * {
        std::unique_ptr<FFKStringList> list(new FFKStringList());
        const auto libraries = internal::Packages::getExternalLibraries();
        if (libraries != nullptr) {
            list->items.assign(libraries->begin(), libraries->end());
        }
        return list.release();
    });
}

char *ffk_arch_detect_get_arch(void) {
    return guard([&]() -> char * {
        return duplicateString(internal::ArchDetect::getArch());
    });
}
