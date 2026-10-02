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

#ifndef FFMPEG_KIT_C_H
#define FFMPEG_KIT_C_H

/*
 * FFmpegKitNext flat C API.
 *
 * This header is the C ABI of libffmpegkit: extern "C" functions over opaque
 * handles, plain integers and UTF-8 strings. No C++ type ever crosses this
 * boundary, so the library can be used from C and from any language with a C
 * foreign function interface.
 *
 * On Linux this API is a second entry point into the library. The object
 * oriented C++ API (ffmpegkit::FFmpegKit and friends) stays public: its
 * headers are installed and libffmpegkit exports its classes, so C++
 * consumers can keep using it directly. This API is implemented on top of
 * those same classes, and the functions, types and rules declared here are the
 * same as on Windows.
 *
 * MEMORY OWNERSHIP
 *
 *   - The library owns every allocation it hands out and every allocation is
 *     released by an ffk_*_free function from this header. Callers must never
 *     free returned memory themselves, so the same ownership rules apply on
 *     every platform.
 *   - Every `char *` returned by this API is a heap allocated UTF-8 string
 *     owned by the caller and must be released with ffk_string_free(). A NULL
 *     return means the value is absent, never that an empty string was
 *     returned. `const char *` parameters are borrowed for the duration of the
 *     call and are never retained.
 *   - Every `uint8_t *` returned through an out parameter must be released
 *     with ffk_bytes_free().
 *   - Handles returned by this API are owned by the caller and must be
 *     released with the matching ffk_*_free function. Handles passed into
 *     callbacks are owned by the library and are only valid for the duration
 *     of the callback; use ffk_session_retain() to keep one alive longer.
 *   - Passing NULL for a handle is allowed: getters return 0 or NULL and
 *     ffk_*_free functions do nothing. Passing a handle of the wrong kind, or
 *     one that has already been released, is undefined behavior.
 *
 * TEXT
 *
 *   Text is UTF-8, which is not checked. A NULL text is the empty text, a NULL
 *   entry of an array of texts is an empty text, a NULL key of a key/value
 *   mapping is skipped and a NULL value of one is empty.
 *
 * ERROR HANDLING
 *
 *   No C++ exception ever escapes this API. Every entry point clears the
 *   calling thread's error slot on entry and, if the operation throws, stores
 *   the message there and returns a neutral value (0, NULL or no-op). Callers
 *   check ffk_has_error() and take the message with ffk_take_error(). The
 *   message can be of any length.
 *
 * CALLBACKS
 *
 *   Callbacks are a C function pointer plus a `void *user_data` cookie plus an
 *   ffk_free_cb that the library calls when it drops its last reference to the
 *   cookie. Since the consumer supplies the free function, the cookie is
 *   always released by the allocator that created it. The library takes
 *   ownership of the cookie as soon as a call that accepts callbacks starts,
 *   even if that call fails or the callback is NULL, in which case the cookie
 *   is released before the call returns. Callbacks of asynchronous executions
 *   run on threads owned by the library. Nothing is promised about which thread
 *   that is, and asynchronous executions may run at the same time, so a
 *   callback has to be safe to run on any thread, next to another one. The
 *   *_get_*_callback() functions read back only callbacks that were registered
 *   through this API.
 *
 * METADATA
 *
 *   Media, stream and chapter properties are exchanged as JSON text: object
 *   keys are sorted and slashes are not escaped. Metadata given as JSON text
 *   has to be a JSON object; anything else leaves an object without
 *   properties.
 *
 *   A number property is a JSON integer that fits in 64 bits. A fraction, an
 *   exponent (5.0 and 1e2 are doubles although they are whole), a boolean, null
 *   and an integer outside the range are not numbers, but they are still there
 *   as JSON text.
 *
 * ENUMS
 *
 *   Enumerations (session state, log level, log redirection strategy, signal)
 *   cross this boundary as plain int, never as a C++ enum type.
 */

#include <stddef.h>
#include <stdint.h>

#define FFK_API __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Opaque handles                                                            */
/* ------------------------------------------------------------------------ */

typedef struct FFKSession FFKSession;
typedef struct FFKLog FFKLog;
typedef struct FFKStatistics FFKStatistics;
typedef struct FFKMediaInformation FFKMediaInformation;
typedef struct FFKStreamInformation FFKStreamInformation;
typedef struct FFKChapter FFKChapter;

typedef struct FFKStringList FFKStringList;
typedef struct FFKSessionList FFKSessionList;
typedef struct FFKLogList FFKLogList;
typedef struct FFKStatisticsList FFKStatisticsList;
typedef struct FFKStreamInformationList FFKStreamInformationList;
typedef struct FFKChapterList FFKChapterList;

/* ------------------------------------------------------------------------ */
/* Callback types                                                            */
/* ------------------------------------------------------------------------ */

/** Releases a callback cookie. Called by the library, runs consumer code. */
typedef void (*ffk_free_cb)(void *user_data);

typedef void (*ffk_log_cb)(FFKLog *log, void *user_data);
typedef void (*ffk_statistics_cb)(FFKStatistics *statistics, void *user_data);
typedef void (*ffk_session_cb)(FFKSession *session, void *user_data);
typedef void (*ffk_session_delete_cb)(long session_id, void *user_data);

/* ------------------------------------------------------------------------ */
/* Errors and memory                                                         */
/* ------------------------------------------------------------------------ */

/** Returns 1 when the last call on this thread failed, 0 otherwise. */
FFK_API int ffk_has_error(void);

/**
 * Takes the error message left by the last failed call on this thread and
 * clears the slot.
 *
 * @return error message to release with ffk_string_free(), or NULL
 */
FFK_API char *ffk_take_error(void);

/** Clears the calling thread's error slot. */
FFK_API void ffk_clear_error(void);

/** Releases a string returned by this API. NULL is ignored. */
FFK_API void ffk_string_free(char *value);

/** Releases a byte buffer returned by this API. NULL is ignored. */
FFK_API void ffk_bytes_free(uint8_t *data);

/* ------------------------------------------------------------------------ */
/* Lists                                                                     */
/* ------------------------------------------------------------------------ */

FFK_API size_t ffk_string_list_size(const FFKStringList *list);
/** @return element to release with ffk_string_free(), or NULL when out of range */
FFK_API char *ffk_string_list_get(const FFKStringList *list, size_t index);
FFK_API void ffk_string_list_free(FFKStringList *list);

FFK_API size_t ffk_session_list_size(const FFKSessionList *list);
/** @return element to release with ffk_session_free(), or NULL when out of range */
FFK_API FFKSession *ffk_session_list_get(const FFKSessionList *list,
                                         size_t index);
FFK_API void ffk_session_list_free(FFKSessionList *list);

FFK_API size_t ffk_log_list_size(const FFKLogList *list);
/** @return element to release with ffk_log_free(), or NULL when out of range */
FFK_API FFKLog *ffk_log_list_get(const FFKLogList *list, size_t index);
FFK_API void ffk_log_list_free(FFKLogList *list);

FFK_API size_t ffk_statistics_list_size(const FFKStatisticsList *list);
/** @return element to release with ffk_statistics_free(), or NULL when out of range */
FFK_API FFKStatistics *ffk_statistics_list_get(const FFKStatisticsList *list,
                                               size_t index);
FFK_API void ffk_statistics_list_free(FFKStatisticsList *list);

FFK_API size_t
ffk_stream_information_list_size(const FFKStreamInformationList *list);
/** @return element to release with ffk_stream_information_free(), or NULL */
FFK_API FFKStreamInformation *
ffk_stream_information_list_get(const FFKStreamInformationList *list,
                                size_t index);
FFK_API void
ffk_stream_information_list_free(FFKStreamInformationList *list);

FFK_API size_t ffk_chapter_list_size(const FFKChapterList *list);
/** @return element to release with ffk_chapter_free(), or NULL when out of range */
FFK_API FFKChapter *ffk_chapter_list_get(const FFKChapterList *list,
                                         size_t index);
FFK_API void ffk_chapter_list_free(FFKChapterList *list);

/* ------------------------------------------------------------------------ */
/* Log                                                                       */
/* ------------------------------------------------------------------------ */

FFK_API FFKLog *ffk_log_create(long session_id, int level,
                               const char *message);
FFK_API void ffk_log_free(FFKLog *log);
FFK_API long ffk_log_get_session_id(const FFKLog *log);
FFK_API int ffk_log_get_level(const FFKLog *log);
FFK_API char *ffk_log_get_message(const FFKLog *log);

/* ------------------------------------------------------------------------ */
/* Statistics                                                                */
/* ------------------------------------------------------------------------ */

FFK_API FFKStatistics *
ffk_statistics_create(long session_id, int video_frame_number, float video_fps,
                      float video_quality, int64_t size, double time,
                      double bitrate, double speed);
FFK_API void ffk_statistics_free(FFKStatistics *statistics);
FFK_API long ffk_statistics_get_session_id(const FFKStatistics *statistics);
FFK_API int
ffk_statistics_get_video_frame_number(const FFKStatistics *statistics);
FFK_API float ffk_statistics_get_video_fps(const FFKStatistics *statistics);
FFK_API float
ffk_statistics_get_video_quality(const FFKStatistics *statistics);
FFK_API int64_t ffk_statistics_get_size(const FFKStatistics *statistics);
FFK_API double ffk_statistics_get_time(const FFKStatistics *statistics);
FFK_API double ffk_statistics_get_bitrate(const FFKStatistics *statistics);
FFK_API double ffk_statistics_get_speed(const FFKStatistics *statistics);

/* ------------------------------------------------------------------------ */
/* Session                                                                   */
/* ------------------------------------------------------------------------ */

/**
 * Creates a plain session, the base every kit session is built on.
 *
 * @param log_redirection_strategy strategy as int, or -1 to use the value
 * configured with ffk_config_set_log_redirection_strategy()
 */
FFK_API FFKSession *ffk_abstract_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    int log_redirection_strategy);

/**
 * Creates an FFmpeg session.
 *
 * @param log_redirection_strategy strategy as int, or -1 to use the value
 * configured with ffk_config_set_log_redirection_strategy()
 */
FFK_API FFKSession *ffk_ffmpeg_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, ffk_statistics_cb statistics_callback,
    void *statistics_user_data, ffk_free_cb statistics_free,
    int log_redirection_strategy);

FFK_API FFKSession *ffk_ffprobe_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, int log_redirection_strategy);

FFK_API FFKSession *ffk_media_information_session_create(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free);

/** Releases a session handle. The session itself stays in the history. */
FFK_API void ffk_session_free(FFKSession *session);

/** @return a new handle for the same session, to release with ffk_session_free() */
FFK_API FFKSession *ffk_session_retain(const FFKSession *session);

FFK_API long ffk_session_get_session_id(const FFKSession *session);
/** @return creation time as milliseconds since the epoch */
FFK_API int64_t ffk_session_get_create_time(const FFKSession *session);
/** @return start time as milliseconds since the epoch */
FFK_API int64_t ffk_session_get_start_time(const FFKSession *session);
/** @return end time as milliseconds since the epoch */
FFK_API int64_t ffk_session_get_end_time(const FFKSession *session);
FFK_API long ffk_session_get_duration(const FFKSession *session);
FFK_API FFKStringList *ffk_session_get_arguments(const FFKSession *session);
FFK_API char *ffk_session_get_command(const FFKSession *session);
FFK_API FFKLogList *
ffk_session_get_all_logs_with_timeout(const FFKSession *session,
                                      int wait_timeout);
FFK_API FFKLogList *ffk_session_get_all_logs(const FFKSession *session);
FFK_API FFKLogList *ffk_session_get_logs(const FFKSession *session);
FFK_API char *
ffk_session_get_all_logs_as_string_with_timeout(const FFKSession *session,
                                                int wait_timeout);
FFK_API char *ffk_session_get_all_logs_as_string(const FFKSession *session);
FFK_API char *ffk_session_get_logs_as_string(const FFKSession *session);
FFK_API char *ffk_session_get_output(const FFKSession *session);
FFK_API int ffk_session_get_state(const FFKSession *session);
/**
 * @param value receives the return code value when one exists
 * @return 1 when the session has a return code, 0 otherwise
 */
FFK_API int ffk_session_get_return_code(const FFKSession *session, int *value);
/**
 * @return the description of the failure, or NULL when the session has not
 * failed. Despite the name it is not always a stack trace: the format is
 * platform specific.
 */
FFK_API char *ffk_session_get_fail_stack_trace(const FFKSession *session);
FFK_API int
ffk_session_get_log_redirection_strategy(const FFKSession *session);
FFK_API int ffk_session_there_are_asynchronous_messages_in_transmit(
    const FFKSession *session);
FFK_API void
ffk_session_wait_for_asynchronous_messages_in_transmit(const FFKSession *session,
                                                       int timeout);
FFK_API void ffk_session_add_log(FFKSession *session, long log_session_id,
                                 int level, const char *message);
FFK_API void ffk_session_start_running(FFKSession *session);
FFK_API void ffk_session_complete(FFKSession *session, int return_code);
/**
 * Marks the session as failed. ffk_session_get_fail_stack_trace() describes the
 * failure and always contains the text; its format is platform specific, and
 * here it is the text itself.
 */
FFK_API void ffk_session_fail(FFKSession *session, const char *error);
FFK_API int ffk_session_is_ffmpeg(const FFKSession *session);
FFK_API int ffk_session_is_ffprobe(const FFKSession *session);
FFK_API int ffk_session_is_media_information(const FFKSession *session);
FFK_API void ffk_session_cancel(FFKSession *session);

/**
 * Reads back the log callback registered for this session.
 *
 * Only callbacks registered through this C API can be read back. A callback
 * set through the C++ API is reported as not registered. The returned user_data
 * is borrowed and stays valid for as long as the session holds the callback.
 *
 * @return 1 when a callback registered through this API is present, 0 otherwise
 */
FFK_API int ffk_session_get_log_callback(const FFKSession *session,
                                         ffk_log_cb *callback,
                                         void **user_data);

/** Same rules as ffk_session_get_log_callback(). */
FFK_API int ffk_session_get_complete_callback(const FFKSession *session,
                                              ffk_session_cb *callback,
                                              void **user_data);

/* ---- FFmpeg session specific -------------------------------------------- */

FFK_API FFKStatisticsList *
ffk_ffmpeg_session_get_all_statistics_with_timeout(FFKSession *session,
                                                   int wait_timeout);
FFK_API FFKStatisticsList *
ffk_ffmpeg_session_get_all_statistics(FFKSession *session);
FFK_API FFKStatisticsList *
ffk_ffmpeg_session_get_statistics(FFKSession *session);
FFK_API FFKStatistics *
ffk_ffmpeg_session_get_last_received_statistics(FFKSession *session);
FFK_API void ffk_ffmpeg_session_add_statistics(FFKSession *session,
                                               const FFKStatistics *statistics);
/** Same rules as ffk_session_get_log_callback(). */
FFK_API int
ffk_ffmpeg_session_get_statistics_callback(const FFKSession *session,
                                           ffk_statistics_cb *callback,
                                           void **user_data);

/* ---- media information session specific --------------------------------- */

FFK_API FFKMediaInformation *
ffk_media_information_session_get_media_information(FFKSession *session);
FFK_API void ffk_media_information_session_set_media_information(
    FFKSession *session, FFKMediaInformation *media_information);

/* ------------------------------------------------------------------------ */
/* Chapter                                                                   */
/* ------------------------------------------------------------------------ */

/**
 * @param value_json chapter properties as a JSON object; anything else is
 * treated as no properties
 */
FFK_API FFKChapter *ffk_chapter_create(const char *value_json);
FFK_API void ffk_chapter_free(FFKChapter *chapter);
/**
 * @param value receives the property, and is left alone when there is none
 * @return 1 when the property is a JSON integer that fits in 64 bits, 0
 * otherwise
 */
FFK_API int ffk_chapter_get_number_property(FFKChapter *chapter,
                                            const char *key, int64_t *value);
FFK_API char *ffk_chapter_get_string_property(FFKChapter *chapter,
                                              const char *key);
/** @return the property as JSON text, or NULL when it does not exist */
FFK_API char *ffk_chapter_get_property_json(FFKChapter *chapter,
                                            const char *key);
/** @return every property as JSON text, or NULL */
FFK_API char *ffk_chapter_get_all_properties_json(FFKChapter *chapter);

/* ------------------------------------------------------------------------ */
/* Stream information                                                        */
/* ------------------------------------------------------------------------ */

/**
 * @param value_json stream properties as a JSON object; anything else is
 * treated as no properties
 */
FFK_API FFKStreamInformation *
ffk_stream_information_create(const char *value_json);
FFK_API void ffk_stream_information_free(FFKStreamInformation *stream);
/**
 * @param value receives the property, and is left alone when there is none
 * @return 1 when the property is a JSON integer that fits in 64 bits, 0
 * otherwise
 */
FFK_API int ffk_stream_information_get_number_property(
    FFKStreamInformation *stream, const char *key, int64_t *value);
FFK_API char *
ffk_stream_information_get_string_property(FFKStreamInformation *stream,
                                           const char *key);
/** @return the property as JSON text, or NULL when it does not exist */
FFK_API char *
ffk_stream_information_get_property_json(FFKStreamInformation *stream,
                                         const char *key);
/** @return every property as JSON text, or NULL */
FFK_API char *
ffk_stream_information_get_all_properties_json(FFKStreamInformation *stream);

/* ------------------------------------------------------------------------ */
/* Media information                                                         */
/* ------------------------------------------------------------------------ */

/**
 * @param value_json the ffprobe JSON object; its "format" object holds the
 * format properties. Anything that is not a JSON object is treated as no
 * properties.
 * @param stream_json per stream properties as JSON text
 * @param chapter_json per chapter properties as JSON text
 */
FFK_API FFKMediaInformation *
ffk_media_information_create(const char *value_json,
                             const char *const *stream_json,
                             size_t stream_count,
                             const char *const *chapter_json,
                             size_t chapter_count);
FFK_API void ffk_media_information_free(FFKMediaInformation *media_information);
FFK_API FFKStreamInformationList *
ffk_media_information_get_streams(FFKMediaInformation *media_information);
FFK_API FFKChapterList *
ffk_media_information_get_chapters(FFKMediaInformation *media_information);
/**
 * @param value receives the property, and is left alone when there is none
 * @return 1 when the property is a JSON integer that fits in 64 bits, 0
 * otherwise
 */
FFK_API int
ffk_media_information_get_number_property(FFKMediaInformation *media_information,
                                          const char *key, int64_t *value);
FFK_API char *
ffk_media_information_get_string_property(FFKMediaInformation *media_information,
                                          const char *key);
/** @return the property as JSON text, or NULL when it does not exist */
FFK_API char *
ffk_media_information_get_property_json(FFKMediaInformation *media_information,
                                        const char *key);
/**
 * @param value receives the property, and is left alone when there is none
 * @return 1 when the format property is a JSON integer that fits in 64 bits, 0
 * otherwise
 */
FFK_API int ffk_media_information_get_number_format_property(
    FFKMediaInformation *media_information, const char *key, int64_t *value);
FFK_API char *ffk_media_information_get_string_format_property(
    FFKMediaInformation *media_information, const char *key);
/** @return the format property as JSON text, or NULL when it does not exist */
FFK_API char *ffk_media_information_get_format_property_json(
    FFKMediaInformation *media_information, const char *key);
/** @return the format properties as JSON text, or NULL */
FFK_API char *ffk_media_information_get_format_properties_json(
    FFKMediaInformation *media_information);
/** @return every property as JSON text, or NULL */
FFK_API char *ffk_media_information_get_all_properties_json(
    FFKMediaInformation *media_information);

/**
 * @return the parsed ffprobe output, or NULL when it cannot be parsed. Nothing
 * is printed and the error slot is left alone.
 */
FFK_API FFKMediaInformation *
ffk_media_information_parser_from(const char *ffprobe_json_output);
/**
 * Like ffk_media_information_parser_from(), but a parse failure also fills the
 * error slot with the parser's own description of it, whose wording is
 * platform specific.
 */
FFK_API FFKMediaInformation *
ffk_media_information_parser_from_with_error(const char *ffprobe_json_output);

/* ------------------------------------------------------------------------ */
/* FFmpegKit                                                                 */
/* ------------------------------------------------------------------------ */

FFK_API FFKSession *
ffk_ffmpegkit_execute_with_arguments(const char *const *arguments,
                                     size_t argument_count);
FFK_API FFKSession *ffk_ffmpegkit_execute_with_arguments_async(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free, ffk_statistics_cb statistics_callback,
    void *statistics_user_data, ffk_free_cb statistics_free);
FFK_API FFKSession *ffk_ffmpegkit_execute(const char *command);
FFK_API FFKSession *ffk_ffmpegkit_execute_async(
    const char *command, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    ffk_statistics_cb statistics_callback, void *statistics_user_data,
    ffk_free_cb statistics_free);
FFK_API void ffk_ffmpegkit_cancel(void);
FFK_API void ffk_ffmpegkit_cancel_session(long session_id);
FFK_API FFKSessionList *ffk_ffmpegkit_list_sessions(void);

/* ------------------------------------------------------------------------ */
/* FFprobeKit                                                                */
/* ------------------------------------------------------------------------ */

FFK_API FFKSession *
ffk_ffprobekit_execute_with_arguments(const char *const *arguments,
                                      size_t argument_count);
FFK_API FFKSession *ffk_ffprobekit_execute_with_arguments_async(
    const char *const *arguments, size_t argument_count,
    ffk_session_cb complete_callback, void *complete_user_data,
    ffk_free_cb complete_free, ffk_log_cb log_callback, void *log_user_data,
    ffk_free_cb log_free);
FFK_API FFKSession *ffk_ffprobekit_execute(const char *command);
FFK_API FFKSession *ffk_ffprobekit_execute_async(
    const char *command, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free);
FFK_API FFKSession *
ffk_ffprobekit_get_media_information(const char *path, int wait_timeout);
FFK_API FFKSession *ffk_ffprobekit_get_media_information_async(
    const char *path, ffk_session_cb complete_callback,
    void *complete_user_data, ffk_free_cb complete_free,
    ffk_log_cb log_callback, void *log_user_data, ffk_free_cb log_free,
    int wait_timeout);
FFK_API FFKSession *
ffk_ffprobekit_get_media_information_from_command(const char *command);
FFK_API FFKSessionList *ffk_ffprobekit_list_ffprobe_sessions(void);
FFK_API FFKSessionList *ffk_ffprobekit_list_media_information_sessions(void);

/* ------------------------------------------------------------------------ */
/* FFmpegKitConfig                                                           */
/* ------------------------------------------------------------------------ */

FFK_API void ffk_config_enable_redirection(void);
FFK_API void ffk_config_disable_redirection(void);
FFK_API int ffk_config_set_fontconfig_configuration_path(const char *path);
FFK_API void ffk_config_set_font_directory(const char *font_directory_path,
                                           const char *const *mapping_keys,
                                           const char *const *mapping_values,
                                           size_t mapping_count);
FFK_API void ffk_config_set_font_directory_list(
    const char *const *font_directories, size_t font_directory_count,
    const char *const *mapping_keys, const char *const *mapping_values,
    size_t mapping_count);

/**
 * Creates a new named pipe (FIFO) under $HOME/.cache/ffmpegkit/pipes, falling
 * back to $TMPDIR and then the current directory when HOME is not set.
 *
 * The caller is responsible for closing the pipe with
 * ffk_config_close_ffmpeg_pipe().
 *
 * @return full path of the new pipe, to release with ffk_string_free(), or
 * NULL when the pipe could not be created
 *
 * @deprecated Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.
 */
FFK_API char *ffk_config_register_new_ffmpeg_pipe(void);
/**
 * Closes a named pipe created by ffk_config_register_new_ffmpeg_pipe() by
 * removing it from the file system.
 *
 * @deprecated Named pipes are deprecated. Use FFmpegKit input/output buffers and streams instead.
 */
FFK_API void ffk_config_close_ffmpeg_pipe(const char *ffmpeg_pipe_path);

FFK_API long ffk_config_register_ffmpegkit_input_buffer(const uint8_t *data,
                                                        size_t size);
FFK_API long ffk_config_register_ffmpegkit_output_buffer(long initial_capacity,
                                                         long max_capacity);
FFK_API long ffk_config_get_ffmpegkit_buffer_size(long buffer_id);
/**
 * @param data receives a buffer to release with ffk_bytes_free()
 * @param size receives the buffer size
 * @return 1 when a buffer was returned, 0 otherwise
 */
FFK_API int ffk_config_get_ffmpegkit_output_buffer(long buffer_id,
                                                   uint8_t **data,
                                                   size_t *size);
FFK_API void ffk_config_unregister_ffmpegkit_buffer(long buffer_id);

FFK_API long ffk_config_register_ffmpegkit_stream(long capacity, int type);
FFK_API int ffk_config_write_ffmpegkit_stream(long stream_id,
                                              const uint8_t *data,
                                              size_t length, int timeout_ms);
/**
 * @param data receives a buffer to release with ffk_bytes_free()
 * @param size receives the buffer size
 * @return 1 when a buffer was returned, 0 otherwise
 */
FFK_API int ffk_config_read_ffmpegkit_stream(long stream_id, int max_bytes,
                                             int timeout_ms, uint8_t **data,
                                             size_t *size);
FFK_API void ffk_config_close_ffmpegkit_stream_input(long stream_id);
FFK_API void ffk_config_unregister_ffmpegkit_stream(long stream_id);

/**
 * Builds the url that addresses a registered buffer or stream.
 *
 * The ffkitmem and ffkitstream protocol handlers inside the library parse this
 * url, so it is built here rather than in the caller: one spelling, no drift.
 *
 * @param protocol "ffkitmem" or "ffkitstream"
 * @param id buffer or stream id
 * @param extension file extension hint, normalised by the library
 * @return url to release with ffk_string_free()
 */
FFK_API char *ffk_protocol_build_url(const char *protocol, long id,
                                     const char *extension);

FFK_API char *ffk_config_get_ffmpeg_version(void);
FFK_API char *ffk_config_get_version(void);
FFK_API int ffk_config_is_lts_build(void);
FFK_API char *ffk_config_get_build_date(void);
FFK_API int ffk_config_set_environment_variable(const char *variable_name,
                                                const char *variable_value);
FFK_API void ffk_config_ignore_signal(int signal);

FFK_API void ffk_config_ffmpeg_execute(FFKSession *session);
FFK_API void ffk_config_ffprobe_execute(FFKSession *session);
FFK_API void ffk_config_get_media_information_execute(FFKSession *session,
                                                      int wait_timeout);
FFK_API void ffk_config_async_ffmpeg_execute(FFKSession *session);
FFK_API void ffk_config_async_ffprobe_execute(FFKSession *session);
FFK_API void ffk_config_async_get_media_information_execute(FFKSession *session,
                                                            int wait_timeout);

FFK_API void ffk_config_enable_log_callback(ffk_log_cb callback,
                                            void *user_data,
                                            ffk_free_cb free_user_data);
FFK_API void ffk_config_enable_statistics_callback(ffk_statistics_cb callback,
                                                   void *user_data,
                                                   ffk_free_cb free_user_data);
FFK_API void ffk_config_enable_ffmpeg_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data);
/** Same rules as ffk_session_get_log_callback(). */
FFK_API int
ffk_config_get_ffmpeg_session_complete_callback(ffk_session_cb *callback,
                                                void **user_data);
FFK_API void ffk_config_enable_ffprobe_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data);
/** Same rules as ffk_session_get_log_callback(). */
FFK_API int
ffk_config_get_ffprobe_session_complete_callback(ffk_session_cb *callback,
                                                 void **user_data);
FFK_API void ffk_config_enable_media_information_session_complete_callback(
    ffk_session_cb callback, void *user_data, ffk_free_cb free_user_data);
/** Same rules as ffk_session_get_log_callback(). */
FFK_API int ffk_config_get_media_information_session_complete_callback(
    ffk_session_cb *callback, void **user_data);

FFK_API int ffk_config_get_log_level(void);
FFK_API void ffk_config_set_log_level(int level);
FFK_API char *ffk_config_log_level_to_string(int level);

FFK_API int ffk_config_get_session_history_size(void);
FFK_API void ffk_config_set_session_history_size(int session_history_size);
FFK_API FFKSession *ffk_config_get_session(long session_id);
FFK_API void ffk_config_delete_session(long session_id);

/**
 * Registers a session delete listener.
 *
 * @return token to pass to ffk_config_remove_session_delete_listener(), or 0
 * when the listener could not be registered
 */
FFK_API long
ffk_config_add_session_delete_listener(ffk_session_delete_cb callback,
                                       void *user_data,
                                       ffk_free_cb free_user_data);
FFK_API void ffk_config_remove_session_delete_listener(long token);

FFK_API FFKSession *ffk_config_get_last_session(void);
FFK_API FFKSession *ffk_config_get_last_completed_session(void);
FFK_API FFKSessionList *ffk_config_get_sessions(void);
FFK_API void ffk_config_clear_sessions(void);
FFK_API FFKSessionList *ffk_config_get_ffmpeg_sessions(void);
FFK_API FFKSessionList *ffk_config_get_ffprobe_sessions(void);
FFK_API FFKSessionList *ffk_config_get_media_information_sessions(void);
FFK_API FFKSessionList *ffk_config_get_sessions_by_state(int state);

FFK_API int ffk_config_get_log_redirection_strategy(void);
FFK_API void ffk_config_set_log_redirection_strategy(int strategy);
FFK_API int ffk_config_messages_in_transmit(long session_id);
FFK_API char *ffk_config_session_state_to_string(int state);
FFK_API FFKStringList *ffk_config_parse_arguments(const char *command);
FFK_API char *ffk_config_arguments_to_string(const char *const *arguments,
                                             size_t argument_count);

/* ------------------------------------------------------------------------ */
/* Packages and architecture                                                 */
/* ------------------------------------------------------------------------ */

FFK_API char *ffk_packages_get_package_name(void);
FFK_API FFKStringList *ffk_packages_get_external_libraries(void);
FFK_API char *ffk_arch_detect_get_arch(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FFMPEG_KIT_C_H */
