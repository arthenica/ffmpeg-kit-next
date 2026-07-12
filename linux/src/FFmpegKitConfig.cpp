/*
 * Copyright (c) 2022, 2026 Taner Sener
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

#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "fftools/cmdutils.h"
#include "libavformat/avio.h"
#include "libavutil/bprint.h"
#include "libavutil/common.h"
#include "libavutil/error.h"
#include "libavutil/ffversion.h"
#include "libavutil/mem.h"
}
#include "ArchDetect.h"
#include "FFmpegKit.h"
#include "FFmpegKitConfig.h"
#include "FFmpegSession.h"
#include "FFprobeKit.h"
#include "FFprobeSession.h"
#include "Level.h"
#include "LogRedirectionStrategy.h"
#include "MediaInformationSession.h"
#include "Packages.h"
#include "SessionState.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <future>
#include <limits>
#include <iostream>
#include <mutex>

extern "C" {
void set_report_callback(void (*callback)(int, float, float, int64_t, double,
                                          double, double));
void cancel_operation(long id);
}

/**
 * Generates ids for named ffmpeg kit pipes.
 */
static std::atomic<long> pipeIndexGenerator(1);

/* Session history variables */
static int sessionHistorySize;
static std::map<long, std::shared_ptr<ffmpegkit::Session>> sessionHistoryMap;
static std::list<std::shared_ptr<ffmpegkit::Session>> sessionHistoryList;
static std::recursive_mutex sessionMutex;
static std::list<std::weak_ptr<ffmpegkit::SessionDeleteListener>>
    sessionDeleteListeners;
static std::recursive_mutex sessionDeleteListenerMutex;

/** Session control variables */
#define SESSION_MAP_SIZE 1000
static std::atomic<short> sessionMap[SESSION_MAP_SIZE];
static std::atomic<int> sessionInTransitMessageCountMap[SESSION_MAP_SIZE];

/** Holds callback defined to redirect logs */
static ffmpegkit::LogCallback logCallback;

/** Holds callback defined to redirect statistics */
static ffmpegkit::StatisticsCallback statisticsCallback;

/** Holds complete callbacks defined to redirect asynchronous execution results
 */
static ffmpegkit::FFmpegSessionCompleteCallback ffmpegSessionCompleteCallback;
static ffmpegkit::FFprobeSessionCompleteCallback ffprobeSessionCompleteCallback;
static ffmpegkit::MediaInformationSessionCompleteCallback
    mediaInformationSessionCompleteCallback;

static ffmpegkit::LogRedirectionStrategy globalLogRedirectionStrategy;

/** Redirection control variables */
static int redirectionEnabled;
static std::recursive_mutex callbackDataMutex;
static std::mutex callbackMutex;
static std::condition_variable callbackMonitor;
class CallbackData;
static std::list<CallbackData *> callbackDataList;

/** Fields that control the handling of SIGNALs */
volatile int handleSIGQUIT = 1;
volatile int handleSIGINT = 1;
volatile int handleSIGTERM = 1;
volatile int handleSIGXCPU = 1;
volatile int handleSIGPIPE = 1;

/** Holds the id of the current execution */
__thread long globalSessionId = 0;

/** Holds the default log level */
int configuredLogLevel = ffmpegkit::LevelAVLogInfo;

#define FFKIT_RESOURCE_INPUT 1
#define FFKIT_RESOURCE_OUTPUT 2
#define FFKIT_DEFAULT_OUTPUT_CAPACITY 4096
#define FFKIT_DEFAULT_STREAM_CAPACITY 1048576

/*
 * The two helpers below (ffkit_max_alloc_size and ffkit_int64_add_overflow)
 * are duplicated verbatim in the Android, Apple and Linux FFmpegKit sources
 * because there is no shared native layer between the platforms. Keep all
 * three copies in sync: any change here must be mirrored in the others.
 */
static int64_t ffkit_max_alloc_size() {
    const uint64_t maxSize =
        (uint64_t)std::numeric_limits<size_t>::max();
    const uint64_t maxInt64 =
        (uint64_t)std::numeric_limits<int64_t>::max();
    return (int64_t)std::min(maxSize, maxInt64);
}

static bool ffkit_int64_add_overflow(const int64_t left, const int64_t right,
                                     int64_t *result) {
    if ((right > 0 &&
         left > std::numeric_limits<int64_t>::max() - right) ||
        (right < 0 &&
         left < std::numeric_limits<int64_t>::min() - right)) {
        return true;
    }

    *result = left + right;
    return false;
}

typedef struct FFKitMemoryResource {
    int64_t id;
    int type;
    uint8_t *data;
    int64_t size;
    int64_t capacity;
    int64_t maxCapacity;
    int ownsData;
    int unregistered;
    int openCount;
    pthread_mutex_t mutex;
    struct FFKitMemoryResource *next;
} FFKitMemoryResource;

typedef struct FFKitMemoryHandle {
    FFKitMemoryResource *resource;
    int64_t position;
    int flags;
} FFKitMemoryHandle;

typedef struct FFKitStreamResource {
    int64_t id;
    int type;
    uint8_t *data;
    int64_t capacity;
    int64_t size;
    int64_t readPosition;
    int64_t writePosition;
    int closed;
    int writeClosed;
    int unregistered;
    int openCount;
    pthread_mutex_t mutex;
    pthread_cond_t canRead;
    pthread_cond_t canWrite;
    struct FFKitStreamResource *next;
} FFKitStreamResource;

typedef struct FFKitStreamHandle {
    FFKitStreamResource *resource;
    int flags;
} FFKitStreamHandle;

static pthread_mutex_t ffkitMemoryRegistryMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ffkitStreamRegistryMutex = PTHREAD_MUTEX_INITIALIZER;
static FFKitMemoryResource *ffkitMemoryResources = NULL;
static FFKitStreamResource *ffkitStreamResources = NULL;
static std::atomic<long long> ffkitNextResourceId(1);

#ifdef USES_FFMPEG_KIT_PROTOCOLS
typedef int (*ffkit_local_protocol_open_function)(int64_t, int, void **);
typedef int (*ffkit_local_protocol_read_function)(void *, unsigned char *, int);
typedef int (*ffkit_local_protocol_write_function)(void *, const unsigned char *,
                                                   int);
typedef int64_t (*ffkit_local_protocol_seek_function)(void *, int64_t, int);
typedef int (*ffkit_local_protocol_close_function)(void *);

extern "C" void av_set_ffkitmem_functions(
    ffkit_local_protocol_open_function open_function,
    ffkit_local_protocol_read_function read_function,
    ffkit_local_protocol_write_function write_function,
    ffkit_local_protocol_seek_function seek_function,
    ffkit_local_protocol_close_function close_function);

extern "C" void av_set_ffkitstream_functions(
    ffkit_local_protocol_open_function open_function,
    ffkit_local_protocol_read_function read_function,
    ffkit_local_protocol_write_function write_function,
    ffkit_local_protocol_seek_function seek_function,
    ffkit_local_protocol_close_function close_function);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration for function defined in fftools/ffmpeg.c */
int ffmpeg_execute(int argc, char **argv);

/** Forward declaration for function defined in fftools/ffprobe.c */
int ffprobe_execute(int argc, char **argv);

void ffmpegkit_log_callback_function(void *ptr, int level, const char *format,
                                     va_list vargs);

/* Per-session log duties defined in the fftools (fftools/opt_common.c and
 * fftools/ffprobe.c) plus the FFmpegKit callbacks that invoke them. Declared with
 * C linkage so they resolve against the C-compiled fftools objects. */
void ffmpegkit_report_write(void *ptr, int level, const char *fmt, va_list vl);
void ffmpegkit_show_log_capture(void *ptr, int level, const char *fmt, va_list vl);
int ffmpegkit_redirection_enabled(void);
void ffmpegkit_log_callback_default(void *ptr, int level, const char *format,
                                    va_list vargs);

#ifdef __cplusplus
}
#endif

static std::once_flag ffmpegKitInitializerFlag;
static pthread_t callbackThread;

void *ffmpegKitInitialize();

const void *_ffmpegKitConfigInitializer{ffmpegKitInitialize()};

enum CallbackType { LogType, StatisticsType };

static bool fs_exists(const std::string &s, const bool isFile,
                      const bool isDirectory) {
    struct stat dir_info;

    if (stat(s.c_str(), &dir_info) == 0) {
        if (isFile && S_ISREG(dir_info.st_mode)) {
            return true;
        }
        if (isDirectory && S_ISDIR(dir_info.st_mode)) {
            return true;
        }
    }

    return false;
}

static bool fs_create_dir(const std::string &s) {
    if (!fs_exists(s, false, true)) {
        if (mkdir(s.c_str(), S_IRWXU | S_IRWXG | S_IROTH) != 0) {
            std::cout << "Failed to create directory: " << s
                      << ". Operation failed with " << errno << "."
                      << std::endl;
            return false;
        }
    }
    return true;
}

std::list<long> deleteExpiredSessionsLocked() {
    std::list<long> deletedSessionIds;

    while (sessionHistoryList.size() > sessionHistorySize) {
        auto first = sessionHistoryList.front();
        if (first != nullptr) {
            const long sessionId = first->getSessionId();
            sessionHistoryList.pop_front();
            sessionHistoryMap.erase(sessionId);
            deletedSessionIds.push_back(sessionId);
        }
    }

    return deletedSessionIds;
}

void notifySessionDeleted(const long sessionId) {
    std::list<std::shared_ptr<ffmpegkit::SessionDeleteListener>> listeners;

    std::unique_lock<std::recursive_mutex> listenerLock(
        sessionDeleteListenerMutex, std::defer_lock);
    listenerLock.lock();

    for (auto it = sessionDeleteListeners.begin();
         it != sessionDeleteListeners.end();) {
        auto listener = it->lock();
        if (listener == nullptr) {
            it = sessionDeleteListeners.erase(it);
        } else {
            listeners.push_back(listener);
            ++it;
        }
    }

    listenerLock.unlock();

    for (auto it = listeners.begin(); it != listeners.end(); ++it) {
        try {
            (*it)->sessionDeleted(sessionId);
        } catch (const std::exception &exception) {
            std::cout << "Exception thrown inside session delete listener. "
                      << exception.what() << std::endl;
        } catch (...) {
            std::cout << "Exception thrown inside session delete listener."
                      << std::endl;
        }
    }
}

void notifySessionsDeleted(const std::list<long> &sessionIds) {
    for (auto it = sessionIds.begin(); it != sessionIds.end(); ++it) {
        notifySessionDeleted(*it);
    }
}

void addSessionToSessionHistory(
    const std::shared_ptr<ffmpegkit::Session> session) {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    std::list<long> deletedSessionIds;

    const long sessionId = session->getSessionId();

    lock.lock();

    /*
     * ASYNC SESSIONS CALL THIS METHOD TWICE
     * THIS CHECK PREVENTS ADDING THE SAME SESSION AGAIN
     */
    if (sessionHistoryMap.count(sessionId) == 0) {
        sessionHistoryMap.insert({sessionId, session});
        sessionHistoryList.push_back(session);
        deletedSessionIds = deleteExpiredSessionsLocked();
    }

    lock.unlock();

    notifySessionsDeleted(deletedSessionIds);
}

/**
 * Callback data class.
 */
class CallbackData {
  public:
    CallbackData(const long sessionId, const int logLevel, const AVBPrint *data)
        : _type{LogType}, _sessionId{sessionId}, _logLevel{logLevel} {
        av_bprint_init(&_logData, 0, AV_BPRINT_SIZE_UNLIMITED);
        av_bprintf(&_logData, "%s", data->str);
    }

    CallbackData(const long sessionId, const int videoFrameNumber,
                 const float videoFps, const float videoQuality,
                 const int64_t size, const double time, const double bitrate,
                 const double speed)
        : _type{StatisticsType}, _sessionId{sessionId},
          _statisticsFrameNumber{videoFrameNumber}, _statisticsFps{videoFps},
          _statisticsQuality{videoQuality}, _statisticsSize{size},
          _statisticsTime{time}, _statisticsBitrate{bitrate},
          _statisticsSpeed{speed} {}

    CallbackType getType() { return _type; }

    long getSessionId() { return _sessionId; }

    int getLogLevel() { return _logLevel; }

    AVBPrint *getLogData() { return &_logData; }

    int getStatisticsFrameNumber() { return _statisticsFrameNumber; }

    float getStatisticsFps() { return _statisticsFps; }

    float getStatisticsQuality() { return _statisticsQuality; }

    int64_t getStatisticsSize() { return _statisticsSize; }

    double getStatisticsTime() { return _statisticsTime; }

    double getStatisticsBitrate() { return _statisticsBitrate; }

    double getStatisticsSpeed() { return _statisticsSpeed; }

  private:
    CallbackType _type;
    long _sessionId; // session id

    int _logLevel;     // log level
    AVBPrint _logData; // log data

    int _statisticsFrameNumber; // statistics frame number
    float _statisticsFps;       // statistics fps
    float _statisticsQuality;   // statistics quality
    int64_t _statisticsSize;    // statistics size
    double _statisticsTime;     // statistics time
    double _statisticsBitrate;  // statistics bitrate
    double _statisticsSpeed;    // statistics speed
};

/**
 * Waits on the callback semaphore for the given time.
 *
 * @param milliSeconds wait time in milliseconds
 */
static void callbackWait(int milliSeconds) {
    std::unique_lock<std::mutex> callbackLock{callbackMutex};
    callbackMonitor.wait_for(callbackLock,
                             std::chrono::milliseconds(milliSeconds));
}

/**
 * Notifies threads waiting on callback semaphore.
 */
static void callbackNotify() { callbackMonitor.notify_one(); }

static const char *avutil_log_get_level_str(int level) {
    switch (level) {
    case AV_LOG_STDERR:
        return "stderr";
    case AV_LOG_QUIET:
        return "quiet";
    case AV_LOG_DEBUG:
        return "debug";
    case AV_LOG_VERBOSE:
        return "verbose";
    case AV_LOG_INFO:
        return "info";
    case AV_LOG_WARNING:
        return "warning";
    case AV_LOG_ERROR:
        return "error";
    case AV_LOG_FATAL:
        return "fatal";
    case AV_LOG_PANIC:
        return "panic";
    default:
        return "";
    }
}

static void avutil_log_format_line(void *avcl, int level, const char *fmt,
                                   va_list vl, AVBPrint part[4],
                                   int *print_prefix) {
    int flags = av_log_get_flags();
    AVClass *avc = avcl ? *(AVClass **)avcl : NULL;
    av_bprint_init(part + 0, 0, 1);
    av_bprint_init(part + 1, 0, 1);
    av_bprint_init(part + 2, 0, 1);
    av_bprint_init(part + 3, 0, 65536);

    if (*print_prefix && avc) {
        if (avc->parent_log_context_offset) {
            AVClass **parent = *(AVClass ***)(((uint8_t *)avcl) +
                                              avc->parent_log_context_offset);
            if (parent && *parent) {
                av_bprintf(part + 0, "[%s @ %p] ", (*parent)->item_name(parent),
                           parent);
            }
        }
        av_bprintf(part + 1, "[%s @ %p] ", avc->item_name(avcl), avcl);
    }

    if (*print_prefix && (level > AV_LOG_QUIET) && (flags & AV_LOG_PRINT_LEVEL))
        av_bprintf(part + 2, "[%s] ", avutil_log_get_level_str(level));

    av_vbprintf(part + 3, fmt, vl);

    if (*part[0].str || *part[1].str || *part[2].str || *part[3].str) {
        char lastc = part[3].len && part[3].len <= part[3].size
                         ? part[3].str[part[3].len - 1]
                         : 0;
        *print_prefix = lastc == '\n' || lastc == '\r';
    }
}

static void avutil_log_sanitize(char *line) {
    while (*line) {
        if (*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line = '?';
        line++;
    }
}

/**
 * Adds log data to the end of callback data list.
 *
 * @param level log level
 * @param data log data
 */
static void logCallbackDataAdd(int level, AVBPrint *data) {
    std::unique_lock<std::recursive_mutex> lock(callbackDataMutex,
                                                std::defer_lock);
    CallbackData *callbackData = new CallbackData(globalSessionId, level, data);

    lock.lock();
    callbackDataList.push_back(callbackData);
    lock.unlock();

    callbackNotify();

    std::atomic_fetch_add(
        &sessionInTransitMessageCountMap[globalSessionId % SESSION_MAP_SIZE],
        1);
}

/**
 * Adds statistics data to the end of callback data list.
 */
static void statisticsCallbackDataAdd(int frameNumber, float fps, float quality,
                                      int64_t size, int time, double bitrate,
                                      double speed) {
    std::unique_lock<std::recursive_mutex> lock(callbackDataMutex,
                                                std::defer_lock);
    CallbackData *callbackData = new CallbackData(
        globalSessionId, frameNumber, fps, quality, size, time, bitrate, speed);

    lock.lock();
    callbackDataList.push_back(callbackData);
    lock.unlock();

    callbackNotify();

    std::atomic_fetch_add(
        &sessionInTransitMessageCountMap[globalSessionId % SESSION_MAP_SIZE],
        1);
}

/**
 * Removes head of callback data list.
 */
static CallbackData *callbackDataRemove() {
    std::unique_lock<std::recursive_mutex> lock(callbackDataMutex,
                                                std::defer_lock);
    CallbackData *newData = nullptr;

    lock.lock();
    if (callbackDataList.size() > 0) {
        newData = callbackDataList.front();
        callbackDataList.pop_front();
    }
    lock.unlock();

    return newData;
}

/**
 * Registers a session id to the session map.
 *
 * @param sessionId session id
 */
static void registerSessionId(long sessionId) {
    std::atomic_store(&sessionMap[sessionId % SESSION_MAP_SIZE], (short)1);
}

/**
 * Removes a session id from the session map.
 *
 * @param sessionId session id
 */
static void removeSession(long sessionId) {
    std::atomic_store(&sessionMap[sessionId % SESSION_MAP_SIZE], (short)0);
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Adds a cancel session request to the session map.
 *
 * @param sessionId session id
 */
void cancelSession(long sessionId) {
    std::atomic_store(&sessionMap[sessionId % SESSION_MAP_SIZE], (short)2);
}

/**
 * Adds a cancel session request for every running session to the session map.
 * Only sessions currently marked running (1) are switched to cancel (2); slots
 * that are idle (0) or already cancelling (2) are left untouched, and a session
 * that starts after this call claims its own slot and is therefore unaffected.
 */
void cancelAllSessions(void) {
    for (int i = 0; i < SESSION_MAP_SIZE; i++) {
        short running = 1;
        std::atomic_compare_exchange_strong(&sessionMap[i], &running, (short)2);
    }
}

/**
 * Checks whether a cancel request for the given session id exists in the
 * session map.
 *
 * @param sessionId session id
 * @return 1 if exists, false otherwise
 */
int cancelRequested(long sessionId) {
    if (std::atomic_load(&sessionMap[sessionId % SESSION_MAP_SIZE]) == 2) {
        return 1;
    } else {
        return 0;
    }
}

#ifdef __cplusplus
}
#endif

/**
 * Resets the number of messages in transmit for this session.
 *
 * @param sessionId session id
 */
static void resetMessagesInTransmit(long sessionId) {
    std::atomic_store(
        &sessionInTransitMessageCountMap[sessionId % SESSION_MAP_SIZE], 0);
}

static int ffkit_cond_wait(pthread_cond_t *condition, pthread_mutex_t *mutex,
                           int timeoutMs) {
    if (timeoutMs < 0) {
        return pthread_cond_wait(condition, mutex);
    }

    if (timeoutMs == 0) {
        return ETIMEDOUT;
    }

    struct timeval tv;
    struct timespec ts;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec + timeoutMs / 1000;
    ts.tv_nsec = (tv.tv_usec * 1000) + ((timeoutMs % 1000) * 1000000);
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec %= 1000000000L;

    return pthread_cond_timedwait(condition, mutex, &ts);
}

static int64_t ffkit_next_resource_id() {
    return ffkitNextResourceId.fetch_add(1);
}

static FFKitMemoryResource *ffkit_memory_find_locked(int64_t id) {
    FFKitMemoryResource *current = ffkitMemoryResources;

    while (current != NULL) {
        if (current->id == id) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static void ffkit_memory_free(FFKitMemoryResource *resource) {
    if (resource == NULL) {
        return;
    }

    if (resource->ownsData && resource->data != NULL) {
        av_free(resource->data);
    }

    pthread_mutex_destroy(&resource->mutex);
    av_free(resource);
}

static void ffkit_memory_add(FFKitMemoryResource *resource) {
    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource->next = ffkitMemoryResources;
    ffkitMemoryResources = resource;
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);
}

static FFKitStreamResource *ffkit_stream_find_locked(int64_t id) {
    FFKitStreamResource *current = ffkitStreamResources;

    while (current != NULL) {
        if (current->id == id) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static void ffkit_stream_free(FFKitStreamResource *resource) {
    if (resource == NULL) {
        return;
    }

    if (resource->data != NULL) {
        av_free(resource->data);
    }

    pthread_cond_destroy(&resource->canRead);
    pthread_cond_destroy(&resource->canWrite);
    pthread_mutex_destroy(&resource->mutex);
    av_free(resource);
}

static void ffkit_stream_add(FFKitStreamResource *resource) {
    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource->next = ffkitStreamResources;
    ffkitStreamResources = resource;
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);
}

static int ffkit_memory_open(int64_t id, int flags, void **opaque) {
    FFKitMemoryResource *resource = NULL;
    FFKitMemoryHandle *handle = NULL;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        if (resource->unregistered ||
            ((flags & AVIO_FLAG_WRITE) &&
             resource->type != FFKIT_RESOURCE_OUTPUT) ||
            (!(flags & AVIO_FLAG_WRITE) &&
             resource->type != FFKIT_RESOURCE_INPUT)) {
            pthread_mutex_unlock(&resource->mutex);
            pthread_mutex_unlock(&ffkitMemoryRegistryMutex);
            return AVERROR(ENOENT);
        }
        resource->openCount++;
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    if (resource == NULL) {
        return AVERROR(ENOENT);
    }

    handle = (FFKitMemoryHandle *)av_mallocz(sizeof(FFKitMemoryHandle));
    if (handle == NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->openCount--;
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(ENOMEM);
    }

    handle->resource = resource;
    handle->flags = flags;
    handle->position = 0;
    *opaque = handle;
    return 0;
}

static int ffkit_memory_read(void *opaque, unsigned char *buf, int size) {
    FFKitMemoryHandle *handle = (FFKitMemoryHandle *)opaque;
    FFKitMemoryResource *resource;
    int bytesToRead;

    if (handle == NULL || buf == NULL || size < 0) {
        return AVERROR(EINVAL);
    }

    resource = handle->resource;
    pthread_mutex_lock(&resource->mutex);
    if (resource->type != FFKIT_RESOURCE_INPUT) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EBADF);
    }

    if (handle->position >= resource->size) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR_EOF;
    }

    bytesToRead = (int)FFMIN((int64_t)size, resource->size - handle->position);
    memcpy(buf, resource->data + handle->position, bytesToRead);
    handle->position += bytesToRead;
    pthread_mutex_unlock(&resource->mutex);

    return bytesToRead;
}

static int ffkit_memory_ensure_capacity(FFKitMemoryResource *resource,
                                        int64_t requiredCapacity) {
    int64_t newCapacity;
    uint8_t *newData;

    if (requiredCapacity <= resource->capacity) {
        return 0;
    }

    if (requiredCapacity > resource->maxCapacity) {
        return AVERROR(ENOSPC);
    }

    newCapacity = resource->capacity > 0 ? resource->capacity
                                         : FFKIT_DEFAULT_OUTPUT_CAPACITY;
    while (newCapacity < requiredCapacity) {
        if (newCapacity > resource->maxCapacity / 2) {
            newCapacity = resource->maxCapacity;
        } else {
            newCapacity *= 2;
        }
    }

    newData = (uint8_t *)av_realloc(resource->data, newCapacity);
    if (newData == NULL) {
        return AVERROR(ENOMEM);
    }

    resource->data = newData;
    resource->capacity = newCapacity;
    return 0;
}

static int ffkit_memory_write(void *opaque, const unsigned char *buf, int size) {
    FFKitMemoryHandle *handle = (FFKitMemoryHandle *)opaque;
    FFKitMemoryResource *resource;
    int ret;
    int64_t requiredSize;

    if (handle == NULL || buf == NULL || size < 0) {
        return AVERROR(EINVAL);
    }

    resource = handle->resource;
    pthread_mutex_lock(&resource->mutex);
    if (resource->type != FFKIT_RESOURCE_OUTPUT) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EBADF);
    }

    if (ffkit_int64_add_overflow(handle->position, size, &requiredSize)) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EOVERFLOW);
    }

    ret = ffkit_memory_ensure_capacity(resource, requiredSize);
    if (ret < 0) {
        pthread_mutex_unlock(&resource->mutex);
        return ret;
    }

    if (handle->position > resource->size) {
        memset(resource->data + resource->size, 0,
               handle->position - resource->size);
    }

    memcpy(resource->data + handle->position, buf, size);
    handle->position = requiredSize;
    if (requiredSize > resource->size) {
        resource->size = requiredSize;
    }
    pthread_mutex_unlock(&resource->mutex);

    return size;
}

static int64_t ffkit_memory_seek(void *opaque, int64_t pos, int whence) {
    FFKitMemoryHandle *handle = (FFKitMemoryHandle *)opaque;
    FFKitMemoryResource *resource;
    int64_t newPosition;

    if (handle == NULL) {
        return AVERROR(EINVAL);
    }

    resource = handle->resource;
    pthread_mutex_lock(&resource->mutex);

    if (whence == AVSEEK_SIZE) {
        int64_t size = resource->size;
        pthread_mutex_unlock(&resource->mutex);
        return size;
    }

    if (whence == SEEK_SET) {
        newPosition = pos;
    } else if (whence == SEEK_CUR) {
        if (ffkit_int64_add_overflow(handle->position, pos, &newPosition)) {
            pthread_mutex_unlock(&resource->mutex);
            return AVERROR(EOVERFLOW);
        }
    } else if (whence == SEEK_END) {
        if (ffkit_int64_add_overflow(resource->size, pos, &newPosition)) {
            pthread_mutex_unlock(&resource->mutex);
            return AVERROR(EOVERFLOW);
        }
    } else {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EINVAL);
    }

    if (newPosition < 0) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EINVAL);
    }

    handle->position = newPosition;
    pthread_mutex_unlock(&resource->mutex);

    return newPosition;
}

static int ffkit_memory_close(void *opaque) {
    FFKitMemoryHandle *handle = (FFKitMemoryHandle *)opaque;
    FFKitMemoryResource *resource;
    int freeResource = 0;

    if (handle == NULL) {
        return 0;
    }

    resource = handle->resource;
    pthread_mutex_lock(&resource->mutex);
    if (resource->openCount > 0) {
        resource->openCount--;
    }
    freeResource = resource->unregistered && resource->openCount == 0;
    pthread_mutex_unlock(&resource->mutex);

    av_free(handle);

    if (freeResource) {
        ffkit_memory_free(resource);
    }

    return 0;
}

static int ffkit_stream_open(int64_t id, int flags, void **opaque) {
    FFKitStreamResource *resource = NULL;
    FFKitStreamHandle *handle = NULL;

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        if (resource->unregistered ||
            ((flags & AVIO_FLAG_WRITE) &&
             resource->type != FFKIT_RESOURCE_OUTPUT) ||
            (!(flags & AVIO_FLAG_WRITE) &&
             resource->type != FFKIT_RESOURCE_INPUT)) {
            pthread_mutex_unlock(&resource->mutex);
            pthread_mutex_unlock(&ffkitStreamRegistryMutex);
            return AVERROR(ENOENT);
        }
        resource->openCount++;
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);

    if (resource == NULL) {
        return AVERROR(ENOENT);
    }

    handle = (FFKitStreamHandle *)av_mallocz(sizeof(FFKitStreamHandle));
    if (handle == NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->openCount--;
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(ENOMEM);
    }

    handle->resource = resource;
    handle->flags = flags;
    *opaque = handle;
    return 0;
}

static int ffkit_stream_write_bytes(FFKitStreamResource *resource,
                                    const uint8_t *buf, int size,
                                    int timeoutMs) {
    int written = 0;

    if (resource == NULL || (buf == NULL && size > 0) || size < 0) {
        return AVERROR(EINVAL);
    }

    pthread_mutex_lock(&resource->mutex);
    while (written < size) {
        while (resource->size == resource->capacity && !resource->closed &&
               !resource->writeClosed) {
            int waitRc =
                ffkit_cond_wait(&resource->canWrite, &resource->mutex,
                                timeoutMs);
            if (waitRc == ETIMEDOUT) {
                pthread_mutex_unlock(&resource->mutex);
                return written;
            }
        }

        if (resource->closed || resource->writeClosed) {
            pthread_mutex_unlock(&resource->mutex);
            return written > 0 ? written : AVERROR(EPIPE);
        }

        int64_t freeSize = resource->capacity - resource->size;
        int chunkSize = (int)FFMIN((int64_t)(size - written), freeSize);
        chunkSize = (int)FFMIN((int64_t)chunkSize,
                               resource->capacity - resource->writePosition);

        memcpy(resource->data + resource->writePosition, buf + written,
               chunkSize);
        resource->writePosition =
            (resource->writePosition + chunkSize) % resource->capacity;
        resource->size += chunkSize;
        written += chunkSize;
        pthread_cond_broadcast(&resource->canRead);
    }
    pthread_mutex_unlock(&resource->mutex);

    return written;
}

static int ffkit_stream_read_bytes(FFKitStreamResource *resource, uint8_t *buf,
                                   int size, int timeoutMs, int *timedOut,
                                   int *eof) {
    int bytesRead = 0;

    if (timedOut != NULL) {
        *timedOut = 0;
    }
    if (eof != NULL) {
        *eof = 0;
    }

    if (resource == NULL || buf == NULL || size < 0) {
        return AVERROR(EINVAL);
    }

    pthread_mutex_lock(&resource->mutex);
    while (resource->size == 0 && !resource->writeClosed && !resource->closed) {
        int waitRc =
            ffkit_cond_wait(&resource->canRead, &resource->mutex, timeoutMs);
        if (waitRc == ETIMEDOUT) {
            if (timedOut != NULL) {
                *timedOut = 1;
            }
            pthread_mutex_unlock(&resource->mutex);
            return 0;
        }
    }

    if (resource->size == 0 && resource->writeClosed) {
        if (eof != NULL) {
            *eof = 1;
        }
        pthread_mutex_unlock(&resource->mutex);
        return 0;
    }

    if (resource->size == 0 && resource->closed) {
        pthread_mutex_unlock(&resource->mutex);
        return AVERROR(EPIPE);
    }

    while (bytesRead < size && resource->size > 0) {
        int chunkSize = (int)FFMIN((int64_t)(size - bytesRead), resource->size);
        chunkSize = (int)FFMIN((int64_t)chunkSize,
                               resource->capacity - resource->readPosition);

        memcpy(buf + bytesRead, resource->data + resource->readPosition,
               chunkSize);
        resource->readPosition =
            (resource->readPosition + chunkSize) % resource->capacity;
        resource->size -= chunkSize;
        bytesRead += chunkSize;
        pthread_cond_broadcast(&resource->canWrite);
    }
    pthread_mutex_unlock(&resource->mutex);

    return bytesRead;
}

static int ffkit_stream_read(void *opaque, unsigned char *buf, int size) {
    FFKitStreamHandle *handle = (FFKitStreamHandle *)opaque;
    int eof = 0;
    int ret;

    if (handle == NULL) {
        return AVERROR(EINVAL);
    }

    ret = ffkit_stream_read_bytes(handle->resource, buf, size, -1, NULL, &eof);
    return eof ? AVERROR_EOF : ret;
}

static int ffkit_stream_write(void *opaque, const unsigned char *buf, int size) {
    FFKitStreamHandle *handle = (FFKitStreamHandle *)opaque;

    if (handle == NULL) {
        return AVERROR(EINVAL);
    }

    return ffkit_stream_write_bytes(handle->resource, buf, size, -1);
}

static int64_t ffkit_stream_seek(void *opaque, int64_t pos, int whence) {
    return AVERROR(ESPIPE);
}

static int ffkit_stream_close(void *opaque) {
    FFKitStreamHandle *handle = (FFKitStreamHandle *)opaque;
    FFKitStreamResource *resource;
    int freeResource = 0;

    if (handle == NULL) {
        return 0;
    }

    resource = handle->resource;
    pthread_mutex_lock(&resource->mutex);
    if (resource->type == FFKIT_RESOURCE_OUTPUT) {
        resource->writeClosed = 1;
    } else {
        resource->closed = 1;
    }
    if (resource->openCount > 0) {
        resource->openCount--;
    }
    freeResource = resource->unregistered && resource->openCount == 0;
    pthread_cond_broadcast(&resource->canRead);
    pthread_cond_broadcast(&resource->canWrite);
    pthread_mutex_unlock(&resource->mutex);

    av_free(handle);

    if (freeResource) {
        ffkit_stream_free(resource);
    }

    return 0;
}

/**
 * Callback function for FFmpeg/FFprobe logs.
 *
 * @param ptr pointer to AVClass struct
 * @param level log level
 * @param format format string
 * @param vargs arguments
 */
/*
 * FFmpegKitNext per-session log "duties".
 *
 * -report (ffmpeg) and -show_log (ffprobe) used to each install their own global
 * av_log callback, hijacking redirection and (for ffprobe) needing a manual
 * restore. Instead they now just leave a signal in place (report_file != NULL,
 * do_show_log != 0) and the single, always-installed FFmpegKit callback invokes
 * these passive duty routines for every log line. Each duty self-guards, so both
 * are cheap no-ops when their feature is inactive. A va_list is consumed once, so
 * each duty gets its own va_copy. Neither duty may call av_log() (it would recurse
 * back into the callback).
 */
static void run_duties(void *ptr, int level, const char *format,
                       va_list vargs) {
    va_list copy;

    va_copy(copy, vargs);
    ffmpegkit_report_write(ptr, level, format, copy);
    va_end(copy);

    va_copy(copy, vargs);
    ffmpegkit_show_log_capture(ptr, level, format, copy);
    va_end(copy);
}

void ffmpegkit_log_callback_function(void *ptr, int level, const char *format,
                                     va_list vargs) {
    AVBPrint fullLine;
    AVBPrint part[4];
    int print_prefix = 1;

    // DO NOT PROCESS UNWANTED LOGS
    if (level >= 0) {
        level &= 0xff;
    }

    // Run -report / -show_log capture for every line, independent of the console
    // log-level filter below (the report keeps its own report_file_level threshold).
    run_duties(ptr, level, format, vargs);

    int activeLogLevel = av_log_get_level();

    // LevelAVLogStdErr logs are always redirected
    if ((activeLogLevel == ffmpegkit::LevelAVLogQuiet &&
         level != ffmpegkit::LevelAVLogStdErr) ||
        (level > activeLogLevel)) {
        return;
    }

    av_bprint_init(&fullLine, 0, AV_BPRINT_SIZE_UNLIMITED);

    avutil_log_format_line(ptr, level, format, vargs, part, &print_prefix);
    avutil_log_sanitize(part[0].str);
    avutil_log_sanitize(part[1].str);
    avutil_log_sanitize(part[2].str);
    avutil_log_sanitize(part[3].str);

    // COMBINE ALL 4 LOG PARTS
    av_bprintf(&fullLine, "%s%s%s%s", part[0].str, part[1].str, part[2].str,
               part[3].str);

    if (fullLine.len > 0) {
        logCallbackDataAdd(level, &fullLine);
    }

    av_bprint_finalize(part, NULL);
    av_bprint_finalize(part + 1, NULL);
    av_bprint_finalize(part + 2, NULL);
    av_bprint_finalize(part + 3, NULL);
    av_bprint_finalize(&fullLine, NULL);
}

int ffmpegkit_redirection_enabled(void) { return redirectionEnabled; }

/*
 * FFmpegKitNext: av_log callback installed when redirection is DISABLED.
 * Reproduces stock ffmpeg logging (stderr) while still running the per-session
 * duties, so -report / -show_log keep working even with redirection off.
 */
void ffmpegkit_log_callback_default(void *ptr, int level, const char *format,
                                    va_list vargs) {
    va_list copy;

    va_copy(copy, vargs);
    av_log_default_callback(ptr, level, format, copy);
    va_end(copy);

    run_duties(ptr, level, format, vargs);
}

/**
 * Callback function for FFmpeg statistics.
 *
 * @param frameNumber last processed frame number
 * @param fps frames processed per second
 * @param quality quality of the output stream (video only)
 * @param size size in bytes
 * @param time processed output duration
 * @param bitrate output bit rate in kbits/s
 * @param speed processing speed = processed duration / operation duration
 */
void ffmpegkit_statistics_callback_function(int frameNumber, float fps,
                                            float quality, int64_t size,
                                            double time, double bitrate,
                                            double speed) {
    statisticsCallbackDataAdd(frameNumber, fps, quality, size, time, bitrate,
                              speed);
}

static void process_log(long sessionId, int levelValueInt,
                        AVBPrint *logMessage) {
    int activeLogLevel = av_log_get_level();
    ffmpegkit::Level levelValue = static_cast<ffmpegkit::Level>(levelValueInt);
    std::shared_ptr<ffmpegkit::Log> log = std::make_shared<ffmpegkit::Log>(
        sessionId, levelValue, logMessage->str);
    bool globalCallbackDefined = false;
    bool sessionCallbackDefined = false;
    ffmpegkit::LogRedirectionStrategy activeLogRedirectionStrategy =
        globalLogRedirectionStrategy;

    // LevelAVLogStdErr logs are always redirected
    if ((activeLogLevel == ffmpegkit::LevelAVLogQuiet &&
         levelValue != ffmpegkit::LevelAVLogStdErr) ||
        (levelValue > activeLogLevel)) {
        // LOG NEITHER PRINTED NOR FORWARDED
        return;
    }

    auto session = ffmpegkit::FFmpegKitConfig::getSession(sessionId);
    if (session != nullptr) {
        activeLogRedirectionStrategy = session->getLogRedirectionStrategy();
        session->addLog(log);

        ffmpegkit::LogCallback sessionLogCallback = session->getLogCallback();
        if (sessionLogCallback != nullptr) {
            sessionCallbackDefined = true;

            try {
                // NOTIFY SESSION CALLBACK DEFINED
                sessionLogCallback(log);
            } catch (const std::exception &exception) {
                std::cout << "Exception thrown inside session log callback. "
                          << exception.what() << std::endl;
            }
        }
    }

    ffmpegkit::LogCallback globalLogCallback = logCallback;
    if (globalLogCallback != nullptr) {
        globalCallbackDefined = true;

        try {
            // NOTIFY GLOBAL CALLBACK DEFINED
            globalLogCallback(log);
        } catch (const std::exception &exception) {
            std::cout << "Exception thrown inside global log callback. "
                      << exception.what() << std::endl;
        }
    }

    // EXECUTE THE LOG STRATEGY
    switch (activeLogRedirectionStrategy) {
    case ffmpegkit::LogRedirectionStrategyNeverPrintLogs: {
        return;
    }
    case ffmpegkit::
        LogRedirectionStrategyPrintLogsWhenGlobalCallbackNotDefined: {
        if (globalCallbackDefined) {
            return;
        }
    } break;
    case ffmpegkit::
        LogRedirectionStrategyPrintLogsWhenSessionCallbackNotDefined: {
        if (sessionCallbackDefined) {
            return;
        }
    } break;
    case ffmpegkit::LogRedirectionStrategyPrintLogsWhenNoCallbacksDefined: {
        if (globalCallbackDefined || sessionCallbackDefined) {
            return;
        }
    } break;
    case ffmpegkit::LogRedirectionStrategyAlwaysPrintLogs: {
    } break;
    }

    // PRINT LOGS
    switch (levelValue) {
    case ffmpegkit::LevelAVLogQuiet:
        // PRINT NO OUTPUT
        break;
    default:
        // WRITE TO STDOUT
        std::cout << ffmpegkit::FFmpegKitConfig::logLevelToString(levelValue)
                  << ": " << logMessage->str;
        break;
    }
}

void process_statistics(long sessionId, int videoFrameNumber, float videoFps,
                        float videoQuality, long size, double time,
                        double bitrate, double speed) {
    std::shared_ptr<ffmpegkit::Statistics> statistics =
        std::make_shared<ffmpegkit::Statistics>(sessionId, videoFrameNumber,
                                                videoFps, videoQuality, size,
                                                time, bitrate, speed);

    auto session = ffmpegkit::FFmpegKitConfig::getSession(sessionId);
    if (session != nullptr && session->isFFmpeg()) {
        std::shared_ptr<ffmpegkit::FFmpegSession> ffmpegSession =
            std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
        ffmpegSession->addStatistics(statistics);

        ffmpegkit::StatisticsCallback sessionStatisticsCallback =
            ffmpegSession->getStatisticsCallback();
        if (sessionStatisticsCallback != nullptr) {
            try {
                sessionStatisticsCallback(statistics);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside session statistics callback. "
                    << exception.what() << std::endl;
            }
        }
    }

    ffmpegkit::StatisticsCallback globalStatisticsCallback = statisticsCallback;
    if (globalStatisticsCallback != nullptr) {
        try {
            globalStatisticsCallback(statistics);
        } catch (const std::exception &exception) {
            std::cout << "Exception thrown inside global statistics callback. "
                      << exception.what() << std::endl;
        }
    }
}

/**
 * Forwards asynchronous messages to Callbacks.
 */
void *callbackThreadFunction(void *pointer) {
    int activeLogLevel = av_log_get_level();
    if ((activeLogLevel != ffmpegkit::LevelAVLogQuiet) &&
        (ffmpegkit::LevelAVLogDebug <= activeLogLevel)) {
        std::cout << "Async callback block started." << std::endl;
    }

    while (redirectionEnabled) {
        try {
            CallbackData *callbackData = callbackDataRemove();

            if (callbackData != nullptr) {

                if (callbackData->getType() == LogType) {
                    process_log(callbackData->getSessionId(),
                                callbackData->getLogLevel(),
                                callbackData->getLogData());
                    av_bprint_finalize(callbackData->getLogData(), NULL);
                } else {
                    process_statistics(callbackData->getSessionId(),
                                       callbackData->getStatisticsFrameNumber(),
                                       callbackData->getStatisticsFps(),
                                       callbackData->getStatisticsQuality(),
                                       callbackData->getStatisticsSize(),
                                       callbackData->getStatisticsTime(),
                                       callbackData->getStatisticsBitrate(),
                                       callbackData->getStatisticsSpeed());
                }

                std::atomic_fetch_sub(
                    &sessionInTransitMessageCountMap
                        [callbackData->getSessionId() % SESSION_MAP_SIZE],
                    1);

            } else {
                callbackWait(100);
            }

        } catch (const std::exception &exception) {
            activeLogLevel = av_log_get_level();
            if ((activeLogLevel != ffmpegkit::LevelAVLogQuiet) &&
                (ffmpegkit::LevelAVLogWarning <= activeLogLevel)) {
                std::cout << "Async callback block received error: "
                          << exception.what() << std::endl;
            }
        }
    }

    activeLogLevel = av_log_get_level();
    if ((activeLogLevel != ffmpegkit::LevelAVLogQuiet) &&
        (ffmpegkit::LevelAVLogDebug <= activeLogLevel)) {
        std::cout << "Async callback block stopped." << std::endl;
    }

    return NULL;
}

static int
executeFFmpeg(const long sessionId,
              const std::shared_ptr<std::list<std::string>> arguments) {
    const char *LIB_NAME = "ffmpeg";

    // SETS DEFAULT LOG LEVEL BEFORE STARTING A NEW RUN
    av_log_set_level(configuredLogLevel);

    char **commandCharPArray =
        (char **)av_malloc(sizeof(char *) * (arguments->size() + 2));

    /* PRESERVE USAGE FORMAT
     *
     * ffmpeg <arguments>
     */
    commandCharPArray[0] =
        (char *)av_malloc(sizeof(char) * (strlen(LIB_NAME) + 1));
    strcpy(commandCharPArray[0], LIB_NAME);

    // PREPARE ARRAY ELEMENTS
    int i = 0;
    for (auto it = arguments->begin(); it != arguments->end(); it++, i++) {
        commandCharPArray[i + 1] = (char *)it->c_str();
    }
    commandCharPArray[arguments->size() + 1] = NULL;

    // REGISTER THE ID BEFORE STARTING THE SESSION
    globalSessionId = sessionId;
    registerSessionId(sessionId);

    resetMessagesInTransmit(sessionId);

    // RUN
    int returnCode = ffmpeg_execute((arguments->size() + 1), commandCharPArray);

    // ALWAYS REMOVE THE ID FROM THE MAP
    removeSession(sessionId);

    // CLEANUP
    av_free(commandCharPArray[0]);
    av_free(commandCharPArray);

    return returnCode;
}

int executeFFprobe(const long sessionId,
                   const std::shared_ptr<std::list<std::string>> arguments) {
    const char *LIB_NAME = "ffprobe";

    // SETS DEFAULT LOG LEVEL BEFORE STARTING A NEW RUN
    av_log_set_level(configuredLogLevel);

    char **commandCharPArray =
        (char **)av_malloc(sizeof(char *) * (arguments->size() + 2));

    /* PRESERVE USAGE FORMAT
     *
     * ffprobe <arguments>
     */
    commandCharPArray[0] =
        (char *)av_malloc(sizeof(char) * (strlen(LIB_NAME) + 1));
    strcpy(commandCharPArray[0], LIB_NAME);

    // PREPARE ARRAY ELEMENTS
    int i = 0;
    for (auto it = arguments->begin(); it != arguments->end(); it++, i++) {
        commandCharPArray[i + 1] = (char *)it->c_str();
    }
    commandCharPArray[arguments->size() + 1] = NULL;

    // REGISTER THE ID BEFORE STARTING THE SESSION
    globalSessionId = sessionId;
    registerSessionId(sessionId);

    resetMessagesInTransmit(sessionId);

    // RUN
    int returnCode =
        ffprobe_execute((arguments->size() + 1), commandCharPArray);

    // ALWAYS REMOVE THE ID FROM THE MAP
    removeSession(sessionId);

    // CLEANUP
    av_free(commandCharPArray[0]);
    av_free(commandCharPArray);

    return returnCode;
}

void *ffmpegKitInitialize() {
    std::call_once(ffmpegKitInitializerFlag, []() {
        std::cout << "Loading ffmpeg-kit-next." << std::endl;

        sessionHistorySize = 10;

        for (int i = 0; i < SESSION_MAP_SIZE; i++) {
            std::atomic_init(&sessionMap[i], (short)0);
            std::atomic_init(&sessionInTransitMessageCountMap[i], 0);
        }

        logCallback = nullptr;
        statisticsCallback = nullptr;
        ffmpegSessionCompleteCallback = nullptr;
        ffprobeSessionCompleteCallback = nullptr;
        mediaInformationSessionCompleteCallback = nullptr;

        globalLogRedirectionStrategy =
            ffmpegkit::LogRedirectionStrategyPrintLogsWhenNoCallbacksDefined;

        redirectionEnabled = 0;

#ifdef USES_FFMPEG_KIT_PROTOCOLS
        av_set_ffkitmem_functions(ffkit_memory_open, ffkit_memory_read,
                                  ffkit_memory_write, ffkit_memory_seek,
                                  ffkit_memory_close);
        av_set_ffkitstream_functions(ffkit_stream_open, ffkit_stream_read,
                                     ffkit_stream_write, ffkit_stream_seek,
                                     ffkit_stream_close);
#endif

        ffmpegkit::FFmpegKitConfig::enableRedirection();

        std::cout << "Loaded ffmpeg-kit-next-"
                  << ffmpegkit::ArchDetect::getArch() << "-"
                  << ffmpegkit::FFmpegKitConfig::getVersion() << "-"
                  << ffmpegkit::FFmpegKitConfig::getBuildDate() << "."
                  << std::endl;
    });

    return NULL;
}

void ffmpegkit::FFmpegKitConfig::enableRedirection() {
    std::unique_lock<std::recursive_mutex> lock(callbackDataMutex,
                                                std::defer_lock);
    lock.lock();

    if (redirectionEnabled != 0) {
        lock.unlock();
        return;
    }
    redirectionEnabled = 1;

    lock.unlock();

    int rc =
        pthread_create(&callbackThread, NULL, callbackThreadFunction, NULL);
    if (rc != 0) {
        std::cout << "Failed to create async callback block: %d" << rc
                  << std::endl;
        lock.unlock();
        return;
    }

    av_log_set_callback(ffmpegkit_log_callback_function);
    set_report_callback(ffmpegkit_statistics_callback_function);
}

void ffmpegkit::FFmpegKitConfig::disableRedirection() {
    std::unique_lock<std::recursive_mutex> lock(callbackDataMutex,
                                                std::defer_lock);

    lock.lock();

    if (redirectionEnabled != 1) {
        lock.unlock();
        return;
    }
    redirectionEnabled = 0;

    lock.unlock();

    callbackNotify();

    pthread_detach(callbackThread);

    av_log_set_callback(ffmpegkit_log_callback_default);
    set_report_callback(NULL);
}

int ffmpegkit::FFmpegKitConfig::setFontconfigConfigurationPath(
    const std::string &path) {
    return ffmpegkit::FFmpegKitConfig::setEnvironmentVariable("FONTCONFIG_PATH",
                                                              path);
}

void ffmpegkit::FFmpegKitConfig::setFontDirectory(
    const std::string &fontDirectoryPath,
    const std::map<std::string, std::string> &fontNameMapping) {
    ffmpegkit::FFmpegKitConfig::setFontDirectoryList(
        std::list<std::string>{fontDirectoryPath}, fontNameMapping);
}

void ffmpegkit::FFmpegKitConfig::setFontDirectoryList(
    const std::list<std::string> &fontDirectoryList,
    const std::map<std::string, std::string> &fontNameMapping) {
    int validFontNameMappingCount = 0;

    const char *parentDirectory = std::getenv("HOME");
    if (parentDirectory == NULL) {
        parentDirectory = std::getenv("TMPDIR");
        if (parentDirectory == NULL) {
            parentDirectory = ".";
        }
    }

    std::string cacheDir = std::string(parentDirectory) + "/.cache";
    std::string ffmpegKitDir = cacheDir + "/ffmpegkit";
    auto tempConfigurationDirectory = ffmpegKitDir + "/fontconfig";
    auto fontConfigurationFile =
        std::string(tempConfigurationDirectory) + "/fonts.conf";

    if (!fs_create_dir(cacheDir) || !fs_create_dir(ffmpegKitDir) ||
        !fs_create_dir(tempConfigurationDirectory)) {
        return;
    }
    std::cout << "Created temporary font conf directory: TRUE." << std::endl;

    if (fs_exists(fontConfigurationFile, true, false)) {
        bool fontConfigurationDeleted =
            std::remove(fontConfigurationFile.c_str());
        std::cout << "Deleted old temporary font configuration: "
                  << (fontConfigurationDeleted == 0 ? "TRUE" : "FALSE") << "."
                  << std::endl;
    }

    /* PROCESS MAPPINGS FIRST */
    std::string fontNameMappingBlock = "";
    for (auto const &pair : fontNameMapping) {
        if ((pair.first.size() > 0) && (pair.second.size() > 0)) {

            fontNameMappingBlock += "    <match target=\"pattern\">\n";
            fontNameMappingBlock +=
                "        <test qual=\"any\" name=\"family\">\n";
            fontNameMappingBlock += "                <string>";
            fontNameMappingBlock += pair.first;
            fontNameMappingBlock += "</string>\n";
            fontNameMappingBlock += "        </test>\n";
            fontNameMappingBlock += "        <edit name=\"family\" "
                                    "mode=\"assign\" binding=\"same\">\n";
            fontNameMappingBlock += "            <string>";
            fontNameMappingBlock += pair.second;
            fontNameMappingBlock += "</string>\n";
            fontNameMappingBlock += "        </edit>\n";
            fontNameMappingBlock += "    </match>\n";

            validFontNameMappingCount++;
        }
    }

    std::string fontConfiguration;
    fontConfiguration += "<?xml version=\"1.0\"?>\n";
    fontConfiguration += "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n";
    fontConfiguration += "<fontconfig>\n";
    fontConfiguration += "    <dir prefix=\"cwd\">.</dir>\n";

    for (const auto &fontDirectoryPath : fontDirectoryList) {
        fontConfiguration += "    <dir>";
        fontConfiguration += fontDirectoryPath;
        fontConfiguration += "</dir>\n";
    }
    fontConfiguration += fontNameMappingBlock;
    fontConfiguration += "</fontconfig>\n";

    std::ofstream fontConfigurationStream(fontConfigurationFile,
                                          std::ios::out | std::ios::trunc);
    if (fontConfigurationStream) {
        fontConfigurationStream << fontConfiguration;
    }
    if (fontConfigurationStream.bad()) {
        std::cout << "Failed to set font directory. Error received while "
                     "saving font configuration: "
                  << fontConfigurationStream.rdbuf() << "." << std::endl;
    }
    fontConfigurationStream.close();

    std::cout << "Saved new temporary font configuration with "
              << validFontNameMappingCount << " font name mappings."
              << std::endl;

    ffmpegkit::FFmpegKitConfig::setFontconfigConfigurationPath(
        tempConfigurationDirectory.c_str());

    for (const auto &fontDirectoryPath : fontDirectoryList) {
        std::cout << "Font directory " << fontDirectoryPath
                  << " registered successfully." << std::endl;
    }
}

std::shared_ptr<std::string>
ffmpegkit::FFmpegKitConfig::registerNewFFmpegPipe() {
    const char *parentDirectory = std::getenv("HOME");
    if (parentDirectory == NULL) {
        parentDirectory = std::getenv("TMPDIR");
        if (parentDirectory == NULL) {
            parentDirectory = ".";
        }
    }

    // PIPES ARE CREATED UNDER THE PIPES DIRECTORY
    std::string cacheDir = std::string(parentDirectory) + "/.cache";
    std::string ffmpegKitDir = cacheDir + "/ffmpegkit";
    std::string pipesDir = ffmpegKitDir + "/pipes";

    if (!fs_create_dir(cacheDir) || !fs_create_dir(ffmpegKitDir) ||
        !fs_create_dir(pipesDir)) {
        return nullptr;
    }

    std::shared_ptr<std::string> newFFmpegPipePath =
        std::make_shared<std::string>(pipesDir + "/" +
                                      FFmpegKitNamedPipePrefix +
                                      std::to_string(pipeIndexGenerator++));

    // FIRST CLOSE OLD PIPES WITH THE SAME NAME
    ffmpegkit::FFmpegKitConfig::closeFFmpegPipe(newFFmpegPipePath->c_str());

    int rc = mkfifo(newFFmpegPipePath->c_str(), S_IRWXU | S_IRWXG | S_IROTH);
    if (rc == 0) {
        return newFFmpegPipePath;
    } else {
        std::cout << "Failed to register new FFmpeg pipe " << newFFmpegPipePath
                  << ". Operation failed with rc=" << rc << "." << std::endl;
        return nullptr;
    }
}

void ffmpegkit::FFmpegKitConfig::closeFFmpegPipe(
    const std::string &ffmpegPipePath) {
    std::remove(ffmpegPipePath.c_str());
}

long ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(
    const std::vector<uint8_t> &data) {
    return ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(
        data.empty() ? NULL : data.data(), data.size());
}

long ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(
    const uint8_t *data, const size_t size) {
    int64_t id;
    FFKitMemoryResource *resource;

    if (data == NULL && size > 0) {
        return 0;
    }
    if (size > (size_t)ffkit_max_alloc_size()) {
        return 0;
    }

    resource = (FFKitMemoryResource *)av_mallocz(sizeof(FFKitMemoryResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = (uint8_t *)av_malloc(size > 0 ? size : 1);
    if (resource->data == NULL) {
        av_free(resource);
        return 0;
    }

    if (size > 0) {
        memcpy(resource->data, data, size);
    }

    id = ffkit_next_resource_id();
    resource->id = id;
    resource->type = FFKIT_RESOURCE_INPUT;
    resource->size = (int64_t)size;
    resource->capacity = (int64_t)size;
    resource->maxCapacity = (int64_t)size;
    resource->ownsData = 1;
    pthread_mutex_init(&resource->mutex, NULL);
    ffkit_memory_add(resource);

    return (long)id;
}

long ffmpegkit::FFmpegKitConfig::registerFFmpegKitOutputBuffer(
    const long initialCapacity, const long maxCapacity) {
    int64_t id;
    FFKitMemoryResource *resource;
    int64_t capacity =
        initialCapacity > 0 ? initialCapacity : FFKIT_DEFAULT_OUTPUT_CAPACITY;
    int64_t maximumCapacity =
        maxCapacity > 0 ? maxCapacity : ffkit_max_alloc_size();

    if (initialCapacity < 0 || maxCapacity < 0 ||
        capacity > maximumCapacity || capacity > ffkit_max_alloc_size() ||
        maximumCapacity > ffkit_max_alloc_size()) {
        return 0;
    }

    resource = (FFKitMemoryResource *)av_mallocz(sizeof(FFKitMemoryResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = (uint8_t *)av_malloc(capacity);
    if (resource->data == NULL) {
        av_free(resource);
        return 0;
    }

    id = ffkit_next_resource_id();
    resource->id = id;
    resource->type = FFKIT_RESOURCE_OUTPUT;
    resource->capacity = capacity;
    resource->maxCapacity = maximumCapacity;
    resource->ownsData = 1;
    pthread_mutex_init(&resource->mutex, NULL);
    ffkit_memory_add(resource);

    return (long)id;
}

long ffmpegkit::FFmpegKitConfig::getFFmpegKitBufferSize(const long bufferId) {
    FFKitMemoryResource *resource;
    int64_t size = -1;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(bufferId);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        size = resource->size;
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    return (long)size;
}

std::shared_ptr<std::vector<uint8_t>>
ffmpegkit::FFmpegKitConfig::getFFmpegKitOutputBuffer(const long bufferId) {
    FFKitMemoryResource *resource;
    std::shared_ptr<std::vector<uint8_t>> result = nullptr;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(bufferId);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        if (resource->type == FFKIT_RESOURCE_OUTPUT &&
            resource->size >= 0 &&
            (uint64_t)resource->size <=
                (uint64_t)std::numeric_limits<size_t>::max()) {
            result = std::make_shared<std::vector<uint8_t>>(
                resource->data, resource->data + resource->size);
        }
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    return result;
}

void ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitBuffer(
    const long bufferId) {
    FFKitMemoryResource *current;
    FFKitMemoryResource *previous = NULL;
    FFKitMemoryResource *resource = NULL;
    int freeResource = 0;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    current = ffkitMemoryResources;
    while (current != NULL) {
        if (current->id == bufferId) {
            resource = current;
            if (previous == NULL) {
                ffkitMemoryResources = current->next;
            } else {
                previous->next = current->next;
            }
            break;
        }
        previous = current;
        current = current->next;
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->unregistered = 1;
        freeResource = resource->openCount == 0;
        pthread_mutex_unlock(&resource->mutex);

        if (freeResource) {
            ffkit_memory_free(resource);
        }
    }
}

long ffmpegkit::FFmpegKitConfig::registerFFmpegKitStream(const long capacity,
                                                         const int type) {
    int64_t id;
    FFKitStreamResource *resource;
    int64_t streamCapacity =
        capacity > 0 ? capacity : FFKIT_DEFAULT_STREAM_CAPACITY;

    if (capacity < 0 || streamCapacity > ffkit_max_alloc_size() ||
        (type != FFKIT_RESOURCE_INPUT && type != FFKIT_RESOURCE_OUTPUT)) {
        return 0;
    }

    resource = (FFKitStreamResource *)av_mallocz(sizeof(FFKitStreamResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = (uint8_t *)av_malloc(streamCapacity);
    if (resource->data == NULL) {
        av_free(resource);
        return 0;
    }

    id = ffkit_next_resource_id();
    resource->id = id;
    resource->type = type;
    resource->capacity = streamCapacity;
    pthread_mutex_init(&resource->mutex, NULL);
    pthread_cond_init(&resource->canRead, NULL);
    pthread_cond_init(&resource->canWrite, NULL);
    ffkit_stream_add(resource);

    return (long)id;
}

int ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
    const long streamId, const std::vector<uint8_t> &data, const size_t offset,
    const size_t length, const int timeoutMs) {
    if (offset > data.size() || length > (data.size() - offset)) {
        return AVERROR(EINVAL);
    }

    return ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
        streamId, data.empty() ? NULL : data.data() + offset, length,
        timeoutMs);
}

int ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
    const long streamId, const uint8_t *data, const size_t length,
    const int timeoutMs) {
    FFKitStreamResource *resource;
    int ret = AVERROR(ENOENT);
    int freeResource = 0;

    if ((data == NULL && length > 0) ||
        length > (size_t)std::numeric_limits<int>::max()) {
        return AVERROR(EINVAL);
    }

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(streamId);
    if (resource != NULL && resource->type == FFKIT_RESOURCE_INPUT) {
        pthread_mutex_lock(&resource->mutex);
        if (!resource->unregistered) {
            resource->openCount++;
            ret = 0;
        }
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);

    if (ret == 0) {
        ret = ffkit_stream_write_bytes(resource, data, (int)length, timeoutMs);

        pthread_mutex_lock(&resource->mutex);
        if (resource->openCount > 0) {
            resource->openCount--;
        }
        freeResource = resource->unregistered && resource->openCount == 0;
        pthread_mutex_unlock(&resource->mutex);

        if (freeResource) {
            ffkit_stream_free(resource);
        }
    }

    return ret;
}

std::shared_ptr<std::vector<uint8_t>>
ffmpegkit::FFmpegKitConfig::readFFmpegKitStream(const long streamId,
                                                const int maxBytes,
                                                const int timeoutMs) {
    FFKitStreamResource *resource;
    uint8_t *buffer;
    int ret = AVERROR(ENOENT);
    int timedOut = 0;
    int eof = 0;
    int freeResource = 0;
    std::shared_ptr<std::vector<uint8_t>> result = nullptr;

    if (maxBytes < 0) {
        return nullptr;
    }

    buffer = (uint8_t *)av_malloc(maxBytes > 0 ? maxBytes : 1);
    if (buffer == NULL) {
        return nullptr;
    }

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(streamId);
    if (resource != NULL && resource->type == FFKIT_RESOURCE_OUTPUT) {
        pthread_mutex_lock(&resource->mutex);
        if (!resource->unregistered) {
            resource->openCount++;
            ret = 0;
        }
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);

    if (ret == 0) {
        ret = ffkit_stream_read_bytes(resource, buffer, maxBytes, timeoutMs,
                                      &timedOut, &eof);

        pthread_mutex_lock(&resource->mutex);
        if (resource->openCount > 0) {
            resource->openCount--;
        }
        freeResource = resource->unregistered && resource->openCount == 0;
        pthread_mutex_unlock(&resource->mutex);

        if (freeResource) {
            ffkit_stream_free(resource);
        }
    }

    if (ret > 0 || eof) {
        result = std::make_shared<std::vector<uint8_t>>(buffer, buffer + ret);
    } else if (timedOut) {
        result = nullptr;
    }

    av_free(buffer);
    return result;
}

void ffmpegkit::FFmpegKitConfig::closeFFmpegKitStreamInput(
    const long streamId) {
    FFKitStreamResource *resource;

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(streamId);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->writeClosed = 1;
        pthread_cond_broadcast(&resource->canRead);
        pthread_cond_broadcast(&resource->canWrite);
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);
}

void ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(
    const long streamId) {
    FFKitStreamResource *current;
    FFKitStreamResource *previous = NULL;
    FFKitStreamResource *resource = NULL;
    int freeResource = 0;

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    current = ffkitStreamResources;
    while (current != NULL) {
        if (current->id == streamId) {
            resource = current;
            if (previous == NULL) {
                ffkitStreamResources = current->next;
            } else {
                previous->next = current->next;
            }
            break;
        }
        previous = current;
        current = current->next;
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);

    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->unregistered = 1;
        resource->closed = 1;
        resource->writeClosed = 1;
        freeResource = resource->openCount == 0;
        pthread_cond_broadcast(&resource->canRead);
        pthread_cond_broadcast(&resource->canWrite);
        pthread_mutex_unlock(&resource->mutex);

        if (freeResource) {
            ffkit_stream_free(resource);
        }
    }
}

std::string ffmpegkit::FFmpegKitConfig::getFFmpegVersion() {
    return FFMPEG_VERSION;
}

std::string ffmpegkit::FFmpegKitConfig::getVersion() {
    if (FFmpegKitConfig::isLTSBuild()) {
        return std::string("").append(FFmpegKitVersion).append("-lts");
    } else {
        return FFmpegKitVersion;
    }
}

bool ffmpegkit::FFmpegKitConfig::isLTSBuild() {
#if defined(FFMPEG_KIT_LTS)
    return true;
#else
    return false;
#endif
}

std::string ffmpegkit::FFmpegKitConfig::getBuildDate() {
    char buildDate[10];
    sprintf(buildDate, "%d", FFMPEG_KIT_BUILD_DATE);
    return std::string(buildDate);
}

int ffmpegkit::FFmpegKitConfig::setEnvironmentVariable(
    const std::string &variableName, const std::string &variableValue) {
    return setenv(variableName.c_str(), variableValue.c_str(), true);
}

void ffmpegkit::FFmpegKitConfig::ignoreSignal(const ffmpegkit::Signal signal) {
    if (signal == ffmpegkit::SignalQuit) {
        handleSIGQUIT = 0;
    } else if (signal == ffmpegkit::SignalInt) {
        handleSIGINT = 0;
    } else if (signal == ffmpegkit::SignalTerm) {
        handleSIGTERM = 0;
    } else if (signal == ffmpegkit::SignalXcpu) {
        handleSIGXCPU = 0;
    } else if (signal == ffmpegkit::SignalPipe) {
        handleSIGPIPE = 0;
    }
}

void ffmpegkit::FFmpegKitConfig::ffmpegExecute(
    const std::shared_ptr<ffmpegkit::FFmpegSession> ffmpegSession) {
    ffmpegSession->startRunning();

    try {
        int returnCode = executeFFmpeg(ffmpegSession->getSessionId(),
                                       ffmpegSession->getArguments());
        ffmpegSession->complete(
            std::make_shared<ffmpegkit::ReturnCode>(returnCode));
    } catch (const std::exception &exception) {
        ffmpegSession->fail(exception.what());
        std::cout << "FFmpeg execute failed: "
                  << ffmpegkit::FFmpegKitConfig::argumentsToString(
                         ffmpegSession->getArguments())
                  << "." << exception.what() << std::endl;
    }
}

void ffmpegkit::FFmpegKitConfig::ffprobeExecute(
    const std::shared_ptr<ffmpegkit::FFprobeSession> ffprobeSession) {
    ffprobeSession->startRunning();

    try {
        int returnCode = executeFFprobe(ffprobeSession->getSessionId(),
                                        ffprobeSession->getArguments());
        ffprobeSession->complete(
            std::make_shared<ffmpegkit::ReturnCode>(returnCode));
    } catch (const std::exception &exception) {
        ffprobeSession->fail(exception.what());
        std::cout << "FFprobe execute failed: "
                  << ffmpegkit::FFmpegKitConfig::argumentsToString(
                         ffprobeSession->getArguments())
                  << "." << exception.what() << std::endl;
    }
}

void ffmpegkit::FFmpegKitConfig::getMediaInformationExecute(
    const std::shared_ptr<ffmpegkit::MediaInformationSession>
        mediaInformationSession,
    const int waitTimeout) {
    mediaInformationSession->startRunning();

    try {
        int returnCodeValue =
            executeFFprobe(mediaInformationSession->getSessionId(),
                           mediaInformationSession->getArguments());
        auto returnCode =
            std::make_shared<ffmpegkit::ReturnCode>(returnCodeValue);
        mediaInformationSession->complete(returnCode);
        if (returnCode->isValueSuccess()) {
            auto allLogs =
                mediaInformationSession->getAllLogsWithTimeout(waitTimeout);
            std::string ffprobeJsonOutput;
            std::for_each(allLogs->cbegin(), allLogs->cend(),
                          [&](std::shared_ptr<ffmpegkit::Log> log) {
                              if (log->getLevel() == LevelAVLogStdErr) {
                                  ffprobeJsonOutput.append(log->getMessage());
                              }
                          });
            auto mediaInformation =
                ffmpegkit::MediaInformationJsonParser::fromWithError(
                    ffprobeJsonOutput.c_str());
            mediaInformationSession->setMediaInformation(mediaInformation);
        }
    } catch (const std::exception &exception) {
        mediaInformationSession->fail(exception.what());
        std::cout << "Get media information execute failed: "
                  << ffmpegkit::FFmpegKitConfig::argumentsToString(
                         mediaInformationSession->getArguments())
                  << "." << exception.what() << std::endl;
    }
}

void ffmpegkit::FFmpegKitConfig::asyncFFmpegExecute(
    const std::shared_ptr<ffmpegkit::FFmpegSession> ffmpegSession) {
    auto thread = std::thread([ffmpegSession]() {
        ffmpegkit::FFmpegKitConfig::ffmpegExecute(ffmpegSession);

        ffmpegkit::FFmpegSessionCompleteCallback completeCallback =
            ffmpegSession->getCompleteCallback();
        if (completeCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback(ffmpegSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside session complete callback. "
                    << exception.what() << std::endl;
            }
        }

        ffmpegkit::FFmpegSessionCompleteCallback
            globalFFmpegSessionCompleteCallback =
                ffmpegkit::FFmpegKitConfig::getFFmpegSessionCompleteCallback();
        if (globalFFmpegSessionCompleteCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                globalFFmpegSessionCompleteCallback(ffmpegSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside global complete callback. "
                    << exception.what() << std::endl;
            }
        }
    });

    thread.detach();
}

void ffmpegkit::FFmpegKitConfig::asyncFFprobeExecute(
    const std::shared_ptr<ffmpegkit::FFprobeSession> ffprobeSession) {
    auto thread = std::thread([ffprobeSession]() {
        ffmpegkit::FFmpegKitConfig::ffprobeExecute(ffprobeSession);

        ffmpegkit::FFprobeSessionCompleteCallback completeCallback =
            ffprobeSession->getCompleteCallback();
        if (completeCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback(ffprobeSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside session complete callback. "
                    << exception.what() << std::endl;
            }
        }

        ffmpegkit::FFprobeSessionCompleteCallback
            globalFFprobeSessionCompleteCallback =
                ffmpegkit::FFmpegKitConfig::getFFprobeSessionCompleteCallback();
        if (globalFFprobeSessionCompleteCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                globalFFprobeSessionCompleteCallback(ffprobeSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside global complete callback. "
                    << exception.what() << std::endl;
            }
        }
    });

    thread.detach();
}

void ffmpegkit::FFmpegKitConfig::asyncGetMediaInformationExecute(
    const std::shared_ptr<ffmpegkit::MediaInformationSession>
        mediaInformationSession,
    const int waitTimeout) {
    auto thread = std::thread([mediaInformationSession, waitTimeout]() {
        ffmpegkit::FFmpegKitConfig::getMediaInformationExecute(
            mediaInformationSession, waitTimeout);

        ffmpegkit::MediaInformationSessionCompleteCallback completeCallback =
            mediaInformationSession->getCompleteCallback();
        if (completeCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback(mediaInformationSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside session complete callback. "
                    << exception.what() << std::endl;
            }
        }

        ffmpegkit::MediaInformationSessionCompleteCallback
            globalMediaInformationSessionCompleteCallback = ffmpegkit::
                FFmpegKitConfig::getMediaInformationSessionCompleteCallback();
        if (globalMediaInformationSessionCompleteCallback != nullptr) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                globalMediaInformationSessionCompleteCallback(
                    mediaInformationSession);
            } catch (const std::exception &exception) {
                std::cout
                    << "Exception thrown inside global complete callback. "
                    << exception.what() << std::endl;
            }
        }
    });

    thread.detach();
}

void ffmpegkit::FFmpegKitConfig::enableLogCallback(
    const ffmpegkit::LogCallback callback) {
    logCallback = callback;
}

void ffmpegkit::FFmpegKitConfig::enableStatisticsCallback(
    const ffmpegkit::StatisticsCallback callback) {
    statisticsCallback = callback;
}

void ffmpegkit::FFmpegKitConfig::enableFFmpegSessionCompleteCallback(
    const FFmpegSessionCompleteCallback completeCallback) {
    ffmpegSessionCompleteCallback = completeCallback;
}

ffmpegkit::FFmpegSessionCompleteCallback
ffmpegkit::FFmpegKitConfig::getFFmpegSessionCompleteCallback() {
    return ffmpegSessionCompleteCallback;
}

void ffmpegkit::FFmpegKitConfig::enableFFprobeSessionCompleteCallback(
    const FFprobeSessionCompleteCallback completeCallback) {
    ffprobeSessionCompleteCallback = completeCallback;
}

ffmpegkit::FFprobeSessionCompleteCallback
ffmpegkit::FFmpegKitConfig::getFFprobeSessionCompleteCallback() {
    return ffprobeSessionCompleteCallback;
}

void ffmpegkit::FFmpegKitConfig::enableMediaInformationSessionCompleteCallback(
    const MediaInformationSessionCompleteCallback completeCallback) {
    mediaInformationSessionCompleteCallback = completeCallback;
}

ffmpegkit::MediaInformationSessionCompleteCallback
ffmpegkit::FFmpegKitConfig::getMediaInformationSessionCompleteCallback() {
    return mediaInformationSessionCompleteCallback;
}

ffmpegkit::Level ffmpegkit::FFmpegKitConfig::getLogLevel() {
    return static_cast<ffmpegkit::Level>(configuredLogLevel);
}

void ffmpegkit::FFmpegKitConfig::setLogLevel(const ffmpegkit::Level level) {
    configuredLogLevel = level;
}

std::string
ffmpegkit::FFmpegKitConfig::logLevelToString(const ffmpegkit::Level level) {
    switch (level) {
    case ffmpegkit::LevelAVLogStdErr:
        return "STDERR";
    case ffmpegkit::LevelAVLogTrace:
        return "TRACE";
    case ffmpegkit::LevelAVLogDebug:
        return "DEBUG";
    case ffmpegkit::LevelAVLogVerbose:
        return "VERBOSE";
    case ffmpegkit::LevelAVLogInfo:
        return "INFO";
    case ffmpegkit::LevelAVLogWarning:
        return "WARNING";
    case ffmpegkit::LevelAVLogError:
        return "ERROR";
    case ffmpegkit::LevelAVLogFatal:
        return "FATAL";
    case ffmpegkit::LevelAVLogPanic:
        return "PANIC";
    case ffmpegkit::LevelAVLogQuiet:
        return "QUIET";
    default:
        return "";
    }
}

int ffmpegkit::FFmpegKitConfig::getSessionHistorySize() {
    return sessionHistorySize;
}

void ffmpegkit::FFmpegKitConfig::setSessionHistorySize(
    const int newSessionHistorySize) {
    if (newSessionHistorySize >= SESSION_MAP_SIZE) {

        /*
         * THERE IS A HARD LIMIT ON THE NATIVE SIDE. HISTORY SIZE MUST BE
         * SMALLER THAN SESSION_MAP_SIZE
         */
        throw std::runtime_error(
            "Session history size must not exceed the hard limit!");
    } else if (newSessionHistorySize > 0) {
        std::list<long> deletedSessionIds;
        std::unique_lock<std::recursive_mutex> lock(sessionMutex,
                                                    std::defer_lock);
        lock.lock();

        sessionHistorySize = newSessionHistorySize;
        deletedSessionIds = deleteExpiredSessionsLocked();

        lock.unlock();

        notifySessionsDeleted(deletedSessionIds);
    }
}

std::shared_ptr<ffmpegkit::Session>
ffmpegkit::FFmpegKitConfig::getSession(const long sessionId) {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    lock.lock();

    auto session = sessionHistoryMap.find(sessionId);
    if (session != sessionHistoryMap.end()) {
        return session->second;
    } else {
        return nullptr;
    }
}

void ffmpegkit::FFmpegKitConfig::deleteSession(const long sessionId) {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    bool deleted = false;

    lock.lock();

    deleted = sessionHistoryMap.erase(sessionId) > 0;
    if (deleted) {
        auto it = std::remove_if(
            sessionHistoryList.begin(), sessionHistoryList.end(),
            [sessionId](std::shared_ptr<ffmpegkit::Session> session) {
                return session->getSessionId() == sessionId;
            });
        sessionHistoryList.erase(it, sessionHistoryList.end());
    }

    lock.unlock();

    if (deleted) {
        notifySessionDeleted(sessionId);
    }
}

void ffmpegkit::FFmpegKitConfig::addSessionDeleteListener(
    const std::shared_ptr<ffmpegkit::SessionDeleteListener> listener) {
    if (listener == nullptr) {
        return;
    }

    std::unique_lock<std::recursive_mutex> listenerLock(
        sessionDeleteListenerMutex, std::defer_lock);
    listenerLock.lock();

    for (auto it = sessionDeleteListeners.begin();
         it != sessionDeleteListeners.end();) {
        auto existingListener = it->lock();
        if (existingListener == nullptr || existingListener == listener) {
            it = sessionDeleteListeners.erase(it);
        } else {
            ++it;
        }
    }

    sessionDeleteListeners.push_back(listener);

    listenerLock.unlock();
}

void ffmpegkit::FFmpegKitConfig::removeSessionDeleteListener(
    const std::shared_ptr<ffmpegkit::SessionDeleteListener> listener) {
    if (listener == nullptr) {
        return;
    }

    std::unique_lock<std::recursive_mutex> listenerLock(
        sessionDeleteListenerMutex, std::defer_lock);
    listenerLock.lock();

    for (auto it = sessionDeleteListeners.begin();
         it != sessionDeleteListeners.end();) {
        auto existingListener = it->lock();
        if (existingListener == nullptr || existingListener == listener) {
            it = sessionDeleteListeners.erase(it);
        } else {
            ++it;
        }
    }

    listenerLock.unlock();
}

std::shared_ptr<ffmpegkit::Session>
ffmpegkit::FFmpegKitConfig::getLastSession() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    lock.lock();

    return sessionHistoryList.front();
}

std::shared_ptr<ffmpegkit::Session>
ffmpegkit::FFmpegKitConfig::getLastCompletedSession() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);

    lock.lock();

    for (auto rit = sessionHistoryList.rbegin();
         rit != sessionHistoryList.rend(); ++rit) {
        auto session = *rit;
        if (session->getState() == SessionStateCompleted) {
            return session;
        }
    }

    return nullptr;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Session>>>
ffmpegkit::FFmpegKitConfig::getSessions() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    lock.lock();

    auto sessionHistoryListCopy =
        std::make_shared<std::list<std::shared_ptr<ffmpegkit::Session>>>(
            sessionHistoryList);

    lock.unlock();

    return sessionHistoryListCopy;
}

void ffmpegkit::FFmpegKitConfig::clearSessions() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    std::list<long> deletedSessionIds;

    lock.lock();

    for (auto it = sessionHistoryList.begin(); it != sessionHistoryList.end();
         ++it) {
        auto session = *it;
        if (session != nullptr) {
            deletedSessionIds.push_back(session->getSessionId());
        }
    }

    sessionHistoryList.clear();
    sessionHistoryMap.clear();

    lock.unlock();

    notifySessionsDeleted(deletedSessionIds);
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::FFmpegSession>>>
ffmpegkit::FFmpegKitConfig::getFFmpegSessions() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    const auto ffmpegSessions = std::make_shared<
        std::list<std::shared_ptr<ffmpegkit::FFmpegSession>>>();

    lock.lock();

    for (auto it = sessionHistoryList.begin(); it != sessionHistoryList.end();
         ++it) {
        auto session = *it;
        if (session->isFFmpeg()) {
            ffmpegSessions->push_back(
                std::static_pointer_cast<ffmpegkit::FFmpegSession>(session));
        }
    }

    lock.unlock();

    return ffmpegSessions;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::FFprobeSession>>>
ffmpegkit::FFmpegKitConfig::getFFprobeSessions() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    const auto ffprobeSessions = std::make_shared<
        std::list<std::shared_ptr<ffmpegkit::FFprobeSession>>>();

    lock.lock();

    for (auto it = sessionHistoryList.begin(); it != sessionHistoryList.end();
         ++it) {
        auto session = *it;
        if (session->isFFprobe()) {
            ffprobeSessions->push_back(
                std::static_pointer_cast<ffmpegkit::FFprobeSession>(session));
        }
    }

    lock.unlock();

    return ffprobeSessions;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::MediaInformationSession>>>
ffmpegkit::FFmpegKitConfig::getMediaInformationSessions() {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    const auto mediaInformationSessions = std::make_shared<
        std::list<std::shared_ptr<ffmpegkit::MediaInformationSession>>>();

    lock.lock();

    for (auto it = sessionHistoryList.begin(); it != sessionHistoryList.end();
         ++it) {
        auto session = *it;
        if (session->isMediaInformation()) {
            mediaInformationSessions->push_back(
                std::static_pointer_cast<ffmpegkit::MediaInformationSession>(
                    session));
        }
    }

    lock.unlock();

    return mediaInformationSessions;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Session>>>
ffmpegkit::FFmpegKitConfig::getSessionsByState(const SessionState state) {
    std::unique_lock<std::recursive_mutex> lock(sessionMutex, std::defer_lock);
    auto sessions =
        std::make_shared<std::list<std::shared_ptr<ffmpegkit::Session>>>();

    lock.lock();

    for (auto it = sessionHistoryList.begin(); it != sessionHistoryList.end();
         ++it) {
        auto session = *it;
        if (session->getState() == state) {
            sessions->push_back(session);
        }
    }

    lock.unlock();

    return sessions;
}

ffmpegkit::LogRedirectionStrategy
ffmpegkit::FFmpegKitConfig::getLogRedirectionStrategy() {
    return globalLogRedirectionStrategy;
}

void ffmpegkit::FFmpegKitConfig::setLogRedirectionStrategy(
    const LogRedirectionStrategy logRedirectionStrategy) {
    globalLogRedirectionStrategy = logRedirectionStrategy;
}

int ffmpegkit::FFmpegKitConfig::messagesInTransmit(const long sessionId) {
    return std::atomic_load(
        &sessionInTransitMessageCountMap[sessionId % SESSION_MAP_SIZE]);
}

std::string
ffmpegkit::FFmpegKitConfig::sessionStateToString(SessionState state) {
    switch (state) {
    case SessionStateCreated:
        return "CREATED";
    case SessionStateRunning:
        return "RUNNING";
    case SessionStateFailed:
        return "FAILED";
    case SessionStateCompleted:
        return "COMPLETED";
    default:
        return "";
    }
}

std::list<std::string>
ffmpegkit::FFmpegKitConfig::parseArguments(const std::string &command) {
    std::list<std::string> argumentList;
    std::string currentArgument;

    bool singleQuoteStarted = false;
    bool doubleQuoteStarted = false;

    for (int i = 0; i < command.size(); i++) {
        char previousChar;
        if (i > 0) {
            previousChar = command[i - 1];
        } else {
            previousChar = 0;
        }
        char currentChar = command[i];

        if (currentChar == ' ') {
            if (singleQuoteStarted || doubleQuoteStarted) {
                currentArgument += currentChar;
            } else if (currentArgument.size() > 0) {
                argumentList.push_back(currentArgument);
                currentArgument = "";
            }
        } else if (currentChar == '\'' &&
                   (previousChar == 0 || previousChar != '\\')) {
            if (singleQuoteStarted) {
                singleQuoteStarted = false;
            } else if (doubleQuoteStarted) {
                currentArgument += currentChar;
            } else {
                singleQuoteStarted = true;
            }
        } else if (currentChar == '\"' &&
                   (previousChar == 0 || previousChar != '\\')) {
            if (doubleQuoteStarted) {
                doubleQuoteStarted = false;
            } else if (singleQuoteStarted) {
                currentArgument += currentChar;
            } else {
                doubleQuoteStarted = true;
            }
        } else {
            currentArgument += currentChar;
        }
    }

    if (currentArgument.size() > 0) {
        argumentList.push_back(currentArgument);
    }

    return argumentList;
}

std::string ffmpegkit::FFmpegKitConfig::argumentsToString(
    std::shared_ptr<std::list<std::string>> arguments) {
    if (arguments == nullptr) {
        return "null";
    }

    std::string string;
    for (auto it = arguments->begin(); it != arguments->end(); ++it) {
        auto argument = *it;
        if (it != arguments->begin()) {
            string += " ";
        }
        string += argument;
    }

    return string;
}
