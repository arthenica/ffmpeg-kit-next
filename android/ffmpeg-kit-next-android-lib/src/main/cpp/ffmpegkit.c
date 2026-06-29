/*
 * Copyright (c) 2018-2021, 2026 Taner Sener
 * Copyright (c) 2024 ARTHENICA LTD
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
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "config.h"
#include "ffmpegkit.h"
#include "ffprobekit.h"
#include "fftools_ffmpeg.h"
#include "libavcodec/jni.h"
#include "libavformat/avio.h"
#include "libavutil/bprint.h"
#include "libavutil/error.h"
#include "libavutil/file.h"
#include "libavutil/mem.h"

#define LogType 1
#define StatisticsType 2

/** Callback data structure */
struct CallbackData {
    int type;       // 1 (log callback) or 2 (statistics callback)
    long sessionId; // session identifier

    int logLevel;     // log level
    AVBPrint logData; // log data

    int statisticsFrameNumber; // statistics frame number
    float statisticsFps;       // statistics fps
    float statisticsQuality;   // statistics quality
    int64_t statisticsSize;    // statistics size
    double statisticsTime;     // statistics time
    double statisticsBitrate;  // statistics bitrate
    double statisticsSpeed;    // statistics speed

    struct CallbackData *next;
};

/** Session control variables */
#define SESSION_MAP_SIZE 1000
static atomic_short sessionMap[SESSION_MAP_SIZE];
static atomic_int sessionInTransitMessageCountMap[SESSION_MAP_SIZE];

/** Redirection control variables */
static pthread_mutex_t lockMutex;
static pthread_mutex_t monitorMutex;
static pthread_cond_t monitorCondition;

pthread_t callbackThread;
int redirectionEnabled;

struct CallbackData *callbackDataHead;
struct CallbackData *callbackDataTail;

/** Global reference to the virtual machine running */
static JavaVM *globalVm;

/** Global reference of Config class in Java */
static jclass configClass;

/** Global reference of log redirection method in Java */
static jmethodID logMethod;

/** Global reference of statistics redirection method in Java */
static jmethodID statisticsMethod;

/** Global reference of safOpen method in Java */
static jmethodID safOpenMethod;

/** Global reference of safClose method in Java */
static jmethodID safCloseMethod;

/** Global reference of String class in Java */
static jclass stringClass;

/** Global reference of String constructor in Java */
static jmethodID stringConstructor;

/** Full name of the Config class */
const char *configClassName = "com/arthenica/ffmpegkit/FFmpegKitConfig";

/** Full name of String class */
const char *stringClassName = "java/lang/String";

/** Fields that control the handling of SIGNALs */
volatile int handleSIGQUIT = 1;
volatile int handleSIGINT = 1;
volatile int handleSIGTERM = 1;
volatile int handleSIGXCPU = 1;
volatile int handleSIGPIPE = 1;

/** Holds the id of the current session */
__thread long globalSessionId = 0;

/** Holds the default log level */
int configuredLogLevel = AV_LOG_INFO;

#define FFKIT_RESOURCE_INPUT 1
#define FFKIT_RESOURCE_OUTPUT 2
#define FFKIT_DEFAULT_OUTPUT_CAPACITY 4096
#define FFKIT_DEFAULT_STREAM_CAPACITY 1048576

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
    jobject directBufferRef;
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
static atomic_llong ffkitNextResourceId = 1;

#ifdef USES_FFMPEG_KIT_PROTOCOLS
typedef int (*ffkit_local_protocol_open_function)(int64_t, int, void **);
typedef int (*ffkit_local_protocol_read_function)(void *, unsigned char *, int);
typedef int (*ffkit_local_protocol_write_function)(void *, const unsigned char *,
                                                   int);
typedef int64_t (*ffkit_local_protocol_seek_function)(void *, int64_t, int);
typedef int (*ffkit_local_protocol_close_function)(void *);

extern void av_set_ffkitmem_functions(
    ffkit_local_protocol_open_function open_function,
    ffkit_local_protocol_read_function read_function,
    ffkit_local_protocol_write_function write_function,
    ffkit_local_protocol_seek_function seek_function,
    ffkit_local_protocol_close_function close_function);

extern void av_set_ffkitstream_functions(
    ffkit_local_protocol_open_function open_function,
    ffkit_local_protocol_read_function read_function,
    ffkit_local_protocol_write_function write_function,
    ffkit_local_protocol_seek_function seek_function,
    ffkit_local_protocol_close_function close_function);
#endif

/** Prototypes of native functions defined by Config class. */
JNINativeMethod configMethods[] = {
    {"enableNativeRedirection", "()V",
     (void *)
         Java_com_arthenica_ffmpegkit_FFmpegKitConfig_enableNativeRedirection},
    {"disableNativeRedirection", "()V",
     (void *)
         Java_com_arthenica_ffmpegkit_FFmpegKitConfig_disableNativeRedirection},
    {"setNativeLogLevel", "(I)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_setNativeLogLevel},
    {"getNativeLogLevel", "()I",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeLogLevel},
    {"getNativeFFmpegVersion", "()Ljava/lang/String;",
     (void *)
         Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegVersion},
    {"getNativeVersion", "()Ljava/lang/String;",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeVersion},
    {"getNativePackageName", "()Ljava/lang/String;",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativePackageName},
    {"nativeFFmpegExecute", "(J[Ljava/lang/String;)I",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegExecute},
    {"nativeFFmpegCancel", "(J)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegCancel},
    {"nativeFFprobeExecute", "(J[Ljava/lang/String;)I",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFprobeExecute},
    {"registerNewNativeFFmpegPipe", "(Ljava/lang/String;)I",
     (void *)
         Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNewNativeFFmpegPipe},
    {"getNativeBuildDate", "()Ljava/lang/String;",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeBuildDate},
    {"setNativeEnvironmentVariable", "(Ljava/lang/String;Ljava/lang/String;)I",
     (void *)
         Java_com_arthenica_ffmpegkit_FFmpegKitConfig_setNativeEnvironmentVariable},
    {"ignoreNativeSignal", "(I)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_ignoreNativeSignal},
    {"messagesInTransmit", "(J)I",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_messagesInTransmit},
    {"registerNativeFFmpegKitInputBuffer", "([B)J",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitInputBuffer},
    {"registerNativeFFmpegKitInputDirectBuffer", "(Ljava/nio/ByteBuffer;I)J",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitInputDirectBuffer},
    {"registerNativeFFmpegKitOutputBuffer", "(JJ)J",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitOutputBuffer},
    {"getNativeFFmpegKitBufferSize", "(J)J",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitBufferSize},
    {"getNativeFFmpegKitOutputBuffer", "(J)[B",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitOutputBuffer},
    {"getNativeFFmpegKitOutputBufferDirect", "(J)Ljava/nio/ByteBuffer;",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitOutputBufferDirect},
    {"unregisterNativeFFmpegKitBuffer", "(J)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_unregisterNativeFFmpegKitBuffer},
    {"registerNativeFFmpegKitStream", "(JI)J",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitStream},
    {"nativeFFmpegKitStreamWrite", "(J[BIII)I",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamWrite},
    {"nativeFFmpegKitStreamRead", "(JII)[B",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamRead},
    {"nativeFFmpegKitStreamCloseInput", "(J)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamCloseInput},
    {"unregisterNativeFFmpegKitStream", "(J)V",
     (void *)Java_com_arthenica_ffmpegkit_FFmpegKitConfig_unregisterNativeFFmpegKitStream}};

/** Forward declaration for function defined in fftools_ffmpeg.c */
int ffmpeg_execute(int argc, char **argv);

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

static void avutil_log_sanitize(uint8_t *line) {
    while (*line) {
        if (*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line = '?';
        line++;
    }
}

void mutexInit() {
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);

    pthread_mutex_init(&lockMutex, &attributes);
    pthread_mutexattr_destroy(&attributes);
}

void monitorInit() {
    pthread_mutexattr_t attributes;
    pthread_mutexattr_init(&attributes);
    pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE_NP);

    pthread_condattr_t cattributes;
    pthread_condattr_init(&cattributes);
    pthread_condattr_setpshared(&cattributes, PTHREAD_PROCESS_PRIVATE);

    pthread_mutex_init(&monitorMutex, &attributes);
    pthread_mutexattr_destroy(&attributes);

    pthread_cond_init(&monitorCondition, &cattributes);
    pthread_condattr_destroy(&cattributes);
}

void mutexUnInit() { pthread_mutex_destroy(&lockMutex); }

void monitorUnInit() {
    pthread_mutex_destroy(&monitorMutex);
    pthread_cond_destroy(&monitorCondition);
}

void mutexLock() { pthread_mutex_lock(&lockMutex); }

void mutexUnlock() { pthread_mutex_unlock(&lockMutex); }

void monitorWait(int milliSeconds) {
    struct timeval tp;
    struct timespec ts;
    int rc;

    rc = gettimeofday(&tp, NULL);
    if (rc) {
        return;
    }

    ts.tv_sec = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += milliSeconds / 1000;
    ts.tv_nsec += (milliSeconds % 1000) * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;

    pthread_mutex_lock(&monitorMutex);
    pthread_cond_timedwait(&monitorCondition, &monitorMutex, &ts);
    pthread_mutex_unlock(&monitorMutex);
}

void monitorNotify() {
    pthread_mutex_lock(&monitorMutex);
    pthread_cond_signal(&monitorCondition);
    pthread_mutex_unlock(&monitorMutex);
}

/**
 * Adds log data to the end of callback data list.
 *
 * @param level log level
 * @param data log data
 */
void logCallbackDataAdd(int level, AVBPrint *data) {

    // CREATE DATA STRUCT FIRST
    struct CallbackData *newData =
        (struct CallbackData *)av_malloc(sizeof(struct CallbackData));
    newData->type = LogType;
    newData->sessionId = globalSessionId;
    newData->logLevel = level;
    av_bprint_init(&newData->logData, 0, AV_BPRINT_SIZE_UNLIMITED);
    av_bprintf(&newData->logData, "%s", data->str);
    newData->next = NULL;

    mutexLock();

    // INSERT IT TO THE END OF QUEUE
    if (callbackDataTail == NULL) {
        callbackDataTail = newData;

        if (callbackDataHead != NULL) {
            LOGE("Dangling callback data head detected. This can cause memory "
                 "leak.");
        } else {
            callbackDataHead = newData;
        }
    } else {
        struct CallbackData *oldTail = callbackDataTail;
        oldTail->next = newData;

        callbackDataTail = newData;
    }

    mutexUnlock();

    monitorNotify();

    atomic_fetch_add(
        &sessionInTransitMessageCountMap[globalSessionId % SESSION_MAP_SIZE],
        1);
}

/**
 * Adds statistics data to the end of callback data list.
 */
void statisticsCallbackDataAdd(int frameNumber, float fps, float quality,
                               int64_t size, double time, double bitrate,
                               double speed) {

    // CREATE DATA STRUCT FIRST
    struct CallbackData *newData =
        (struct CallbackData *)av_malloc(sizeof(struct CallbackData));
    newData->type = StatisticsType;
    newData->sessionId = globalSessionId;
    newData->statisticsFrameNumber = frameNumber;
    newData->statisticsFps = fps;
    newData->statisticsQuality = quality;
    newData->statisticsSize = size;
    newData->statisticsTime = time;
    newData->statisticsBitrate = bitrate;
    newData->statisticsSpeed = speed;

    newData->next = NULL;

    mutexLock();

    // INSERT IT TO THE END OF QUEUE
    if (callbackDataTail == NULL) {
        callbackDataTail = newData;

        if (callbackDataHead != NULL) {
            LOGE("Dangling callback data head detected. This can cause memory "
                 "leak.");
        } else {
            callbackDataHead = newData;
        }
    } else {
        struct CallbackData *oldTail = callbackDataTail;
        oldTail->next = newData;

        callbackDataTail = newData;
    }

    mutexUnlock();

    monitorNotify();

    atomic_fetch_add(
        &sessionInTransitMessageCountMap[globalSessionId % SESSION_MAP_SIZE],
        1);
}

/**
 * Adds a session id to the session map.
 *
 * @param id session id
 */
void addSession(long id) {
    atomic_store(&sessionMap[id % SESSION_MAP_SIZE], 1);
}

/**
 * Removes head of callback data list.
 */
struct CallbackData *callbackDataRemove() {
    struct CallbackData *currentData;

    mutexLock();

    if (callbackDataHead == NULL) {
        currentData = NULL;
    } else {
        currentData = callbackDataHead;

        struct CallbackData *nextHead = currentData->next;
        if (nextHead == NULL) {
            if (callbackDataHead != callbackDataTail) {
                LOGE("Head and tail callback data pointers do not match for "
                     "single callback data element. This can cause memory "
                     "leak.");
            } else {
                callbackDataTail = NULL;
            }
            callbackDataHead = NULL;

        } else {
            callbackDataHead = nextHead;
        }
    }

    mutexUnlock();

    return currentData;
}

/**
 * Removes a session id from the session map.
 *
 * @param id session id
 */
void removeSession(long id) {
    atomic_store(&sessionMap[id % SESSION_MAP_SIZE], 0);
}

/**
 * Adds a cancel session request to the session map.
 *
 * @param id session id
 */
void cancelSession(long id) {
    atomic_store(&sessionMap[id % SESSION_MAP_SIZE], 2);
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
        atomic_compare_exchange_strong(&sessionMap[i], &running, 2);
    }
}

/**
 * Checks whether a cancel request for the given session id exists in the
 * session map.
 *
 * @param id session id
 * @return 1 if exists, false otherwise
 */
int cancelRequested(long id) {
    if (atomic_load(&sessionMap[id % SESSION_MAP_SIZE]) == 2) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Resets the number of messages in transmit for this session.
 *
 * @param id session id
 */
void resetMessagesInTransmit(long id) {
    atomic_store(&sessionInTransitMessageCountMap[id % SESSION_MAP_SIZE], 0);
}

/**
 * Callback function for FFmpeg logs.
 *
 * @param ptr pointer to AVClass struct
 * @param level log level
 * @param format format string
 * @param vargs arguments
 */
void ffmpegkit_log_callback_function(void *ptr, int level, const char *format,
                                     va_list vargs) {
    AVBPrint fullLine;
    AVBPrint part[4];
    int print_prefix = 1;

    if (level >= 0) {
        level &= 0xff;
    }
    int activeLogLevel = av_log_get_level();

    // AV_LOG_STDERR logs are always redirected
    if ((activeLogLevel == AV_LOG_QUIET && level != AV_LOG_STDERR) ||
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

/**
 * Forwards callback messages to Java classes.
 */
void *callbackThreadFunction() {
    JNIEnv *env;
    jint getEnvRc =
        (*globalVm)->GetEnv(globalVm, (void **)&env, JNI_VERSION_1_6);
    if (getEnvRc != JNI_OK) {
        if (getEnvRc != JNI_EDETACHED) {
            LOGE("Callback thread failed to GetEnv for class %s with rc %d.\n",
                 configClassName, getEnvRc);
            return NULL;
        }

        if ((*globalVm)->AttachCurrentThread(globalVm, &env, NULL) != 0) {
            LOGE(
                "Callback thread failed to AttachCurrentThread for class %s.\n",
                configClassName);
            return NULL;
        }
    }

    LOGD("Async callback block started.\n");

    while (redirectionEnabled) {

        struct CallbackData *callbackData = callbackDataRemove();
        if (callbackData != NULL) {
            if (callbackData->type == LogType) {

                // LOG CALLBACK

                int size = callbackData->logData.len;

                jbyteArray byteArray =
                    (jbyteArray)(*env)->NewByteArray(env, size);
                (*env)->SetByteArrayRegion(env, byteArray, 0, size,
                                           callbackData->logData.str);
                (*env)->CallStaticVoidMethod(env, configClass, logMethod,
                                             (jlong)callbackData->sessionId,
                                             callbackData->logLevel, byteArray);
                (*env)->DeleteLocalRef(env, byteArray);

                // CLEAN LOG DATA
                av_bprint_finalize(&callbackData->logData, NULL);

            } else {

                // STATISTICS CALLBACK

                (*env)->CallStaticVoidMethod(
                    env, configClass, statisticsMethod,
                    (jlong)callbackData->sessionId,
                    callbackData->statisticsFrameNumber,
                    callbackData->statisticsFps,
                    callbackData->statisticsQuality,
                    callbackData->statisticsSize, callbackData->statisticsTime,
                    callbackData->statisticsBitrate,
                    callbackData->statisticsSpeed);
            }

            atomic_fetch_sub(
                &sessionInTransitMessageCountMap[callbackData->sessionId %
                                                 SESSION_MAP_SIZE],
                1);

            // CLEAN STRUCT
            callbackData->next = NULL;
            av_free(callbackData);

        } else {
            monitorWait(100);
        }
    }

    (*globalVm)->DetachCurrentThread(globalVm);

    LOGD("Async callback block stopped.\n");

    return NULL;
}

/**
 * Used by saf protocol; If it is called from a Java thread, we don't need
 * attach/detach. However it can be called from other threads as well (as it
 * happens for concat demuxer), in that case we perform attach & detach. Returns
 * file descriptor created for this SAF id or 0 if an error occurs.
 */
int saf_open(int safId) {
    JNIEnv *env = NULL;
    bool attached = false;
    jint getEnvRc =
        (*globalVm)->GetEnv(globalVm, (void **)&env, JNI_VERSION_1_6);
    if (getEnvRc != JNI_OK) {
        if (getEnvRc != JNI_EDETACHED) {
            LOGE("saf_open failed to GetEnv for class %s with rc %d.\n",
                 configClassName, getEnvRc);
            return 0;
        }
        if ((*globalVm)->AttachCurrentThread(globalVm, &env, NULL) != 0) {
            LOGE("saf_open failed to AttachCurrentThread for class %s.\n",
                 configClassName);
            return 0;
        } else {
            attached = true;
        }
    }
    int result =
        (*env)->CallStaticIntMethod(env, configClass, safOpenMethod, safId);
    if (attached)
        (*globalVm)->DetachCurrentThread(globalVm);
    return result;
}

/**
 * Used by saf protocol; If it is called from a Java thread, we don't need
 * attach/detach. However it can be called from other threads as well (as it
 * happens for concat demuxer), in that case we perform attach & detach. Returns
 * 1 if the given file descriptor is closed successfully, 0 if an error occurs.
 */
int saf_close(int fd) {
    JNIEnv *env = NULL;
    bool attached = false;
    jint getEnvRc =
        (*globalVm)->GetEnv(globalVm, (void **)&env, JNI_VERSION_1_6);
    if (getEnvRc != JNI_OK) {
        if (getEnvRc != JNI_EDETACHED) {
            LOGE("saf_close failed to GetEnv for class %s with rc %d.\n",
                 configClassName, getEnvRc);
            return 0;
        }

        if ((*globalVm)->AttachCurrentThread(globalVm, &env, NULL) != 0) {
            LOGE("saf_close failed to AttachCurrentThread for class %s.\n",
                 configClassName);
            return 0;
        } else {
            attached = true;
        }
    }
    int result =
        (*env)->CallStaticIntMethod(env, configClass, safCloseMethod, fd);
    if (attached)
        (*globalVm)->DetachCurrentThread(globalVm);
    return result;
}

static JNIEnv *ffkit_get_env(bool *attached) {
    JNIEnv *env = NULL;
    *attached = false;

    jint getEnvRc =
        (*globalVm)->GetEnv(globalVm, (void **)&env, JNI_VERSION_1_6);
    if (getEnvRc == JNI_OK) {
        return env;
    }

    if (getEnvRc == JNI_EDETACHED &&
        (*globalVm)->AttachCurrentThread(globalVm, &env, NULL) == 0) {
        *attached = true;
        return env;
    }

    return NULL;
}

static void ffkit_release_env(bool attached) {
    if (attached) {
        (*globalVm)->DetachCurrentThread(globalVm);
    }
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
    return atomic_fetch_add(&ffkitNextResourceId, 1);
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

    if (resource->directBufferRef != NULL) {
        bool attached = false;
        JNIEnv *env = ffkit_get_env(&attached);
        if (env != NULL) {
            (*env)->DeleteGlobalRef(env, resource->directBufferRef);
        }
        ffkit_release_env(attached);
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
            ((flags & AVIO_FLAG_WRITE) && resource->type != FFKIT_RESOURCE_OUTPUT) ||
            (!(flags & AVIO_FLAG_WRITE) && resource->type != FFKIT_RESOURCE_INPUT)) {
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

    handle = av_mallocz(sizeof(FFKitMemoryHandle));
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
    FFKitMemoryHandle *handle = opaque;
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

    newData = av_realloc(resource->data, newCapacity);
    if (newData == NULL) {
        return AVERROR(ENOMEM);
    }

    resource->data = newData;
    resource->capacity = newCapacity;
    return 0;
}

static int ffkit_memory_write(void *opaque, const unsigned char *buf, int size) {
    FFKitMemoryHandle *handle = opaque;
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

    requiredSize = handle->position + size;
    if (requiredSize < handle->position) {
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
    handle->position += size;
    if (handle->position > resource->size) {
        resource->size = handle->position;
    }
    pthread_mutex_unlock(&resource->mutex);

    return size;
}

static int64_t ffkit_memory_seek(void *opaque, int64_t pos, int whence) {
    FFKitMemoryHandle *handle = opaque;
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
        newPosition = handle->position + pos;
    } else if (whence == SEEK_END) {
        newPosition = resource->size + pos;
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
    FFKitMemoryHandle *handle = opaque;
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
            ((flags & AVIO_FLAG_WRITE) && resource->type != FFKIT_RESOURCE_OUTPUT) ||
            (!(flags & AVIO_FLAG_WRITE) && resource->type != FFKIT_RESOURCE_INPUT)) {
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

    handle = av_mallocz(sizeof(FFKitStreamHandle));
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

    if (resource == NULL || buf == NULL || size < 0) {
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
        chunkSize =
            (int)FFMIN((int64_t)chunkSize, resource->capacity - resource->writePosition);

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
        chunkSize =
            (int)FFMIN((int64_t)chunkSize, resource->capacity - resource->readPosition);

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
    FFKitStreamHandle *handle = opaque;
    int eof = 0;
    int ret;

    if (handle == NULL) {
        return AVERROR(EINVAL);
    }

    ret = ffkit_stream_read_bytes(handle->resource, buf, size, -1, NULL, &eof);
    return eof ? AVERROR_EOF : ret;
}

static int ffkit_stream_write(void *opaque, const unsigned char *buf, int size) {
    FFKitStreamHandle *handle = opaque;

    if (handle == NULL) {
        return AVERROR(EINVAL);
    }

    return ffkit_stream_write_bytes(handle->resource, buf, size, -1);
}

static int64_t ffkit_stream_seek(void *opaque, int64_t pos, int whence) {
    return AVERROR(ESPIPE);
}

static int ffkit_stream_close(void *opaque) {
    FFKitStreamHandle *handle = opaque;
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
 * Used by JNI methods to enable redirection.
 */
static void enableNativeRedirection() {
    mutexLock();

    if (redirectionEnabled != 0) {
        mutexUnlock();
        return;
    }
    redirectionEnabled = 1;

    mutexUnlock();

    int rc = pthread_create(&callbackThread, 0, callbackThreadFunction, 0);
    if (rc != 0) {
        LOGE("Failed to create callback thread (rc=%d).\n", rc);
        return;
    }

    av_log_set_callback(ffmpegkit_log_callback_function);
    set_report_callback(ffmpegkit_statistics_callback_function);
}

/**
 * Called when 'ffmpegkit' native library is loaded.
 *
 * @param vm pointer to the running virtual machine
 * @param reserved reserved
 * @return JNI version needed by 'ffmpegkit' library
 */
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **)(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("OnLoad failed to GetEnv for class %s.\n", configClassName);
        return JNI_FALSE;
    }

    jclass localConfigClass = (*env)->FindClass(env, configClassName);
    if (localConfigClass == NULL) {
        LOGE("OnLoad failed to FindClass %s.\n", configClassName);
        return JNI_FALSE;
    }

    if ((*env)->RegisterNatives(env, localConfigClass, configMethods,
                                sizeof(configMethods) / sizeof(configMethods[0])) < 0) {
        LOGE("OnLoad failed to RegisterNatives for class %s.\n",
             configClassName);
        return JNI_FALSE;
    }

    jclass localStringClass = (*env)->FindClass(env, stringClassName);
    if (localStringClass == NULL) {
        LOGE("OnLoad failed to FindClass %s.\n", stringClassName);
        return JNI_FALSE;
    }

    (*env)->GetJavaVM(env, &globalVm);

    logMethod =
        (*env)->GetStaticMethodID(env, localConfigClass, "log", "(JI[B)V");
    if (logMethod == NULL) {
        LOGE("OnLoad thread failed to GetStaticMethodID for %s.\n", "log");
        return JNI_FALSE;
    }

    statisticsMethod = (*env)->GetStaticMethodID(env, localConfigClass,
                                                 "statistics", "(JIFFJDDD)V");
    if (statisticsMethod == NULL) {
        LOGE("OnLoad thread failed to GetStaticMethodID for %s.\n",
             "statistics");
        return JNI_FALSE;
    }

    safOpenMethod =
        (*env)->GetStaticMethodID(env, localConfigClass, "safOpen", "(I)I");
    if (safOpenMethod == NULL) {
        LOGE("OnLoad thread failed to GetStaticMethodID for %s.\n", "safOpen");
        return JNI_FALSE;
    }

    safCloseMethod =
        (*env)->GetStaticMethodID(env, localConfigClass, "safClose", "(I)I");
    if (safCloseMethod == NULL) {
        LOGE("OnLoad thread failed to GetStaticMethodID for %s.\n", "safClose");
        return JNI_FALSE;
    }

    stringConstructor = (*env)->GetMethodID(env, localStringClass, "<init>",
                                            "([BLjava/lang/String;)V");
    if (stringConstructor == NULL) {
        LOGE("OnLoad thread failed to GetMethodID for %s.\n", "<init>");
        return JNI_FALSE;
    }

    av_jni_set_java_vm(vm, NULL);

    configClass = (jclass)((*env)->NewGlobalRef(env, localConfigClass));
    stringClass = (jclass)((*env)->NewGlobalRef(env, localStringClass));

    callbackDataHead = NULL;
    callbackDataTail = NULL;

    for (int i = 0; i < SESSION_MAP_SIZE; i++) {
        atomic_init(&sessionMap[i], 0);
        atomic_init(&sessionInTransitMessageCountMap[i], 0);
    }

    mutexInit();
    monitorInit();

    redirectionEnabled = 0;

#ifdef USES_FFMPEG_KIT_PROTOCOLS
    av_set_saf_open(saf_open);
    av_set_saf_close(saf_close);
    av_set_ffkitmem_functions(ffkit_memory_open, ffkit_memory_read,
                              ffkit_memory_write, ffkit_memory_seek,
                              ffkit_memory_close);
    av_set_ffkitstream_functions(ffkit_stream_open, ffkit_stream_read,
                                 ffkit_stream_write, ffkit_stream_seek,
                                 ffkit_stream_close);
#endif

    enableNativeRedirection();

    return JNI_VERSION_1_6;
}

/**
 * Sets log level.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param level log level
 */
JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_setNativeLogLevel(JNIEnv *env,
                                                               jclass object,
                                                               jint level) {
    configuredLogLevel = level;
}

/**
 * Returns current log level.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 */
JNIEXPORT jint JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeLogLevel(JNIEnv *env,
                                                               jclass object) {
    return configuredLogLevel;
}

/**
 * Enables log and statistics redirection.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 */
JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_enableNativeRedirection(
    JNIEnv *env, jclass object) {
    enableNativeRedirection();
}

/**
 * Disables log and statistics redirection.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 */
JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_disableNativeRedirection(
    JNIEnv *env, jclass object) {

    mutexLock();

    if (redirectionEnabled != 1) {
        mutexUnlock();
        return;
    }
    redirectionEnabled = 0;

    mutexUnlock();

    av_log_set_callback(av_log_default_callback);
    set_report_callback(NULL);

    monitorNotify();
}

/**
 * Returns FFmpeg version bundled within the library natively.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @return FFmpeg version string
 */
JNIEXPORT jstring JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegVersion(
    JNIEnv *env, jclass object) {
    return (*env)->NewStringUTF(env, FFMPEG_VERSION);
}

/**
 * Returns FFmpegKit library version natively.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @return FFmpegKit version string
 */
JNIEXPORT jstring JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeVersion(JNIEnv *env,
                                                              jclass object) {
    return (*env)->NewStringUTF(env, FFMPEG_KIT_VERSION);
}

/**
 * Returns the native FFmpegKit package name.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @return native FFmpegKit package name
 */
JNIEXPORT jstring JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativePackageName(
    JNIEnv *env, jclass object) {

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FFMPEG_KIT_PACKAGE_STR TOSTRING(FFMPEG_KIT_PACKAGE)

#ifdef FFMPEG_KIT_PACKAGE
    return (*env)->NewStringUTF(env, FFMPEG_KIT_PACKAGE_STR);
#else
    return (*env)->NewStringUTF(env, "");
#endif
}

/**
 * Synchronously executes FFmpeg natively with arguments provided.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param id session id
 * @param stringArray reference to the object holding FFmpeg command arguments
 * @return zero on successful execution, non-zero on error
 */
JNIEXPORT jint JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegExecute(
    JNIEnv *env, jclass object, jlong id, jobjectArray stringArray) {
    jstring *tempArray = NULL;
    int argumentCount = 1;
    char **argv = NULL;

    // SETS DEFAULT LOG LEVEL BEFORE STARTING A NEW RUN
    av_log_set_level(configuredLogLevel);

    if (stringArray) {
        int programArgumentCount = (*env)->GetArrayLength(env, stringArray);
        argumentCount = programArgumentCount + 1;

        tempArray =
            (jstring *)av_mallocz(sizeof(jstring) * programArgumentCount);
    }

    /* PRESERVE USAGE FORMAT
     *
     * ffmpeg <arguments>
     */
    argv = (char **)av_mallocz(sizeof(char *) * (argumentCount + 1));
    argv[0] = (char *)av_malloc(sizeof(char) * (strlen(LIB_NAME) + 1));
    strcpy(argv[0], LIB_NAME);

    // PREPARE ARRAY ELEMENTS
    if (stringArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            tempArray[i] =
                (jstring)(*env)->GetObjectArrayElement(env, stringArray, i);
            if (tempArray[i] != NULL) {
                argv[i + 1] =
                    (char *)(*env)->GetStringUTFChars(env, tempArray[i], 0);
            }
        }
    }
    argv[argumentCount] = NULL;

    // REGISTER THE ID BEFORE STARTING THE SESSION
    globalSessionId = (long)id;
    addSession((long)id);

    resetMessagesInTransmit(globalSessionId);

    // RUN
    int returnCode = ffmpeg_execute(argumentCount, argv);

    // ALWAYS REMOVE THE ID FROM THE MAP
    removeSession((long)id);

    // CLEANUP
    if (tempArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            if (tempArray[i] != NULL && argv[i + 1] != NULL) {
                (*env)->ReleaseStringUTFChars(env, tempArray[i], argv[i + 1]);
            }
        }

        av_free(tempArray);
    }
    av_free(argv[0]);
    av_free(argv);

    return returnCode;
}

/**
 * Cancels an ongoing FFmpeg operation natively.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param id session id
 */
JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegCancel(JNIEnv *env,
                                                                jclass object,
                                                                jlong id) {
    cancel_operation(id);
}

/**
 * Creates natively a new named pipe to use in FFmpeg operations.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param ffmpegPipePath full path of ffmpeg pipe
 * @return zero on successful creation, non-zero on error
 */
JNIEXPORT int JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNewNativeFFmpegPipe(
    JNIEnv *env, jclass object, jstring ffmpegPipePath) {
    const char *ffmpegPipePathString =
        (*env)->GetStringUTFChars(env, ffmpegPipePath, 0);

    return mkfifo(ffmpegPipePathString, S_IRWXU | S_IRWXG | S_IROTH);
}

JNIEXPORT jlong JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitInputBuffer(
    JNIEnv *env, jclass object, jbyteArray data) {
    int64_t id;
    jsize size;
    FFKitMemoryResource *resource;

    if (data == NULL) {
        return 0;
    }

    size = (*env)->GetArrayLength(env, data);
    resource = av_mallocz(sizeof(FFKitMemoryResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = av_malloc(size > 0 ? size : 1);
    if (resource->data == NULL) {
        av_free(resource);
        return 0;
    }

    if (size > 0) {
        (*env)->GetByteArrayRegion(env, data, 0, size,
                                   (jbyte *)resource->data);
    }

    id = ffkit_next_resource_id();
    resource->id = id;
    resource->type = FFKIT_RESOURCE_INPUT;
    resource->size = size;
    resource->capacity = size;
    resource->maxCapacity = size;
    resource->ownsData = 1;
    pthread_mutex_init(&resource->mutex, NULL);
    ffkit_memory_add(resource);

    return (jlong)id;
}

JNIEXPORT jlong JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitInputDirectBuffer(
    JNIEnv *env, jclass object, jobject byteBuffer, jint size) {
    int64_t id;
    void *address;
    jlong capacity;
    FFKitMemoryResource *resource;

    if (byteBuffer == NULL || size < 0) {
        return 0;
    }

    address = (*env)->GetDirectBufferAddress(env, byteBuffer);
    capacity = (*env)->GetDirectBufferCapacity(env, byteBuffer);
    if (address == NULL || capacity < size) {
        return 0;
    }

    resource = av_mallocz(sizeof(FFKitMemoryResource));
    if (resource == NULL) {
        return 0;
    }

    id = ffkit_next_resource_id();
    resource->id = id;
    resource->type = FFKIT_RESOURCE_INPUT;
    resource->data = (uint8_t *)address;
    resource->size = size;
    resource->capacity = size;
    resource->maxCapacity = size;
    resource->ownsData = 0;
    resource->directBufferRef = (*env)->NewGlobalRef(env, byteBuffer);
    if (resource->directBufferRef == NULL) {
        av_free(resource);
        return 0;
    }
    pthread_mutex_init(&resource->mutex, NULL);
    ffkit_memory_add(resource);

    return (jlong)id;
}

JNIEXPORT jlong JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitOutputBuffer(
    JNIEnv *env, jclass object, jlong initialCapacity, jlong maxCapacity) {
    int64_t id;
    FFKitMemoryResource *resource;
    int64_t capacity =
        initialCapacity > 0 ? initialCapacity : FFKIT_DEFAULT_OUTPUT_CAPACITY;
    int64_t maximumCapacity = maxCapacity > 0 ? maxCapacity : INT64_MAX;

    if (capacity > maximumCapacity) {
        return 0;
    }

    resource = av_mallocz(sizeof(FFKitMemoryResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = av_malloc(capacity);
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

    return (jlong)id;
}

JNIEXPORT jlong JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitBufferSize(
    JNIEnv *env, jclass object, jlong id) {
    FFKitMemoryResource *resource;
    int64_t size = -1;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        size = resource->size;
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    return (jlong)size;
}

JNIEXPORT jbyteArray JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitOutputBuffer(
    JNIEnv *env, jclass object, jlong id) {
    FFKitMemoryResource *resource;
    jbyteArray result = NULL;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        if (resource->type == FFKIT_RESOURCE_OUTPUT && resource->size <= INT32_MAX) {
            result = (*env)->NewByteArray(env, (jsize)resource->size);
            if (result != NULL && resource->size > 0) {
                (*env)->SetByteArrayRegion(env, result, 0,
                                           (jsize)resource->size,
                                           (jbyte *)resource->data);
            }
        }
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    return result;
}

JNIEXPORT jobject JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeFFmpegKitOutputBufferDirect(
    JNIEnv *env, jclass object, jlong id) {
    FFKitMemoryResource *resource;
    jobject result = NULL;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    resource = ffkit_memory_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        if (resource->type == FFKIT_RESOURCE_OUTPUT && resource->data != NULL) {
            result = (*env)->NewDirectByteBuffer(env, resource->data,
                                                 resource->size);
        }
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitMemoryRegistryMutex);

    return result;
}

JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_unregisterNativeFFmpegKitBuffer(
    JNIEnv *env, jclass object, jlong id) {
    FFKitMemoryResource *current;
    FFKitMemoryResource *previous = NULL;
    FFKitMemoryResource *resource = NULL;
    int freeResource = 0;

    pthread_mutex_lock(&ffkitMemoryRegistryMutex);
    current = ffkitMemoryResources;
    while (current != NULL) {
        if (current->id == id) {
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

JNIEXPORT jlong JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_registerNativeFFmpegKitStream(
    JNIEnv *env, jclass object, jlong capacity, jint type) {
    int64_t id;
    FFKitStreamResource *resource;
    int64_t streamCapacity =
        capacity > 0 ? capacity : FFKIT_DEFAULT_STREAM_CAPACITY;

    if (type != FFKIT_RESOURCE_INPUT && type != FFKIT_RESOURCE_OUTPUT) {
        return 0;
    }

    resource = av_mallocz(sizeof(FFKitStreamResource));
    if (resource == NULL) {
        return 0;
    }

    resource->data = av_malloc(streamCapacity);
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

    return (jlong)id;
}

JNIEXPORT jint JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamWrite(
    JNIEnv *env, jclass object, jlong id, jbyteArray data, jint offset,
    jint length, jint timeoutMs) {
    FFKitStreamResource *resource;
    uint8_t *buffer;
    int ret = AVERROR(ENOENT);
    int freeResource = 0;

    if (data == NULL || offset < 0 || length < 0 ||
        offset + length > (*env)->GetArrayLength(env, data)) {
        return AVERROR(EINVAL);
    }

    buffer = av_malloc(length > 0 ? length : 1);
    if (buffer == NULL) {
        return AVERROR(ENOMEM);
    }

    if (length > 0) {
        (*env)->GetByteArrayRegion(env, data, offset, length,
                                   (jbyte *)buffer);
    }

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(id);
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
        ret = ffkit_stream_write_bytes(resource, buffer, length, timeoutMs);

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

    av_free(buffer);
    return ret;
}

JNIEXPORT jbyteArray JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamRead(
    JNIEnv *env, jclass object, jlong id, jint maxBytes, jint timeoutMs) {
    FFKitStreamResource *resource;
    uint8_t *buffer;
    int ret = AVERROR(ENOENT);
    int timedOut = 0;
    int eof = 0;
    int freeResource = 0;
    jbyteArray result = NULL;

    if (maxBytes < 0) {
        return NULL;
    }

    buffer = av_malloc(maxBytes > 0 ? maxBytes : 1);
    if (buffer == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(id);
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
        result = (*env)->NewByteArray(env, ret);
        if (result != NULL && ret > 0) {
            (*env)->SetByteArrayRegion(env, result, 0, ret,
                                       (jbyte *)buffer);
        }
    } else if (timedOut) {
        result = NULL;
    }

    av_free(buffer);
    return result;
}

JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFmpegKitStreamCloseInput(
    JNIEnv *env, jclass object, jlong id) {
    FFKitStreamResource *resource;

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    resource = ffkit_stream_find_locked(id);
    if (resource != NULL) {
        pthread_mutex_lock(&resource->mutex);
        resource->writeClosed = 1;
        pthread_cond_broadcast(&resource->canRead);
        pthread_cond_broadcast(&resource->canWrite);
        pthread_mutex_unlock(&resource->mutex);
    }
    pthread_mutex_unlock(&ffkitStreamRegistryMutex);
}

JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_unregisterNativeFFmpegKitStream(
    JNIEnv *env, jclass object, jlong id) {
    FFKitStreamResource *current;
    FFKitStreamResource *previous = NULL;
    FFKitStreamResource *resource = NULL;
    int freeResource = 0;

    pthread_mutex_lock(&ffkitStreamRegistryMutex);
    current = ffkitStreamResources;
    while (current != NULL) {
        if (current->id == id) {
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

/**
 * Returns FFmpegKit library build date natively.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @return FFmpegKit library build date
 */
JNIEXPORT jstring JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_getNativeBuildDate(JNIEnv *env,
                                                                jclass object) {
    char buildDate[10];
    sprintf(buildDate, "%d", FFMPEG_KIT_BUILD_DATE);
    return (*env)->NewStringUTF(env, buildDate);
}

/**
 * Sets an environment variable natively
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param variableName environment variable name
 * @param variableValue environment variable value
 * @return zero on success, non-zero on error
 */
JNIEXPORT int JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_setNativeEnvironmentVariable(
    JNIEnv *env, jclass object, jstring variableName, jstring variableValue) {
    const char *variableNameString =
        (*env)->GetStringUTFChars(env, variableName, 0);
    const char *variableValueString =
        (*env)->GetStringUTFChars(env, variableValue, 0);

    int rc = setenv(variableNameString, variableValueString, 1);

    (*env)->ReleaseStringUTFChars(env, variableName, variableNameString);
    (*env)->ReleaseStringUTFChars(env, variableValue, variableValueString);
    return rc;
}

/**
 * Registers a new ignored signal. Ignored signals are not handled by the
 * library.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param signum signal number
 */
JNIEXPORT void JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_ignoreNativeSignal(JNIEnv *env,
                                                                jclass object,
                                                                jint signum) {
    if (signum == SIGQUIT) {
        handleSIGQUIT = 0;
    } else if (signum == SIGINT) {
        handleSIGINT = 0;
    } else if (signum == SIGTERM) {
        handleSIGTERM = 0;
    } else if (signum == SIGXCPU) {
        handleSIGXCPU = 0;
    } else if (signum == SIGPIPE) {
        handleSIGPIPE = 0;
    }
}

/**
 * Returns the number of native messages which are not transmitted to the Java
 * callbacks for the given session.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param id session id
 */
JNIEXPORT int JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_messagesInTransmit(JNIEnv *env,
                                                                jclass object,
                                                                jlong id) {
    return atomic_load(&sessionInTransitMessageCountMap[id % SESSION_MAP_SIZE]);
}
