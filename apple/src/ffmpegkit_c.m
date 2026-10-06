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
 * Flat C API (ffmpegkit_c.h) over the Objective-C classes of this library.
 *
 * Objective-C inside, plain C on the outside. Nothing Objective-C ever crosses
 * the boundary, so the functions can be called through dlopen/ctypes/cffi from
 * any language. The header declares exactly what this file implements: a
 * function is added to both together.
 *
 * HANDLES
 *
 *   A handle is the Objective-C object itself, retained once with
 *   CFBridgingRetain(). The ffk_*_free() functions release that retain. A
 *   handle passed into a callback is borrowed: it is valid until the callback
 *   returns, and ffk_session_retain() extends it. List handles retain an
 *   NSArray snapshot, so a list never changes under its reader.
 *
 * THREADS
 *
 *   Callers are usually not Objective-C threads, so every entry point runs in
 *   its own autorelease pool. Callbacks run on whichever thread the library
 *   delivers them on.
 *
 * ERRORS
 *
 *   No NSException escapes. Every entry point clears the calling thread's
 *   error slot on entry and, when it catches one, stores the reason there and
 *   returns a neutral value (0, NULL or nothing).
 *
 * CALLBACKS
 *
 *   A callback is a function pointer, a cookie and the function that releases
 *   the cookie. The library takes ownership of the cookie as soon as the call
 *   starts, even if the call then fails or the callback is NULL, and releases
 *   it once the last block that refers to it is gone. Objective-C blocks can
 *   not be inspected, so each block built here carries its function pointer
 *   and cookie as an associated object; that is what the *_get_*_callback()
 *   functions read back.
 *
 * BUFFERS AND STREAMS
 *
 *   The id based functions call the methods FFmpegKitConfig implements for the
 *   FFmpegKitInputBuffer, FFmpegKitOutputBuffer, FFmpegKitStreamInput and
 *   FFmpegKitStreamOutput classes. Those methods are private to the library
 *   (declared only in categories inside the class files), so they are declared
 *   the same way here.
 */

#import "ffmpegkit_c.h"

#import "AbstractSession.h"
#import "ArchDetect.h"
#import "Chapter.h"
#import "FFmpegKit.h"
#import "FFmpegKitConfig.h"
#import "FFmpegKitInputBuffer.h"
#import "FFmpegSession.h"
#import "FFprobeKit.h"
#import "FFprobeSession.h"
#import "Log.h"
#import "MediaInformation.h"
#import "MediaInformationJsonParser.h"
#import "MediaInformationSession.h"
#import "Packages.h"
#import "ReturnCode.h"
#import "Statistics.h"
#import "StreamInformation.h"
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Library methods that only exist in categories                             */
/* ------------------------------------------------------------------------ */

@interface FFmpegKitConfig (FFKCApiSupport)
+ (long)registerFFmpegKitInputBufferWithBytes:(const void *)bytes
                                       length:(NSUInteger)length;
+ (long)registerFFmpegKitOutputBuffer:(long)initialCapacity
                          maxCapacity:(long)maxCapacity;
+ (long)getFFmpegKitBufferSize:(long)bufferId;
+ (NSData *)getFFmpegKitOutputBuffer:(long)bufferId;
+ (void)unregisterFFmpegKitBuffer:(long)bufferId;
+ (long)registerFFmpegKitStream:(long)capacity type:(int)type;
+ (int)writeFFmpegKitStream:(long)streamId
                       data:(NSData *)data
                     offset:(NSUInteger)offset
                     length:(NSUInteger)length
                    timeout:(int)timeoutMs;
+ (NSData *)readFFmpegKitStream:(long)streamId
                       maxBytes:(int)maxBytes
                        timeout:(int)timeoutMs;
+ (void)closeFFmpegKitStreamInput:(long)streamId;
+ (void)unregisterFFmpegKitStream:(long)streamId;
@end

@interface FFmpegKitInputBuffer (FFKCApiUrlSupport)
+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension;
@end

/* ------------------------------------------------------------------------ */
/* Error slot                                                                */
/* ------------------------------------------------------------------------ */

/*
 * Errors are reported per thread. The message is a heap string of any length.
 * The thread's copy sits behind a pthread key whose destructor frees it, so a
 * thread that exits with an error still set leaks nothing.
 */
static __thread int ffkThreadErrorSet = 0;
static pthread_key_t ffkErrorKey;
static pthread_once_t ffkErrorKeyOnce = PTHREAD_ONCE_INIT;

static void ffkCreateErrorKey(void) { pthread_key_create(&ffkErrorKey, free); }

static void ffkClearError(void) {
    if (!ffkThreadErrorSet) {
        return;
    }
    pthread_once(&ffkErrorKeyOnce, ffkCreateErrorKey);
    free(pthread_getspecific(ffkErrorKey));
    pthread_setspecific(ffkErrorKey, NULL);
    ffkThreadErrorSet = 0;
}

static void ffkSetError(const char *message) {
    ffkClearError();
    pthread_once(&ffkErrorKeyOnce, ffkCreateErrorKey);
    // When the copy fails there is still an error, only without its message
    pthread_setspecific(ffkErrorKey,
                        strdup(message == NULL ? "Unknown error." : message));
    ffkThreadErrorSet = 1;
}

static void ffkSetErrorFromException(NSException *exception) {
    NSString *reason = exception.reason != nil ? exception.reason : exception.name;
    ffkSetError([reason UTF8String]);
}

/*
 * Every entry point is wrapped in FFK_BEGIN / FFK_END(neutral): the error slot
 * is cleared, the body runs in an autorelease pool, and an NSException raised
 * by it is turned into an error plus the neutral return value.
 */
#define FFK_BEGIN                                                              \
    ffkClearError();                                                           \
    @autoreleasepool {                                                         \
        @try {

#define FFK_END_VOID                                                           \
        } @catch (NSException * exception) {                                   \
            ffkSetErrorFromException(exception);                               \
        }                                                                      \
    }

#define FFK_END(neutral)                                                       \
    FFK_END_VOID                                                               \
    return (neutral);

/* ------------------------------------------------------------------------ */
/* Allocation                                                                */
/* ------------------------------------------------------------------------ */

/*
 * Strings and byte buffers are allocated with malloc and released by
 * ffk_string_free() and ffk_bytes_free(). NULL means the value is absent,
 * never that it is empty.
 */
static char *ffkCopyString(NSString *value) {
    if (value == nil) {
        return NULL;
    }
    const char *utf8 = [value UTF8String];
    if (utf8 == NULL) {
        return NULL;
    }
    const size_t size = strlen(utf8);
    char *copy = malloc(size + 1);
    if (copy == NULL) {
        ffkSetError("Out of memory.");
        return NULL;
    }
    memcpy(copy, utf8, size + 1);
    return copy;
}

/** Copies the bytes, or returns NULL. An empty value still gets a buffer. */
static uint8_t *ffkCopyBytes(NSData *value, size_t *size) {
    const size_t length = [value length];
    uint8_t *copy = malloc(length > 0 ? length : 1);
    if (copy == NULL) {
        ffkSetError("Out of memory.");
        return NULL;
    }
    if (length > 0) {
        memcpy(copy, [value bytes], length);
    }
    *size = length;
    return copy;
}

/* ------------------------------------------------------------------------ */
/* Handles                                                                   */
/* ------------------------------------------------------------------------ */

static void *ffkRetain(id object) {
    return object == nil ? NULL : (void *)CFBridgingRetain(object);
}

static void ffkRelease(const void *handle) {
    if (handle != NULL) {
        CFRelease(handle);
    }
}

static id ffkObject(const void *handle) {
    return (__bridge id)handle;
}

/** The object behind a handle when it is an instance of the class, else nil. */
static id ffkAs(const void *handle, Class expected) {
    id object = ffkObject(handle);
    return [object isKindOfClass:expected] ? object : nil;
}

static id<Session> ffkSession(const FFKSession *handle) {
    return (__bridge id<Session>)(const void *)handle;
}

static Log *ffkLog(const FFKLog *handle) {
    return (__bridge Log *)(const void *)handle;
}

static Statistics *ffkStatistics(const FFKStatistics *handle) {
    return (__bridge Statistics *)(const void *)handle;
}

static NSArray *ffkList(const void *handle) {
    return (__bridge NSArray *)handle;
}

static FFmpegSession *ffkFFmpegSession(const FFKSession *handle) {
    return ffkAs(handle, [FFmpegSession class]);
}

static FFprobeSession *ffkFFprobeSession(const FFKSession *handle) {
    return ffkAs(handle, [FFprobeSession class]);
}

static MediaInformationSession *
ffkMediaInformationSession(const FFKSession *handle) {
    return ffkAs(handle, [MediaInformationSession class]);
}

/** Retains a snapshot of an array, or returns NULL when there is none. */
static void *ffkRetainList(NSArray *array) {
    return array == nil ? NULL : ffkRetain([array copy]);
}

/* ------------------------------------------------------------------------ */
/* Conversions                                                               */
/* ------------------------------------------------------------------------ */

static int64_t ffkMilliseconds(NSDate *date) {
    return date == nil ? 0 : (int64_t)([date timeIntervalSince1970] * 1000.0);
}

/*
 * Text. NULL is the empty string everywhere: a NULL text, a NULL argument and a
 * NULL entry of an array are all empty. The one thing an NSString can not hold
 * is bytes that are not UTF-8, and those are rejected with an error.
 */

/** The text as a string, or nil and no error when it is not valid UTF-8. */
static NSString *ffkQuietText(const char *value) {
    return value == NULL ? @"" : [NSString stringWithUTF8String:value];
}

/** The text as a string, or nil and an error when it is not valid UTF-8. */
static NSString *ffkText(const char *value, const char *what) {
    NSString *text = ffkQuietText(value);
    if (text == nil) {
        char message[80];
        snprintf(message, sizeof(message), "The %s is not valid UTF-8.", what);
        ffkSetError(message);
    }
    return text;
}

/**
 * The texts as strings, or nil and an error when one is not valid UTF-8. A
 * NULL array holds `count` empty texts. `what` names one text in the message.
 */
static NSArray *ffkStrings(const char *const *values, size_t count,
                           const char *what) {
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:count];
    for (size_t i = 0; i < count; i++) {
        NSString *text = ffkQuietText(values == NULL ? NULL : values[i]);
        if (text == nil) {
            char message[80];
            snprintf(message, sizeof(message), "%s %zu is not valid UTF-8.",
                     what, i);
            ffkSetError(message);
            return nil;
        }
        [result addObject:text];
    }
    return result;
}

static NSArray *ffkArguments(const char *const *arguments, size_t count) {
    return ffkStrings(arguments, count, "Argument");
}

/** A property name. Names that are NULL or not UTF-8 simply match nothing. */
static NSString *ffkKey(const char *key) {
    return key == NULL ? nil : [NSString stringWithUTF8String:key];
}

/**
 * A mapping of names to values. An entry without a name is skipped, a NULL
 * value is the empty one, and a later entry replaces an earlier one with the
 * same name. Returns nil and an error when a text is not valid UTF-8.
 */
static NSDictionary *ffkStringMap(const char *const *keys,
                                  const char *const *values, size_t count) {
    NSMutableDictionary *mapping =
        [NSMutableDictionary dictionaryWithCapacity:count];
    for (size_t i = 0; i < count; i++) {
        const char *key = keys == NULL ? NULL : keys[i];
        if (key == NULL) {
            continue;
        }
        NSString *name = [NSString stringWithUTF8String:key];
        NSString *value = ffkQuietText(values == NULL ? NULL : values[i]);
        if (name == nil || value == nil) {
            char message[80];
            snprintf(message, sizeof(message),
                     "Mapping entry %zu is not valid UTF-8.", i);
            ffkSetError(message);
            return nil;
        }
        [mapping setObject:value forKey:name];
    }
    return mapping;
}

/** JSON text into a dictionary; anything that is not a JSON object is absent. */
static NSDictionary *ffkParseDictionary(const char *json) {
    if (json == NULL) {
        return nil;
    }
    NSData *data = [NSData dataWithBytes:json length:strlen(json)];
    id object = [NSJSONSerialization JSONObjectWithData:data
                                                options:kNilOptions
                                                  error:nil];
    return [object isKindOfClass:[NSDictionary class]] ? object : nil;
}

/** Serializes a JSON value (object, array, string, number) or returns NULL. */
static char *ffkJson(id value) {
    if (value == nil) {
        return NULL;
    }
    NSError *error = nil;
    NSData *data = [NSJSONSerialization
        dataWithJSONObject:value
                   options:NSJSONWritingFragmentsAllowed |
                           NSJSONWritingSortedKeys |
                           NSJSONWritingWithoutEscapingSlashes
                     error:&error];
    if (data == nil) {
        ffkSetError([[error localizedDescription] UTF8String]);
        return NULL;
    }
    return ffkCopyString([[NSString alloc] initWithData:data
                                               encoding:NSUTF8StringEncoding]);
}

/**
 * Stores a property as an int64 when it is a JSON integer that fits in one.
 *
 * A boolean, a fraction, an exponent (5.0 and 1e2 included, they are doubles)
 * and an integer beyond 64 bits are not numbers here. NSJSONSerialization tells
 * them apart by type: integers are 'q', or 'Q' when they exceed INT64_MAX, and
 * everything else is a double, an NSDecimalNumber or a boolean.
 */
static int ffkNumber(id value, int64_t *out) {
    if (![value isKindOfClass:[NSNumber class]]) {
        return 0;
    }
    NSNumber *number = value;
    if (CFGetTypeID((__bridge CFTypeRef)number) == CFBooleanGetTypeID()) {
        return 0;
    }
    int64_t result;
    switch ([number objCType][0]) {
    case 'c':
    case 's':
    case 'i':
    case 'l':
    case 'q':
        result = (int64_t)[number longLongValue];
        break;
    case 'C':
    case 'S':
    case 'I':
    case 'L':
    case 'Q': {
        const unsigned long long unsignedValue = [number unsignedLongLongValue];
        if (unsignedValue > (unsigned long long)INT64_MAX) {
            return 0;
        }
        result = (int64_t)unsignedValue;
        break;
    }
    default:
        return 0;
    }
    if (out != NULL) {
        *out = result;
    }
    return 1;
}

static char *ffkStringValue(id value) {
    return [value isKindOfClass:[NSString class]] ? ffkCopyString(value) : NULL;
}

/* ------------------------------------------------------------------------ */
/* Callbacks                                                                 */
/* ------------------------------------------------------------------------ */

/*
 * Owns a consumer cookie and remembers the function it belongs to. Blocks
 * capture it strongly, so the consumer's free function runs when the last
 * block that refers to it is released.
 */
@interface FFKCookie : NSObject
- (instancetype)initWithFunction:(void *)function
                            data:(void *)data
                            free:(ffk_free_cb)freeCallback;
- (void *)function;
- (void *)data;
@end

@implementation FFKCookie {
    void *_function;
    void *_data;
    ffk_free_cb _free;
}

- (instancetype)initWithFunction:(void *)function
                            data:(void *)data
                            free:(ffk_free_cb)freeCallback {
    self = [super init];
    if (self != nil) {
        _function = function;
        _data = data;
        _free = freeCallback;
    }
    return self;
}

- (void *)function {
    return _function;
}

- (void *)data {
    return _data;
}

- (void)dealloc {
    if (_free != NULL) {
        _free(_data);
    }
}

@end

static const char ffkCookieKey = 0;

/** Lets the *_get_*_callback() functions find the cookie of a block again. */
static void ffkAttach(id block, FFKCookie *cookie) {
    objc_setAssociatedObject(block, &ffkCookieKey, cookie,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static FFKCookie *ffkCookieOf(id block) {
    return block == nil ? nil : objc_getAssociatedObject(block, &ffkCookieKey);
}

/*
 * With a NULL callback the cookie is dropped on the spot, which releases it:
 * the contract stays the same whether or not a callback was given.
 */
static LogCallback ffkLogBlock(ffk_log_cb callback, void *data,
                               ffk_free_cb freeData) {
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:data
                                                       free:freeData];
    if (callback == NULL) {
        return nil;
    }
    LogCallback block = [^(Log *log) {
      callback((FFKLog *)(__bridge void *)log, [cookie data]);
    } copy];
    ffkAttach(block, cookie);
    return block;
}

static StatisticsCallback ffkStatisticsBlock(ffk_statistics_cb callback,
                                             void *data, ffk_free_cb freeData) {
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:data
                                                       free:freeData];
    if (callback == NULL) {
        return nil;
    }
    StatisticsCallback block = [^(Statistics *statistics) {
      callback((FFKStatistics *)(__bridge void *)statistics, [cookie data]);
    } copy];
    ffkAttach(block, cookie);
    return block;
}

static FFmpegSessionCompleteCallback
ffkFFmpegCompleteBlock(ffk_session_cb callback, void *data,
                       ffk_free_cb freeData) {
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:data
                                                       free:freeData];
    if (callback == NULL) {
        return nil;
    }
    FFmpegSessionCompleteCallback block = [^(FFmpegSession *session) {
      callback((FFKSession *)(__bridge void *)session, [cookie data]);
    } copy];
    ffkAttach(block, cookie);
    return block;
}

static FFprobeSessionCompleteCallback
ffkFFprobeCompleteBlock(ffk_session_cb callback, void *data,
                        ffk_free_cb freeData) {
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:data
                                                       free:freeData];
    if (callback == NULL) {
        return nil;
    }
    FFprobeSessionCompleteCallback block = [^(FFprobeSession *session) {
      callback((FFKSession *)(__bridge void *)session, [cookie data]);
    } copy];
    ffkAttach(block, cookie);
    return block;
}

static MediaInformationSessionCompleteCallback
ffkMediaInformationCompleteBlock(ffk_session_cb callback, void *data,
                                 ffk_free_cb freeData) {
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:data
                                                       free:freeData];
    if (callback == NULL) {
        return nil;
    }
    MediaInformationSessionCompleteCallback block =
        [^(MediaInformationSession *session) {
          callback((FFKSession *)(__bridge void *)session, [cookie data]);
        } copy];
    ffkAttach(block, cookie);
    return block;
}

/** Reads the function and cookie back out of a block built above. */
static int ffkReadLog(id block, ffk_log_cb *callback, void **userData) {
    FFKCookie *cookie = ffkCookieOf(block);
    if (cookie == nil) {
        return 0;
    }
    if (callback != NULL) {
        *callback = (ffk_log_cb)[cookie function];
    }
    if (userData != NULL) {
        *userData = [cookie data];
    }
    return 1;
}

static int ffkReadStatistics(id block, ffk_statistics_cb *callback,
                             void **userData) {
    FFKCookie *cookie = ffkCookieOf(block);
    if (cookie == nil) {
        return 0;
    }
    if (callback != NULL) {
        *callback = (ffk_statistics_cb)[cookie function];
    }
    if (userData != NULL) {
        *userData = [cookie data];
    }
    return 1;
}

static int ffkReadSession(id block, ffk_session_cb *callback,
                          void **userData) {
    FFKCookie *cookie = ffkCookieOf(block);
    if (cookie == nil) {
        return 0;
    }
    if (callback != NULL) {
        *callback = (ffk_session_cb)[cookie function];
    }
    if (userData != NULL) {
        *userData = [cookie data];
    }
    return 1;
}

/* ------------------------------------------------------------------------ */
/* Session delete listeners                                                  */
/* ------------------------------------------------------------------------ */

/*
 * SessionDeleteListener is a protocol, so the C API registers a function
 * pointer instead and hands back an opaque token. The listeners are kept here
 * so that a token can be resolved back to the object that has to be removed.
 */
@interface FFKSessionDeleteListener : NSObject <SessionDeleteListener>
- (instancetype)initWithCallback:(ffk_session_delete_cb)callback
                          cookie:(FFKCookie *)cookie;
@end

@implementation FFKSessionDeleteListener {
    ffk_session_delete_cb _callback;
    FFKCookie *_cookie;
}

- (instancetype)initWithCallback:(ffk_session_delete_cb)callback
                          cookie:(FFKCookie *)cookie {
    self = [super init];
    if (self != nil) {
        _callback = callback;
        _cookie = cookie;
    }
    return self;
}

- (void)sessionDeleted:(long)sessionId {
    _callback(sessionId, [_cookie data]);
}

@end

static NSLock *ffkListenerLock(void) {
    static NSLock *lock;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      lock = [[NSLock alloc] init];
    });
    return lock;
}

static NSMutableDictionary *ffkListeners(void) {
    static NSMutableDictionary *listeners;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      listeners = [[NSMutableDictionary alloc] init];
    });
    return listeners;
}

static long ffkNextListenerToken = 0;

/* ------------------------------------------------------------------------ */
/* Errors and memory                                                         */
/* ------------------------------------------------------------------------ */

int ffk_has_error(void) { return ffkThreadErrorSet; }

char *ffk_take_error(void) {
    if (!ffkThreadErrorSet) {
        return NULL;
    }
    // The error was set on this thread, so the key exists. The caller owns it.
    char *message = pthread_getspecific(ffkErrorKey);
    pthread_setspecific(ffkErrorKey, NULL);
    ffkThreadErrorSet = 0;
    return message;
}

void ffk_clear_error(void) { ffkClearError(); }

void ffk_string_free(char *value) { free(value); }

void ffk_bytes_free(uint8_t *data) { free(data); }

/* ------------------------------------------------------------------------ */
/* Lists                                                                     */
/* ------------------------------------------------------------------------ */

size_t ffk_string_list_size(const FFKStringList *list) {
    return list == NULL ? 0 : [ffkList(list) count];
}

char *ffk_string_list_get(const FFKStringList *list, size_t index) {
    FFK_BEGIN
    if (list == NULL || index >= [ffkList(list) count]) {
        return NULL;
    }
    return ffkCopyString([ffkList(list) objectAtIndex:index]);
    FFK_END(NULL)
}

void ffk_string_list_free(FFKStringList *list) { ffkRelease(list); }

/* The other lists hold objects, and elements are handles of their own. */
#define FFK_DEFINE_HANDLE_LIST(prefix, ListType, ElementType)                  \
    size_t prefix##_size(const ListType *list) {                               \
        return list == NULL ? 0 : [ffkList(list) count];                       \
    }                                                                          \
                                                                               \
    ElementType *prefix##_get(const ListType *list, size_t index) {            \
        FFK_BEGIN                                                              \
        if (list == NULL || index >= [ffkList(list) count]) {                  \
            return NULL;                                                       \
        }                                                                      \
        return (ElementType *)ffkRetain([ffkList(list) objectAtIndex:index]);  \
        FFK_END(NULL)                                                          \
    }                                                                          \
                                                                               \
    void prefix##_free(ListType *list) { ffkRelease(list); }

FFK_DEFINE_HANDLE_LIST(ffk_session_list, FFKSessionList, FFKSession)
FFK_DEFINE_HANDLE_LIST(ffk_log_list, FFKLogList, FFKLog)
FFK_DEFINE_HANDLE_LIST(ffk_statistics_list, FFKStatisticsList, FFKStatistics)
FFK_DEFINE_HANDLE_LIST(ffk_stream_information_list, FFKStreamInformationList,
                       FFKStreamInformation)
FFK_DEFINE_HANDLE_LIST(ffk_chapter_list, FFKChapterList, FFKChapter)

/* ------------------------------------------------------------------------ */
/* Log                                                                       */
/* ------------------------------------------------------------------------ */

FFKLog *ffk_log_create(long session_id, int level, const char *message) {
    FFK_BEGIN
    NSString *text = ffkText(message, "message");
    if (text == nil) {
        return NULL;
    }
    return (FFKLog *)ffkRetain([[Log alloc] init:session_id :level :text]);
    FFK_END(NULL)
}

void ffk_log_free(FFKLog *log) { ffkRelease(log); }

long ffk_log_get_session_id(const FFKLog *log) {
    FFK_BEGIN
    return log == NULL ? 0 : [ffkLog(log) getSessionId];
    FFK_END(0)
}

int ffk_log_get_level(const FFKLog *log) {
    FFK_BEGIN
    return log == NULL ? 0 : [ffkLog(log) getLevel];
    FFK_END(0)
}

char *ffk_log_get_message(const FFKLog *log) {
    FFK_BEGIN
    return log == NULL ? NULL : ffkCopyString([ffkLog(log) getMessage]);
    FFK_END(NULL)
}

/* ------------------------------------------------------------------------ */
/* Statistics                                                                */
/* ------------------------------------------------------------------------ */

FFKStatistics *ffk_statistics_create(long session_id, int video_frame_number,
                                     float video_fps, float video_quality,
                                     int64_t size, double time, double bitrate,
                                     double speed) {
    FFK_BEGIN
    return (FFKStatistics *)ffkRetain([[Statistics alloc]
                          init:session_id
              videoFrameNumber:video_frame_number
                      videoFps:video_fps
                  videoQuality:video_quality
                          size:size
                          time:time
                       bitrate:bitrate
                         speed:speed]);
    FFK_END(NULL)
}

void ffk_statistics_free(FFKStatistics *statistics) { ffkRelease(statistics); }

long ffk_statistics_get_session_id(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getSessionId];
    FFK_END(0)
}

int ffk_statistics_get_video_frame_number(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0
                              : [ffkStatistics(statistics) getVideoFrameNumber];
    FFK_END(0)
}

float ffk_statistics_get_video_fps(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getVideoFps];
    FFK_END(0)
}

float ffk_statistics_get_video_quality(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getVideoQuality];
    FFK_END(0)
}

int64_t ffk_statistics_get_size(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getSize];
    FFK_END(0)
}

double ffk_statistics_get_time(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getTime];
    FFK_END(0)
}

double ffk_statistics_get_bitrate(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getBitrate];
    FFK_END(0)
}

double ffk_statistics_get_speed(const FFKStatistics *statistics) {
    FFK_BEGIN
    return statistics == NULL ? 0 : [ffkStatistics(statistics) getSpeed];
    FFK_END(0)
}

/* ------------------------------------------------------------------------ */
/* Session                                                                   */
/* ------------------------------------------------------------------------ */

/*
 * Enumerations cross this API as plain int. A log level is any int, because
 * FFmpeg compares every message with it as a threshold, so a level needs no
 * check. The other enumerations have a closed set of values, and a value
 * outside of it is rejected with an error, the way text that is not valid
 * UTF-8 is.
 */

/** Stores the error of a value that is not one of the enumeration's values. */
static void ffkRejectEnum(const char *what, int value, const char *reason) {
    char message[96];
    snprintf(message, sizeof(message), "The %s %d %s", what, value, reason);
    ffkSetError(message);
}

/** A session state, or NO and an error when the value is none of them. */
static BOOL ffkSessionState(int value, SessionState *state) {
    switch (value) {
    case SessionStateCreated:
    case SessionStateRunning:
    case SessionStateFailed:
    case SessionStateCompleted:
        *state = (SessionState)value;
        return YES;
    }
    ffkRejectEnum("state", value, "is not a session state.");
    return NO;
}

/** A log redirection strategy, or NO and an error when the value is none. */
static BOOL ffkLogRedirectionStrategy(int value,
                                      LogRedirectionStrategy *strategy) {
    switch (value) {
    case LogRedirectionStrategyAlwaysPrintLogs:
    case LogRedirectionStrategyPrintLogsWhenNoCallbacksDefined:
    case LogRedirectionStrategyPrintLogsWhenGlobalCallbackNotDefined:
    case LogRedirectionStrategyPrintLogsWhenSessionCallbackNotDefined:
    case LogRedirectionStrategyNeverPrintLogs:
        *strategy = (LogRedirectionStrategy)value;
        return YES;
    }
    ffkRejectEnum("strategy", value, "is not a log redirection strategy.");
    return NO;
}

/** A signal that the library can ignore, or NO and an error. */
static BOOL ffkSignal(int value, Signal *signal) {
    switch (value) {
    case SignalInt:
    case SignalQuit:
    case SignalPipe:
    case SignalTerm:
    case SignalXcpu:
        *signal = (Signal)value;
        return YES;
    }
    ffkRejectEnum("signal", value, "cannot be ignored.");
    return NO;
}

/** The strategy of a session create that selects the configured strategy. */
static const int ffkUseConfiguredStrategy = -1;

/**
 * The strategy of a session create: ffkUseConfiguredStrategy means "whatever
 * FFmpegKitConfig is set to", and anything else has to be a log redirection
 * strategy, or the result is NO and an error.
 */
static BOOL ffkStrategy(int value, LogRedirectionStrategy *strategy) {
    if (value == ffkUseConfiguredStrategy) {
        *strategy = [FFmpegKitConfig getLogRedirectionStrategy];
        return YES;
    }
    return ffkLogRedirectionStrategy(value, strategy);
}

FFKSession *ffk_abstract_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    int log_redirection_strategy) {
    FFK_BEGIN
    // The callbacks come first: the library owns their cookies from here on
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    LogRedirectionStrategy strategy;
    if (!ffkStrategy(log_redirection_strategy, &strategy)) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([[AbstractSession alloc]
                          init:list
               withLogCallback:log
    withLogRedirectionStrategy:strategy]);
    FFK_END(NULL)
}

FFKSession *ffk_ffmpeg_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, ffk_statistics_cb statistics_callback,
    void *statistics_user_data, ffk_free_cb statistics_free,
    int log_redirection_strategy) {
    FFK_BEGIN
    FFmpegSessionCompleteCallback complete = ffkFFmpegCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);
    StatisticsCallback statistics = ffkStatisticsBlock(
        statistics_callback, statistics_user_data, statistics_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    LogRedirectionStrategy strategy;
    if (!ffkStrategy(log_redirection_strategy, &strategy)) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFmpegSession
                       create:list
          withCompleteCallback:complete
               withLogCallback:log
        withStatisticsCallback:statistics
    withLogRedirectionStrategy:strategy]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobe_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, int log_redirection_strategy) {
    FFK_BEGIN
    FFprobeSessionCompleteCallback complete = ffkFFprobeCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    LogRedirectionStrategy strategy;
    if (!ffkStrategy(log_redirection_strategy, &strategy)) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeSession
                       create:list
          withCompleteCallback:complete
               withLogCallback:log
    withLogRedirectionStrategy:strategy]);
    FFK_END(NULL)
}

FFKSession *ffk_media_information_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free) {
    FFK_BEGIN
    MediaInformationSessionCompleteCallback complete =
        ffkMediaInformationCompleteBlock(complete_callback, complete_user_data,
                                         complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([MediaInformationSession
                     create:list
        withCompleteCallback:complete
             withLogCallback:log]);
    FFK_END(NULL)
}

void ffk_session_free(FFKSession *session) { ffkRelease(session); }

FFKSession *ffk_session_retain(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? NULL : (FFKSession *)ffkRetain(ffkSession(session));
    FFK_END(NULL)
}

long ffk_session_get_session_id(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : [ffkSession(session) getSessionId];
    FFK_END(0)
}

int64_t ffk_session_get_create_time(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : ffkMilliseconds([ffkSession(session) getCreateTime]);
    FFK_END(0)
}

int64_t ffk_session_get_start_time(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : ffkMilliseconds([ffkSession(session) getStartTime]);
    FFK_END(0)
}

int64_t ffk_session_get_end_time(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : ffkMilliseconds([ffkSession(session) getEndTime]);
    FFK_END(0)
}

long ffk_session_get_duration(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : [ffkSession(session) getDuration];
    FFK_END(0)
}

FFKStringList *ffk_session_get_arguments(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL
               ? NULL
               : (FFKStringList *)ffkRetainList([ffkSession(session) getArguments]);
    FFK_END(NULL)
}

char *ffk_session_get_command(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? NULL : ffkCopyString([ffkSession(session) getCommand]);
    FFK_END(NULL)
}

FFKLogList *ffk_session_get_all_logs_with_timeout(const FFKSession *session,
                                                  int wait_timeout) {
    FFK_BEGIN
    return session == NULL ? NULL
                           : (FFKLogList *)ffkRetainList([ffkSession(session)
                                 getAllLogsWithTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKLogList *ffk_session_get_all_logs(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL
               ? NULL
               : (FFKLogList *)ffkRetainList([ffkSession(session) getAllLogs]);
    FFK_END(NULL)
}

FFKLogList *ffk_session_get_logs(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL
               ? NULL
               : (FFKLogList *)ffkRetainList([ffkSession(session) getLogs]);
    FFK_END(NULL)
}

char *ffk_session_get_all_logs_as_string_with_timeout(const FFKSession *session,
                                                      int wait_timeout) {
    FFK_BEGIN
    return session == NULL ? NULL
                           : ffkCopyString([ffkSession(session)
                                 getAllLogsAsStringWithTimeout:wait_timeout]);
    FFK_END(NULL)
}

char *ffk_session_get_all_logs_as_string(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL
               ? NULL
               : ffkCopyString([ffkSession(session) getAllLogsAsString]);
    FFK_END(NULL)
}

char *ffk_session_get_logs_as_string(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? NULL
                           : ffkCopyString([ffkSession(session) getLogsAsString]);
    FFK_END(NULL)
}

char *ffk_session_get_output(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? NULL : ffkCopyString([ffkSession(session) getOutput]);
    FFK_END(NULL)
}

int ffk_session_get_state(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0 : (int)[ffkSession(session) getState];
    FFK_END(0)
}

int ffk_session_get_return_code(const FFKSession *session, int *value) {
    FFK_BEGIN
    if (session == NULL) {
        return 0;
    }
    ReturnCode *returnCode = [ffkSession(session) getReturnCode];
    if (returnCode == nil) {
        return 0;
    }
    if (value != NULL) {
        *value = [returnCode getValue];
    }
    return 1;
    FFK_END(0)
}

char *ffk_session_get_fail_stack_trace(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL
               ? NULL
               : ffkCopyString([ffkSession(session) getFailStackTrace]);
    FFK_END(NULL)
}

int ffk_session_get_log_redirection_strategy(const FFKSession *session) {
    FFK_BEGIN
    return session == NULL ? 0
                           : (int)[ffkSession(session) getLogRedirectionStrategy];
    FFK_END(0)
}

int ffk_session_there_are_asynchronous_messages_in_transmit(
    const FFKSession *session) {
    FFK_BEGIN
    return session != NULL &&
           [ffkSession(session) thereAreAsynchronousMessagesInTransmit];
    FFK_END(0)
}

void ffk_session_wait_for_asynchronous_messages_in_transmit(
    const FFKSession *session, int timeout) {
    FFK_BEGIN
    AbstractSession *abstractSession = ffkAs(session, [AbstractSession class]);
    [abstractSession waitForAsynchronousMessagesInTransmit:timeout];
    FFK_END_VOID
}

void ffk_session_add_log(FFKSession *session, long log_session_id, int level,
                         const char *message) {
    FFK_BEGIN
    NSString *text = ffkText(message, "message");
    if (session != NULL && text != nil) {
        [ffkSession(session)
            addLog:[[Log alloc] init:log_session_id :level :text]];
    }
    FFK_END_VOID
}

void ffk_session_start_running(FFKSession *session) {
    FFK_BEGIN
    if (session != NULL) {
        [ffkSession(session) startRunning];
    }
    FFK_END_VOID
}

void ffk_session_complete(FFKSession *session, int return_code) {
    FFK_BEGIN
    if (session != NULL) {
        [ffkSession(session) complete:[[ReturnCode alloc] init:return_code]];
    }
    FFK_END_VOID
}

void ffk_session_fail(FFKSession *session, const char *error) {
    FFK_BEGIN
    if (session == NULL) {
        return;
    }
    NSString *text = ffkText(error, "error");
    if (text == nil) {
        return;
    }
    // The stack trace comes from the exception, so it has to have been raised
    NSException *failure = nil;
    @try {
        @throw [NSException exceptionWithName:@"FFmpegKitSessionFailure"
                                       reason:text
                                     userInfo:@{@"error" : text}];
    } @catch (NSException *raised) {
        failure = raised;
    }
    [ffkSession(session) fail:failure];
    FFK_END_VOID
}

int ffk_session_is_ffmpeg(const FFKSession *session) {
    FFK_BEGIN
    return session != NULL && [ffkSession(session) isFFmpeg];
    FFK_END(0)
}

int ffk_session_is_ffprobe(const FFKSession *session) {
    FFK_BEGIN
    return session != NULL && [ffkSession(session) isFFprobe];
    FFK_END(0)
}

int ffk_session_is_media_information(const FFKSession *session) {
    FFK_BEGIN
    return session != NULL && [ffkSession(session) isMediaInformation];
    FFK_END(0)
}

void ffk_session_cancel(FFKSession *session) {
    FFK_BEGIN
    if (session != NULL) {
        [ffkSession(session) cancel];
    }
    FFK_END_VOID
}

int ffk_session_get_log_callback(const FFKSession *session,
                                 ffk_log_cb *callback, void **user_data) {
    FFK_BEGIN
    if (session == NULL) {
        return 0;
    }
    return ffkReadLog([ffkSession(session) getLogCallback], callback, user_data);
    FFK_END(0)
}

int ffk_session_get_complete_callback(const FFKSession *session,
                                      ffk_session_cb *callback,
                                      void **user_data) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    if (ffmpegSession != nil) {
        return ffkReadSession([ffmpegSession getCompleteCallback], callback,
                              user_data);
    }
    FFprobeSession *ffprobeSession = ffkFFprobeSession(session);
    if (ffprobeSession != nil) {
        return ffkReadSession([ffprobeSession getCompleteCallback], callback,
                              user_data);
    }
    MediaInformationSession *mediaSession = ffkMediaInformationSession(session);
    if (mediaSession != nil) {
        return ffkReadSession([mediaSession getCompleteCallback], callback,
                              user_data);
    }
    return 0;
    FFK_END(0)
}

/* ---- FFmpeg session specific -------------------------------------------- */

FFKStatisticsList *
ffk_ffmpeg_session_get_all_statistics_with_timeout(FFKSession *session,
                                                   int wait_timeout) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    return ffmpegSession == nil
               ? NULL
               : (FFKStatisticsList *)ffkRetainList(
                     [ffmpegSession getAllStatisticsWithTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKStatisticsList *ffk_ffmpeg_session_get_all_statistics(FFKSession *session) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    return ffmpegSession == nil
               ? NULL
               : (FFKStatisticsList *)ffkRetainList(
                     [ffmpegSession getAllStatistics]);
    FFK_END(NULL)
}

FFKStatisticsList *ffk_ffmpeg_session_get_statistics(FFKSession *session) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    return ffmpegSession == nil
               ? NULL
               : (FFKStatisticsList *)ffkRetainList([ffmpegSession getStatistics]);
    FFK_END(NULL)
}

FFKStatistics *
ffk_ffmpeg_session_get_last_received_statistics(FFKSession *session) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    return ffmpegSession == nil
               ? NULL
               : (FFKStatistics *)ffkRetain(
                     [ffmpegSession getLastReceivedStatistics]);
    FFK_END(NULL)
}

void ffk_ffmpeg_session_add_statistics(FFKSession *session,
                                       const FFKStatistics *statistics) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    if (ffmpegSession != nil && statistics != NULL) {
        [ffmpegSession addStatistics:ffkStatistics(statistics)];
    }
    FFK_END_VOID
}

int ffk_ffmpeg_session_get_statistics_callback(const FFKSession *session,
                                               ffk_statistics_cb *callback,
                                               void **user_data) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    if (ffmpegSession == nil) {
        return 0;
    }
    return ffkReadStatistics([ffmpegSession getStatisticsCallback], callback,
                             user_data);
    FFK_END(0)
}

/* ---- media information session specific --------------------------------- */

FFKMediaInformation *
ffk_media_information_session_get_media_information(FFKSession *session) {
    FFK_BEGIN
    MediaInformationSession *mediaSession = ffkMediaInformationSession(session);
    return mediaSession == nil
               ? NULL
               : (FFKMediaInformation *)ffkRetain([mediaSession getMediaInformation]);
    FFK_END(NULL)
}

void ffk_media_information_session_set_media_information(
    FFKSession *session, FFKMediaInformation *media_information) {
    FFK_BEGIN
    MediaInformationSession *mediaSession = ffkMediaInformationSession(session);
    if (mediaSession != nil) {
        [mediaSession
            setMediaInformation:ffkAs(media_information, [MediaInformation class])];
    }
    FFK_END_VOID
}

/* ------------------------------------------------------------------------ */
/* Chapter, stream information and media information properties              */
/* ------------------------------------------------------------------------ */

/*
 * The three metadata classes expose the same generic property accessors. The
 * convenience getters on top of them (getFilename, getCodec and so on) are
 * pure key lookups and are left to the caller.
 */
#define FFK_DEFINE_PROPERTY_ACCESSORS(prefix, HandleType, ObjcType)            \
    void prefix##_free(HandleType *handle) { ffkRelease(handle); }             \
                                                                               \
    int prefix##_get_number_property(HandleType *handle, const char *key,      \
                                     int64_t *value) {                         \
        FFK_BEGIN                                                              \
        ObjcType *object = ffkAs(handle, [ObjcType class]);                    \
        NSString *name = ffkKey(key);                                          \
        if (object == nil || name == nil) {                                    \
            return 0;                                                          \
        }                                                                      \
        return ffkNumber([object getNumberProperty:name], value);              \
        FFK_END(0)                                                             \
    }                                                                          \
                                                                               \
    char *prefix##_get_string_property(HandleType *handle, const char *key) {  \
        FFK_BEGIN                                                              \
        ObjcType *object = ffkAs(handle, [ObjcType class]);                    \
        NSString *name = ffkKey(key);                                          \
        if (object == nil || name == nil) {                                    \
            return NULL;                                                       \
        }                                                                      \
        return ffkStringValue([object getStringProperty:name]);                \
        FFK_END(NULL)                                                          \
    }                                                                          \
                                                                               \
    char *prefix##_get_property_json(HandleType *handle, const char *key) {    \
        FFK_BEGIN                                                              \
        ObjcType *object = ffkAs(handle, [ObjcType class]);                    \
        NSString *name = ffkKey(key);                                          \
        if (object == nil || name == nil) {                                    \
            return NULL;                                                       \
        }                                                                      \
        return ffkJson([object getProperty:name]);                             \
        FFK_END(NULL)                                                          \
    }                                                                          \
                                                                               \
    char *prefix##_get_all_properties_json(HandleType *handle) {               \
        FFK_BEGIN                                                              \
        ObjcType *object = ffkAs(handle, [ObjcType class]);                    \
        return object == nil ? NULL : ffkJson([object getAllProperties]);      \
        FFK_END(NULL)                                                          \
    }

FFKChapter *ffk_chapter_create(const char *value_json) {
    FFK_BEGIN
    return (FFKChapter *)ffkRetain(
        [[Chapter alloc] init:ffkParseDictionary(value_json)]);
    FFK_END(NULL)
}

FFKStreamInformation *ffk_stream_information_create(const char *value_json) {
    FFK_BEGIN
    return (FFKStreamInformation *)ffkRetain(
        [[StreamInformation alloc] init:ffkParseDictionary(value_json)]);
    FFK_END(NULL)
}

FFKMediaInformation *ffk_media_information_create(
    const char *value_json, const char *const *stream_json,
    size_t stream_count, const char *const *chapter_json,
    size_t chapter_count) {
    FFK_BEGIN
    NSMutableArray *streams = [NSMutableArray arrayWithCapacity:stream_count];
    for (size_t i = 0; i < stream_count; i++) {
        [streams addObject:[[StreamInformation alloc]
                               init:ffkParseDictionary(
                                        stream_json == NULL ? NULL
                                                            : stream_json[i])]];
    }
    NSMutableArray *chapters = [NSMutableArray arrayWithCapacity:chapter_count];
    for (size_t i = 0; i < chapter_count; i++) {
        [chapters addObject:[[Chapter alloc]
                                init:ffkParseDictionary(
                                         chapter_json == NULL ? NULL
                                                              : chapter_json[i])]];
    }
    return (FFKMediaInformation *)ffkRetain([[MediaInformation alloc]
                 init:ffkParseDictionary(value_json)
          withStreams:streams
         withChapters:chapters]);
    FFK_END(NULL)
}

FFK_DEFINE_PROPERTY_ACCESSORS(ffk_chapter, FFKChapter, Chapter)
FFK_DEFINE_PROPERTY_ACCESSORS(ffk_stream_information, FFKStreamInformation,
                              StreamInformation)
FFK_DEFINE_PROPERTY_ACCESSORS(ffk_media_information, FFKMediaInformation,
                              MediaInformation)

FFKStreamInformationList *
ffk_media_information_get_streams(FFKMediaInformation *media_information) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    return object == nil
               ? NULL
               : (FFKStreamInformationList *)ffkRetainList([object getStreams]);
    FFK_END(NULL)
}

FFKChapterList *
ffk_media_information_get_chapters(FFKMediaInformation *media_information) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    return object == nil ? NULL
                         : (FFKChapterList *)ffkRetainList([object getChapters]);
    FFK_END(NULL)
}

int ffk_media_information_get_number_format_property(
    FFKMediaInformation *media_information, const char *key, int64_t *value) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    NSString *name = ffkKey(key);
    if (object == nil || name == nil) {
        return 0;
    }
    return ffkNumber([object getNumberFormatProperty:name], value);
    FFK_END(0)
}

char *ffk_media_information_get_string_format_property(
    FFKMediaInformation *media_information, const char *key) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    NSString *name = ffkKey(key);
    if (object == nil || name == nil) {
        return NULL;
    }
    return ffkStringValue([object getStringFormatProperty:name]);
    FFK_END(NULL)
}

char *ffk_media_information_get_format_property_json(
    FFKMediaInformation *media_information, const char *key) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    NSString *name = ffkKey(key);
    if (object == nil || name == nil) {
        return NULL;
    }
    return ffkJson([object getFormatProperty:name]);
    FFK_END(NULL)
}

char *ffk_media_information_get_format_properties_json(
    FFKMediaInformation *media_information) {
    FFK_BEGIN
    MediaInformation *object = ffkAs(media_information, [MediaInformation class]);
    return object == nil ? NULL : ffkJson([object getFormatProperties]);
    FFK_END(NULL)
}

FFKMediaInformation *
ffk_media_information_parser_from(const char *ffprobe_json_output) {
    FFK_BEGIN
    NSString *text = ffkQuietText(ffprobe_json_output);
    if (text == nil) {
        return NULL;
    }
    // Not [MediaInformationJsonParser from:]: when it fails it logs a call
    // stack. This one is silent and leaves the error slot alone, so a failure is
    // nothing but a NULL result.
    @try {
        return (FFKMediaInformation *)ffkRetain(
            [MediaInformationJsonParser fromWithError:text]);
    } @catch (NSException *failure) {
        return NULL;
    }
    FFK_END(NULL)
}

FFKMediaInformation *
ffk_media_information_parser_from_with_error(const char *ffprobe_json_output) {
    FFK_BEGIN
    NSString *text = ffkQuietText(ffprobe_json_output);
    NSString *problem = @"the text is not valid UTF-8";
    if (text != nil) {
        @try {
            return (FFKMediaInformation *)ffkRetain(
                [MediaInformationJsonParser fromWithError:text]);
        } @catch (NSException *failure) {
            // The exception's reason is only the JSON reader's error code: the
            // reader's own description is in its user info
            NSString *detail =
                [[failure userInfo] objectForKey:NSDebugDescriptionErrorKey];
            problem = detail != nil ? detail : [failure reason];
        }
    }
    NSString *message = [NSString
        stringWithFormat:@"Media information could not be parsed: %@", problem];
    ffkSetError([message UTF8String]);
    return NULL;
    FFK_END(NULL)
}

/* ------------------------------------------------------------------------ */
/* FFmpegKit                                                                 */
/* ------------------------------------------------------------------------ */

FFKSession *
ffk_ffmpegkit_execute_with_arguments(const char *const *arguments,
                                     size_t argument_count) {
    FFK_BEGIN
    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFmpegKit executeWithArguments:list]);
    FFK_END(NULL)
}

FFKSession *ffk_ffmpegkit_execute_with_arguments_async(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, ffk_statistics_cb statistics_callback,
    void *statistics_user_data, ffk_free_cb statistics_free) {
    FFK_BEGIN
    // The callbacks come first: the library owns their cookies from here on
    FFmpegSessionCompleteCallback complete = ffkFFmpegCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);
    StatisticsCallback statistics = ffkStatisticsBlock(
        statistics_callback, statistics_user_data, statistics_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFmpegKit executeWithArgumentsAsync:list
                                                   withCompleteCallback:complete
                                                        withLogCallback:log
                                                 withStatisticsCallback:statistics]);
    FFK_END(NULL)
}

FFKSession *ffk_ffmpegkit_execute(const char *command) {
    FFK_BEGIN
    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFmpegKit execute:text]);
    FFK_END(NULL)
}

FFKSession *ffk_ffmpegkit_execute_async(
    const char *command, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    ffk_statistics_cb statistics_callback, void *statistics_user_data,
    ffk_free_cb statistics_free) {
    FFK_BEGIN
    FFmpegSessionCompleteCallback complete = ffkFFmpegCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);
    StatisticsCallback statistics = ffkStatisticsBlock(
        statistics_callback, statistics_user_data, statistics_free);

    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFmpegKit executeAsync:text
                                      withCompleteCallback:complete
                                           withLogCallback:log
                                    withStatisticsCallback:statistics]);
    FFK_END(NULL)
}

void ffk_ffmpegkit_cancel(void) {
    FFK_BEGIN
    [FFmpegKit cancel];
    FFK_END_VOID
}

void ffk_ffmpegkit_cancel_session(long session_id) {
    FFK_BEGIN
    [FFmpegKit cancel:session_id];
    FFK_END_VOID
}

FFKSessionList *ffk_ffmpegkit_list_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList([FFmpegKit listSessions]);
    FFK_END(NULL)
}

/* ------------------------------------------------------------------------ */
/* FFprobeKit                                                                */
/* ------------------------------------------------------------------------ */

FFKSession *
ffk_ffprobekit_execute_with_arguments(const char *const *arguments,
                                      size_t argument_count) {
    FFK_BEGIN
    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit executeWithArguments:list]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_execute_with_arguments_async(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free) {
    FFK_BEGIN
    FFprobeSessionCompleteCallback complete = ffkFFprobeCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit executeWithArgumentsAsync:list
                                                    withCompleteCallback:complete
                                                         withLogCallback:log]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_execute(const char *command) {
    FFK_BEGIN
    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit execute:text]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_execute_async(
    const char *command, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free) {
    FFK_BEGIN
    FFprobeSessionCompleteCallback complete = ffkFFprobeCompleteBlock(
        complete_callback, complete_user_data, complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit executeAsync:text
                                       withCompleteCallback:complete
                                            withLogCallback:log]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_get_media_information(const char *path,
                                                 int wait_timeout) {
    FFK_BEGIN
    NSString *text = ffkText(path, "path");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain(
        [FFprobeKit getMediaInformation:text withTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_get_media_information_async(
    const char *path, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    int wait_timeout) {
    FFK_BEGIN
    MediaInformationSessionCompleteCallback complete =
        ffkMediaInformationCompleteBlock(complete_callback, complete_user_data,
                                         complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSString *text = ffkText(path, "path");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit getMediaInformationAsync:text
                                                   withCompleteCallback:complete
                                                        withLogCallback:log
                                                            withTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKSession *
ffk_ffprobekit_get_media_information_from_command(const char *command) {
    FFK_BEGIN
    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain(
        [FFprobeKit getMediaInformationFromCommand:text]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_get_media_information_from_command_async(
    const char *command, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    int wait_timeout) {
    FFK_BEGIN
    MediaInformationSessionCompleteCallback complete =
        ffkMediaInformationCompleteBlock(complete_callback, complete_user_data,
                                         complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit
        getMediaInformationFromCommandAsync:text
                       withCompleteCallback:complete
                            withLogCallback:log
                            onDispatchQueue:dispatch_get_global_queue(
                                                DISPATCH_QUEUE_PRIORITY_DEFAULT,
                                                0)
                                withTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_get_media_information_from_command_arguments(
    const char *const *arguments, size_t argument_count, int wait_timeout) {
    FFK_BEGIN
    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit
        getMediaInformationFromCommandArguments:list
                                    withTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKSession *ffk_ffprobekit_get_media_information_from_command_arguments_async(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, int wait_timeout) {
    FFK_BEGIN
    MediaInformationSessionCompleteCallback complete =
        ffkMediaInformationCompleteBlock(complete_callback, complete_user_data,
                                         complete_free);
    LogCallback log = ffkLogBlock(log_callback, log_user_data, log_free);

    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return (FFKSession *)ffkRetain([FFprobeKit
        getMediaInformationFromCommandArgumentsAsync:list
                                withCompleteCallback:complete
                                     withLogCallback:log
                                     onDispatchQueue:
                                         dispatch_get_global_queue(
                                             DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)
                                         withTimeout:wait_timeout]);
    FFK_END(NULL)
}

FFKSessionList *ffk_ffprobekit_list_ffprobe_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList([FFprobeKit listFFprobeSessions]);
    FFK_END(NULL)
}

FFKSessionList *ffk_ffprobekit_list_media_information_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList(
        [FFprobeKit listMediaInformationSessions]);
    FFK_END(NULL)
}

/* ------------------------------------------------------------------------ */
/* FFmpegKitConfig                                                           */
/* ------------------------------------------------------------------------ */

void ffk_config_enable_redirection(void) {
    FFK_BEGIN
    [FFmpegKitConfig enableRedirection];
    FFK_END_VOID
}

void ffk_config_disable_redirection(void) {
    FFK_BEGIN
    [FFmpegKitConfig disableRedirection];
    FFK_END_VOID
}

int ffk_config_set_fontconfig_configuration_path(const char *path) {
    FFK_BEGIN
    NSString *text = ffkText(path, "path");
    if (text == nil) {
        return -1;
    }
    return [FFmpegKitConfig setFontconfigConfigurationPath:text];
    FFK_END(-1)
}

void ffk_config_set_font_directory(const char *font_directory_path,
                                   const char *const *mapping_keys,
                                   const char *const *mapping_values,
                                   size_t mapping_count) {
    FFK_BEGIN
    NSString *path = ffkText(font_directory_path, "font directory path");
    if (path == nil) {
        return;
    }
    NSDictionary *mapping =
        ffkStringMap(mapping_keys, mapping_values, mapping_count);
    if (mapping == nil) {
        return;
    }
    [FFmpegKitConfig setFontDirectory:path with:mapping];
    FFK_END_VOID
}

void ffk_config_set_font_directory_list(
    const char *const *font_directories, size_t font_directory_count,
    const char *const *mapping_keys, const char *const *mapping_values,
    size_t mapping_count) {
    FFK_BEGIN
    NSArray *directories =
        ffkStrings(font_directories, font_directory_count, "Font directory");
    if (directories == nil) {
        return;
    }
    NSDictionary *mapping =
        ffkStringMap(mapping_keys, mapping_values, mapping_count);
    if (mapping == nil) {
        return;
    }
    [FFmpegKitConfig setFontDirectoryList:directories with:mapping];
    FFK_END_VOID
}

char *ffk_config_register_new_ffmpeg_pipe(void) {
    FFK_BEGIN
    return ffkCopyString([FFmpegKitConfig registerNewFFmpegPipe]);
    FFK_END(NULL)
}

void ffk_config_close_ffmpeg_pipe(const char *ffmpeg_pipe_path) {
    FFK_BEGIN
    NSString *path = ffkText(ffmpeg_pipe_path, "pipe path");
    if (path == nil) {
        return;
    }
    [FFmpegKitConfig closeFFmpegPipe:path];
    FFK_END_VOID
}

long ffk_config_register_ffmpegkit_input_buffer(const uint8_t *data,
                                                size_t size) {
    FFK_BEGIN
    return [FFmpegKitConfig registerFFmpegKitInputBufferWithBytes:data
                                                           length:size];
    FFK_END(0)
}

long ffk_config_register_ffmpegkit_output_buffer(long initial_capacity,
                                                 long max_capacity) {
    FFK_BEGIN
    return [FFmpegKitConfig registerFFmpegKitOutputBuffer:initial_capacity
                                              maxCapacity:max_capacity];
    FFK_END(0)
}

long ffk_config_get_ffmpegkit_buffer_size(long buffer_id) {
    FFK_BEGIN
    return [FFmpegKitConfig getFFmpegKitBufferSize:buffer_id];
    FFK_END(-1)
}

int ffk_config_get_ffmpegkit_output_buffer(long buffer_id, uint8_t **data,
                                           size_t *size) {
    FFK_BEGIN
    NSData *output = [FFmpegKitConfig getFFmpegKitOutputBuffer:buffer_id];
    if (output == nil) {
        return 0;
    }
    size_t length = 0;
    uint8_t *copy = ffkCopyBytes(output, &length);
    if (copy == NULL) {
        return 0;
    }
    if (data != NULL) {
        *data = copy;
    } else {
        free(copy);
    }
    if (size != NULL) {
        *size = length;
    }
    return 1;
    FFK_END(0)
}

void ffk_config_unregister_ffmpegkit_buffer(long buffer_id) {
    FFK_BEGIN
    [FFmpegKitConfig unregisterFFmpegKitBuffer:buffer_id];
    FFK_END_VOID
}

long ffk_config_register_ffmpegkit_stream(long capacity, int type) {
    FFK_BEGIN
    return [FFmpegKitConfig registerFFmpegKitStream:capacity type:type];
    FFK_END(0)
}

int ffk_config_write_ffmpegkit_stream(long stream_id, const uint8_t *data,
                                      size_t length, int timeout_ms) {
    FFK_BEGIN
    // No copy: the write completes before this call returns
    NSData *bytes = data == NULL && length == 0
                        ? [NSData data]
                        : (data == NULL
                               ? nil
                               : [NSData dataWithBytesNoCopy:(void *)data
                                                      length:length
                                                freeWhenDone:NO]);
    return [FFmpegKitConfig writeFFmpegKitStream:stream_id
                                            data:bytes
                                          offset:0
                                          length:length
                                         timeout:timeout_ms];
    FFK_END(-1)
}

int ffk_config_read_ffmpegkit_stream(long stream_id, int max_bytes,
                                     int timeout_ms, uint8_t **data,
                                     size_t *size) {
    FFK_BEGIN
    NSData *chunk = [FFmpegKitConfig readFFmpegKitStream:stream_id
                                                maxBytes:max_bytes
                                                 timeout:timeout_ms];
    if (chunk == nil) {
        return 0;
    }
    size_t length = 0;
    uint8_t *copy = ffkCopyBytes(chunk, &length);
    if (copy == NULL) {
        return 0;
    }
    if (data != NULL) {
        *data = copy;
    } else {
        free(copy);
    }
    if (size != NULL) {
        *size = length;
    }
    return 1;
    FFK_END(0)
}

void ffk_config_close_ffmpegkit_stream_input(long stream_id) {
    FFK_BEGIN
    [FFmpegKitConfig closeFFmpegKitStreamInput:stream_id];
    FFK_END_VOID
}

void ffk_config_unregister_ffmpegkit_stream(long stream_id) {
    FFK_BEGIN
    [FFmpegKitConfig unregisterFFmpegKitStream:stream_id];
    FFK_END_VOID
}

char *ffk_protocol_build_url(const char *protocol, long id,
                             const char *extension) {
    FFK_BEGIN
    NSString *name = ffkText(protocol, "protocol");
    if (name == nil) {
        return NULL;
    }
    // No extension is not the same as an empty one: it becomes ".bin" either way
    NSString *suffix = extension == NULL ? nil : ffkText(extension, "extension");
    if (extension != NULL && suffix == nil) {
        return NULL;
    }
    return ffkCopyString([FFmpegKitInputBuffer urlWithProtocol:name
                                                    resourceId:id
                                                     extension:suffix]);
    FFK_END(NULL)
}

char *ffk_config_get_ffmpeg_version(void) {
    FFK_BEGIN
    return ffkCopyString([FFmpegKitConfig getFFmpegVersion]);
    FFK_END(NULL)
}

char *ffk_config_get_version(void) {
    FFK_BEGIN
    return ffkCopyString([FFmpegKitConfig getVersion]);
    FFK_END(NULL)
}

int ffk_config_is_lts_build(void) {
    FFK_BEGIN
    return [FFmpegKitConfig isLTSBuild];
    FFK_END(0)
}

char *ffk_config_get_build_date(void) {
    FFK_BEGIN
    return ffkCopyString([FFmpegKitConfig getBuildDate]);
    FFK_END(NULL)
}

int ffk_config_set_environment_variable(const char *variable_name,
                                        const char *variable_value) {
    FFK_BEGIN
    NSString *name = ffkText(variable_name, "variable name");
    NSString *value = name == nil ? nil : ffkText(variable_value, "variable value");
    if (name == nil || value == nil) {
        return -1;
    }
    return [FFmpegKitConfig setEnvironmentVariable:name value:value];
    FFK_END(-1)
}

void ffk_config_ignore_signal(int signal) {
    FFK_BEGIN
    Signal value;
    if (!ffkSignal(signal, &value)) {
        return;
    }
    [FFmpegKitConfig ignoreSignal:value];
    FFK_END_VOID
}

void ffk_config_ffmpeg_execute(FFKSession *session) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    if (ffmpegSession != nil) {
        [FFmpegKitConfig ffmpegExecute:ffmpegSession];
    }
    FFK_END_VOID
}

void ffk_config_ffprobe_execute(FFKSession *session) {
    FFK_BEGIN
    FFprobeSession *ffprobeSession = ffkFFprobeSession(session);
    if (ffprobeSession != nil) {
        [FFmpegKitConfig ffprobeExecute:ffprobeSession];
    }
    FFK_END_VOID
}

void ffk_config_get_media_information_execute(FFKSession *session,
                                              int wait_timeout) {
    FFK_BEGIN
    MediaInformationSession *mediaSession = ffkMediaInformationSession(session);
    if (mediaSession != nil) {
        [FFmpegKitConfig getMediaInformationExecute:mediaSession
                                        withTimeout:wait_timeout];
    }
    FFK_END_VOID
}

void ffk_config_async_ffmpeg_execute(FFKSession *session) {
    FFK_BEGIN
    FFmpegSession *ffmpegSession = ffkFFmpegSession(session);
    if (ffmpegSession != nil) {
        [FFmpegKitConfig asyncFFmpegExecute:ffmpegSession];
    }
    FFK_END_VOID
}

void ffk_config_async_ffprobe_execute(FFKSession *session) {
    FFK_BEGIN
    FFprobeSession *ffprobeSession = ffkFFprobeSession(session);
    if (ffprobeSession != nil) {
        [FFmpegKitConfig asyncFFprobeExecute:ffprobeSession];
    }
    FFK_END_VOID
}

void ffk_config_async_get_media_information_execute(FFKSession *session,
                                                    int wait_timeout) {
    FFK_BEGIN
    MediaInformationSession *mediaSession = ffkMediaInformationSession(session);
    if (mediaSession != nil) {
        [FFmpegKitConfig asyncGetMediaInformationExecute:mediaSession
                                             withTimeout:wait_timeout];
    }
    FFK_END_VOID
}

void ffk_config_enable_log_callback(ffk_log_cb callback, void *user_data,
                                    ffk_free_cb free_user_data) {
    FFK_BEGIN
    [FFmpegKitConfig
        enableLogCallback:ffkLogBlock(callback, user_data, free_user_data)];
    FFK_END_VOID
}

void ffk_config_enable_statistics_callback(ffk_statistics_cb callback,
                                           void *user_data,
                                           ffk_free_cb free_user_data) {
    FFK_BEGIN
    [FFmpegKitConfig enableStatisticsCallback:ffkStatisticsBlock(
                                                  callback, user_data,
                                                  free_user_data)];
    FFK_END_VOID
}

void ffk_config_enable_ffmpeg_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data) {
    FFK_BEGIN
    [FFmpegKitConfig enableFFmpegSessionCompleteCallback:ffkFFmpegCompleteBlock(
                                                            callback, user_data,
                                                            free_user_data)];
    FFK_END_VOID
}

int ffk_config_get_ffmpeg_session_complete_callback(ffk_session_cb *callback,
                                                    void **user_data) {
    FFK_BEGIN
    return ffkReadSession([FFmpegKitConfig getFFmpegSessionCompleteCallback],
                          callback, user_data);
    FFK_END(0)
}

void ffk_config_enable_ffprobe_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data) {
    FFK_BEGIN
    [FFmpegKitConfig enableFFprobeSessionCompleteCallback:ffkFFprobeCompleteBlock(
                                                             callback, user_data,
                                                             free_user_data)];
    FFK_END_VOID
}

int ffk_config_get_ffprobe_session_complete_callback(ffk_session_cb *callback,
                                                     void **user_data) {
    FFK_BEGIN
    return ffkReadSession([FFmpegKitConfig getFFprobeSessionCompleteCallback],
                          callback, user_data);
    FFK_END(0)
}

void ffk_config_enable_media_information_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data) {
    FFK_BEGIN
    [FFmpegKitConfig
        enableMediaInformationSessionCompleteCallback:
            ffkMediaInformationCompleteBlock(callback, user_data,
                                             free_user_data)];
    FFK_END_VOID
}

int ffk_config_get_media_information_session_complete_callback(
    ffk_session_cb *callback, void **user_data) {
    FFK_BEGIN
    return ffkReadSession(
        [FFmpegKitConfig getMediaInformationSessionCompleteCallback], callback,
        user_data);
    FFK_END(0)
}

int ffk_config_get_log_level(void) {
    FFK_BEGIN
    return [FFmpegKitConfig getLogLevel];
    FFK_END(0)
}

void ffk_config_set_log_level(int level) {
    FFK_BEGIN
    [FFmpegKitConfig setLogLevel:level];
    FFK_END_VOID
}

char *ffk_config_log_level_to_string(int level) {
    FFK_BEGIN
    return ffkCopyString([FFmpegKitConfig logLevelToString:level]);
    FFK_END(NULL)
}

int ffk_config_get_session_history_size(void) {
    FFK_BEGIN
    return [FFmpegKitConfig getSessionHistorySize];
    FFK_END(0)
}

void ffk_config_set_session_history_size(int session_history_size) {
    FFK_BEGIN
    [FFmpegKitConfig setSessionHistorySize:session_history_size];
    FFK_END_VOID
}

FFKSession *ffk_config_get_session(long session_id) {
    FFK_BEGIN
    return (FFKSession *)ffkRetain([FFmpegKitConfig getSession:session_id]);
    FFK_END(NULL)
}

void ffk_config_delete_session(long session_id) {
    FFK_BEGIN
    [FFmpegKitConfig deleteSession:session_id];
    FFK_END_VOID
}

long ffk_config_add_session_delete_listener(ffk_session_delete_cb callback,
                                            void *user_data,
                                            ffk_free_cb free_user_data) {
    FFK_BEGIN
    FFKCookie *cookie = [[FFKCookie alloc] initWithFunction:(void *)callback
                                                       data:user_data
                                                       free:free_user_data];
    if (callback == NULL) {
        return 0;
    }
    FFKSessionDeleteListener *listener =
        [[FFKSessionDeleteListener alloc] initWithCallback:callback
                                                    cookie:cookie];
    [FFmpegKitConfig addSessionDeleteListener:listener];

    [ffkListenerLock() lock];
    const long token = ++ffkNextListenerToken;
    ffkListeners()[@(token)] = listener;
    [ffkListenerLock() unlock];
    return token;
    FFK_END(0)
}

void ffk_config_remove_session_delete_listener(long token) {
    FFK_BEGIN
    FFKSessionDeleteListener *listener = nil;
    [ffkListenerLock() lock];
    listener = ffkListeners()[@(token)];
    [ffkListeners() removeObjectForKey:@(token)];
    [ffkListenerLock() unlock];

    if (listener != nil) {
        [FFmpegKitConfig removeSessionDeleteListener:listener];
    }
    FFK_END_VOID
}

FFKSession *ffk_config_get_last_session(void) {
    FFK_BEGIN
    return (FFKSession *)ffkRetain([FFmpegKitConfig getLastSession]);
    FFK_END(NULL)
}

FFKSession *ffk_config_get_last_completed_session(void) {
    FFK_BEGIN
    return (FFKSession *)ffkRetain([FFmpegKitConfig getLastCompletedSession]);
    FFK_END(NULL)
}

FFKSessionList *ffk_config_get_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList([FFmpegKitConfig getSessions]);
    FFK_END(NULL)
}

void ffk_config_clear_sessions(void) {
    FFK_BEGIN
    [FFmpegKitConfig clearSessions];
    FFK_END_VOID
}

FFKSessionList *ffk_config_get_ffmpeg_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList([FFmpegKitConfig getFFmpegSessions]);
    FFK_END(NULL)
}

FFKSessionList *ffk_config_get_ffprobe_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList([FFmpegKitConfig getFFprobeSessions]);
    FFK_END(NULL)
}

FFKSessionList *ffk_config_get_media_information_sessions(void) {
    FFK_BEGIN
    return (FFKSessionList *)ffkRetainList(
        [FFmpegKitConfig getMediaInformationSessions]);
    FFK_END(NULL)
}

FFKSessionList *ffk_config_get_sessions_by_state(int state) {
    FFK_BEGIN
    SessionState value;
    if (!ffkSessionState(state, &value)) {
        return NULL;
    }
    return (FFKSessionList *)ffkRetainList(
        [FFmpegKitConfig getSessionsByState:value]);
    FFK_END(NULL)
}

int ffk_config_get_log_redirection_strategy(void) {
    FFK_BEGIN
    return (int)[FFmpegKitConfig getLogRedirectionStrategy];
    FFK_END(0)
}

void ffk_config_set_log_redirection_strategy(int strategy) {
    FFK_BEGIN
    LogRedirectionStrategy value;
    if (!ffkLogRedirectionStrategy(strategy, &value)) {
        return;
    }
    [FFmpegKitConfig setLogRedirectionStrategy:value];
    FFK_END_VOID
}

int ffk_config_messages_in_transmit(long session_id) {
    FFK_BEGIN
    return [FFmpegKitConfig messagesInTransmit:session_id];
    FFK_END(0)
}

char *ffk_config_session_state_to_string(int state) {
    FFK_BEGIN
    SessionState value;
    if (!ffkSessionState(state, &value)) {
        return NULL;
    }
    return ffkCopyString([FFmpegKitConfig sessionStateToString:value]);
    FFK_END(NULL)
}

FFKStringList *ffk_config_parse_arguments(const char *command) {
    FFK_BEGIN
    NSString *text = ffkText(command, "command");
    if (text == nil) {
        return NULL;
    }
    return (FFKStringList *)ffkRetainList([FFmpegKitConfig parseArguments:text]);
    FFK_END(NULL)
}

char *ffk_config_arguments_to_string(const char *const *arguments,
                                     size_t argument_count) {
    FFK_BEGIN
    NSArray *list = ffkArguments(arguments, argument_count);
    if (list == nil) {
        return NULL;
    }
    return ffkCopyString([FFmpegKitConfig argumentsToString:list]);
    FFK_END(NULL)
}

/* ------------------------------------------------------------------------ */
/* Packages and architecture                                                 */
/* ------------------------------------------------------------------------ */

char *ffk_packages_get_package_name(void) {
    FFK_BEGIN
    return ffkCopyString([Packages getPackageName]);
    FFK_END(NULL)
}

FFKStringList *ffk_packages_get_external_libraries(void) {
    FFK_BEGIN
    NSArray *libraries = [Packages getExternalLibraries];
    return (FFKStringList *)ffkRetainList(libraries != nil ? libraries : @[]);
    FFK_END(NULL)
}

char *ffk_arch_detect_get_arch(void) {
    FFK_BEGIN
    return ffkCopyString([ArchDetect getArch]);
    FFK_END(NULL)
}
