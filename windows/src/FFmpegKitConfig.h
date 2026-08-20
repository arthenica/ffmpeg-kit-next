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

#ifndef FFMPEG_KIT_CONFIG_H
#define FFMPEG_KIT_CONFIG_H

#include "FFmpegSession.h"
#include "FFprobeSession.h"
#include "Level.h"
#include "LogCallback.h"
#include "MediaInformationSession.h"
#include "SessionDeleteListener.h"
#include "StatisticsCallback.h"
#include "ffmpegkit_facade.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <vector>

namespace ffmpegkit {

/**
 * <p>Enumeration type for signals that can be ignored.
 */
enum Signal {
    SignalInt = 2,
    SignalQuit = 3,
    SignalPipe = 13,
    SignalTerm = 15,
    SignalXcpu = 24
};

namespace detail {

/**
 * <p>Wraps a session handle in whichever facade class matches the session's
 * actual type.
 */
inline std::shared_ptr<ffmpegkit::Session> adoptAnySession(FFKSession *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    if (ffk_session_is_ffmpeg(handle) != 0) {
        return adoptSession<ffmpegkit::FFmpegSession>(handle);
    }
    if (ffk_session_is_ffprobe(handle) != 0) {
        return adoptSession<ffmpegkit::FFprobeSession>(handle);
    }
    if (ffk_session_is_media_information(handle) != 0) {
        return adoptSession<ffmpegkit::MediaInformationSession>(handle);
    }
    return adoptSession<ffmpegkit::AbstractSession>(handle);
}

/** Rebuilds a std::list of sessions of mixed types and releases the list. */
inline std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Session>>>
takeAnySessionList(FFKSessionList *list) {
    auto result =
        std::make_shared<std::list<std::shared_ptr<ffmpegkit::Session>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_session_list_size(list);
    for (size_t index = 0; index < size; index++) {
        result->push_back(adoptAnySession(ffk_session_list_get(list, index)));
    }
    ffk_session_list_free(list);
    return result;
}

/* ---- session delete listeners ------------------------------------------- */

struct SessionDeleteListenerHolder {
    std::shared_ptr<ffmpegkit::SessionDeleteListener> listener;
};

inline void sessionDeleteTrampoline(long sessionId, void *userData) {
    SessionDeleteListenerHolder *holder =
        static_cast<SessionDeleteListenerHolder *>(userData);
    if (holder == nullptr || holder->listener == nullptr) {
        return;
    }
    try {
        holder->listener->sessionDeleted(sessionId);
    } catch (...) {
    }
}

inline void freeSessionDeleteListenerHolder(void *userData) {
    delete static_cast<SessionDeleteListenerHolder *>(userData);
}

/*
 * The C API identifies a registered listener by an opaque token, so the
 * listener a caller passed to addSessionDeleteListener() has to be mapped back
 * to its token before it can be removed again.
 */
inline std::mutex &sessionDeleteTokenMutex() {
    static std::mutex mutex;
    return mutex;
}

inline std::map<ffmpegkit::SessionDeleteListener *, long> &
sessionDeleteTokens() {
    static std::map<ffmpegkit::SessionDeleteListener *, long> tokens;
    return tokens;
}

/** Holds a list of strings as the pointer array the C API expects. */
class StringMapArray {
  public:
    explicit StringMapArray(const std::map<std::string, std::string> &mapping) {
        _keys.reserve(mapping.size());
        _values.reserve(mapping.size());
        for (const auto &entry : mapping) {
            _keys.push_back(entry.first);
            _values.push_back(entry.second);
        }
        for (const std::string &key : _keys) {
            _keyPointers.push_back(key.c_str());
        }
        for (const std::string &value : _values) {
            _valuePointers.push_back(value.c_str());
        }
    }

    const char *const *keys() const {
        return _keyPointers.empty() ? nullptr : _keyPointers.data();
    }

    const char *const *values() const {
        return _valuePointers.empty() ? nullptr : _valuePointers.data();
    }

    size_t size() const { return _keyPointers.size(); }

  private:
    std::vector<std::string> _keys;
    std::vector<std::string> _values;
    std::vector<const char *> _keyPointers;
    std::vector<const char *> _valuePointers;
};

} // namespace detail

/**
 * <p>Configuration class of <code>FFmpegKit</code> library.
 */
class FFmpegKitConfig {
  public:
    static constexpr const char *FFmpegKitVersion = "8.1.1";
    static constexpr const char *FFmpegKitNamedPipePrefix = "fk_pipe_";

    /**
     * <p>Enables log and statistics redirection.
     */
    static void enableRedirection() {
        ffk_config_enable_redirection();
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Disables log and statistics redirection.
     */
    static void disableRedirection() {
        ffk_config_disable_redirection();
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Sets and overrides fontconfig configuration directory.
     *
     * @param path directory that contains fontconfig configuration
     * @return zero on success, non-zero on error
     */
    static int setFontconfigConfigurationPath(const std::string &path) {
        const int result =
            ffk_config_set_fontconfig_configuration_path(path.c_str());
        ffmpegkit::detail::checkError();
        return result;
    }

    /**
     * <p>Registers the fonts inside the given path.
     *
     * @param fontDirectoryPath directory that contains fonts
     * @param fontNameMapping mapping from font name to font file name
     */
    static void
    setFontDirectory(const std::string &fontDirectoryPath,
                     const std::map<std::string, std::string> &fontNameMapping) {
        const ffmpegkit::detail::StringMapArray mapping(fontNameMapping);
        ffk_config_set_font_directory(fontDirectoryPath.c_str(), mapping.keys(),
                                      mapping.values(), mapping.size());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Registers the fonts inside the given list of font directories.
     *
     * @param fontDirectoryList list of directories that contain fonts
     * @param fontNameMapping mapping from font name to font file name
     */
    static void setFontDirectoryList(
        const std::list<std::string> &fontDirectoryList,
        const std::map<std::string, std::string> &fontNameMapping) {
        const ffmpegkit::detail::ArgumentArray directories(fontDirectoryList);
        const ffmpegkit::detail::StringMapArray mapping(fontNameMapping);
        ffk_config_set_font_directory_list(directories.data(),
                                           directories.size(), mapping.keys(),
                                           mapping.values(), mapping.size());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Named pipes are not supported on Windows. Always returns nullptr.
     *
     * @return nullptr
     */
    FFMPEGKIT_DEPRECATED("Named pipes are not supported on Windows")
    static std::shared_ptr<std::string> registerNewFFmpegPipe() {
        return ffmpegkit::detail::takeOptionalString(
            ffk_config_register_new_ffmpeg_pipe());
    }

    /**
     * <p>Named pipes are not supported on Windows. Does nothing.
     *
     * @param ffmpegPipePath pipe path
     */
    FFMPEGKIT_DEPRECATED("Named pipes are not supported on Windows")
    static void closeFFmpegPipe(const std::string &ffmpegPipePath) {
        ffk_config_close_ffmpeg_pipe(ffmpegPipePath.c_str());
    }

    /**
     * <p>Registers an input buffer that can be read with the
     * <code>ffkbuffer</code> protocol.
     *
     * @param data buffer contents
     * @return buffer id
     */
    static long registerFFmpegKitInputBuffer(const std::vector<uint8_t> &data) {
        return registerFFmpegKitInputBuffer(data.data(), data.size());
    }

    /**
     * <p>Registers an input buffer that can be read with the
     * <code>ffkbuffer</code> protocol.
     *
     * @param data buffer contents
     * @param size buffer size
     * @return buffer id
     */
    static long registerFFmpegKitInputBuffer(const uint8_t *data,
                                             const size_t size) {
        const long bufferId =
            ffk_config_register_ffmpegkit_input_buffer(data, size);
        ffmpegkit::detail::checkError();
        return bufferId;
    }

    /**
     * <p>Registers an output buffer that can be written with the
     * <code>ffkbuffer</code> protocol.
     *
     * @param initialCapacity initial capacity in bytes
     * @param maxCapacity maximum capacity in bytes
     * @return buffer id
     */
    static long registerFFmpegKitOutputBuffer(const long initialCapacity,
                                              const long maxCapacity) {
        const long bufferId = ffk_config_register_ffmpegkit_output_buffer(
            initialCapacity, maxCapacity);
        ffmpegkit::detail::checkError();
        return bufferId;
    }

    /**
     * <p>Returns the size of the buffer registered with the given id.
     *
     * @param bufferId buffer id
     * @return buffer size in bytes
     */
    static long getFFmpegKitBufferSize(const long bufferId) {
        const long size = ffk_config_get_ffmpegkit_buffer_size(bufferId);
        ffmpegkit::detail::checkError();
        return size;
    }

    /**
     * <p>Returns the contents of an output buffer.
     *
     * @param bufferId buffer id
     * @return buffer contents or nullptr if the buffer does not exist
     */
    static std::shared_ptr<std::vector<uint8_t>>
    getFFmpegKitOutputBuffer(const long bufferId) {
        uint8_t *data = nullptr;
        size_t size = 0;
        const int found =
            ffk_config_get_ffmpegkit_output_buffer(bufferId, &data, &size);
        ffmpegkit::detail::checkError();
        if (found == 0) {
            return nullptr;
        }
        return ffmpegkit::detail::takeBytes(data, size);
    }

    /**
     * <p>Unregisters the buffer with the given id.
     *
     * @param bufferId buffer id
     */
    static void unregisterFFmpegKitBuffer(const long bufferId) {
        ffk_config_unregister_ffmpegkit_buffer(bufferId);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Registers a stream that can be used with the <code>ffkstream</code>
     * protocol.
     *
     * @param capacity stream capacity in bytes
     * @param type stream type
     * @return stream id
     */
    static long registerFFmpegKitStream(const long capacity, const int type) {
        const long streamId =
            ffk_config_register_ffmpegkit_stream(capacity, type);
        ffmpegkit::detail::checkError();
        return streamId;
    }

    /**
     * <p>Writes into an input stream.
     *
     * @param streamId stream id
     * @param data data to write
     * @param offset offset of the first byte to write
     * @param length number of bytes to write
     * @param timeoutMs write timeout in milliseconds
     * @return number of bytes written
     */
    static int writeFFmpegKitStream(const long streamId,
                                    const std::vector<uint8_t> &data,
                                    const size_t offset, const size_t length,
                                    const int timeoutMs) {
        if (offset > data.size() || length > data.size() - offset) {
            throw ffmpegkit::Exception(
                "offset and length must fit inside data");
        }
        return writeFFmpegKitStream(streamId, data.data() + offset, length,
                                    timeoutMs);
    }

    /**
     * <p>Writes into an input stream.
     *
     * @param streamId stream id
     * @param data data to write
     * @param length number of bytes to write
     * @param timeoutMs write timeout in milliseconds
     * @return number of bytes written
     */
    static int writeFFmpegKitStream(const long streamId, const uint8_t *data,
                                    const size_t length, const int timeoutMs) {
        const int written = ffk_config_write_ffmpegkit_stream(streamId, data,
                                                              length, timeoutMs);
        ffmpegkit::detail::checkError();
        return written;
    }

    /**
     * <p>Reads from an output stream.
     *
     * @param streamId stream id
     * @param maxBytes maximum number of bytes to read
     * @param timeoutMs read timeout in milliseconds
     * @return bytes read or nullptr when the stream does not exist
     */
    static std::shared_ptr<std::vector<uint8_t>>
    readFFmpegKitStream(const long streamId, const int maxBytes,
                        const int timeoutMs) {
        uint8_t *data = nullptr;
        size_t size = 0;
        const int found = ffk_config_read_ffmpegkit_stream(
            streamId, maxBytes, timeoutMs, &data, &size);
        ffmpegkit::detail::checkError();
        if (found == 0) {
            return nullptr;
        }
        return ffmpegkit::detail::takeBytes(data, size);
    }

    /**
     * <p>Closes the input side of a stream.
     *
     * @param streamId stream id
     */
    static void closeFFmpegKitStreamInput(const long streamId) {
        ffk_config_close_ffmpegkit_stream_input(streamId);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Unregisters the stream with the given id.
     *
     * @param streamId stream id
     */
    static void unregisterFFmpegKitStream(const long streamId) {
        ffk_config_unregister_ffmpegkit_stream(streamId);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the FFmpeg version bundled within the library.
     *
     * @return FFmpeg version
     */
    static std::string getFFmpegVersion() {
        return ffmpegkit::detail::takeString(ffk_config_get_ffmpeg_version());
    }

    /**
     * <p>Returns FFmpegKit library version.
     *
     * @return FFmpegKit version
     */
    static std::string getVersion() {
        return ffmpegkit::detail::takeString(ffk_config_get_version());
    }

    /**
     * <p>Returns whether this is an LTS release.
     *
     * @return false
     */
    FFMPEGKIT_DEPRECATED("Deprecated. Windows builds do not have an LTS build concept.")
    static bool isLTSBuild() {
        return ffk_config_is_lts_build() != 0;
    }

    /**
     * <p>Returns the build date of the library.
     *
     * @return build date
     */
    static std::string getBuildDate() {
        return ffmpegkit::detail::takeString(ffk_config_get_build_date());
    }

    /**
     * <p>Sets an environment variable.
     *
     * @param variableName environment variable name
     * @param variableValue environment variable value
     * @return zero on success, non-zero on error
     */
    static int setEnvironmentVariable(const std::string &variableName,
                                      const std::string &variableValue) {
        const int result = ffk_config_set_environment_variable(
            variableName.c_str(), variableValue.c_str());
        ffmpegkit::detail::checkError();
        return result;
    }

    /**
     * <p>Registers a new ignored signal. Ignored signals are not handled by
     * the library.
     *
     * @param signal signal to be ignored
     */
    static void ignoreSignal(const ffmpegkit::Signal signal) {
        ffk_config_ignore_signal(static_cast<int>(signal));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Synchronously executes the FFmpeg session provided.
     *
     * @param ffmpegSession FFmpeg session to execute
     */
    static void
    ffmpegExecute(const std::shared_ptr<ffmpegkit::FFmpegSession> ffmpegSession) {
        ffk_config_ffmpeg_execute(sessionHandle(ffmpegSession));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Synchronously executes the FFprobe session provided.
     *
     * @param ffprobeSession FFprobe session to execute
     */
    static void ffprobeExecute(
        const std::shared_ptr<ffmpegkit::FFprobeSession> ffprobeSession) {
        ffk_config_ffprobe_execute(sessionHandle(ffprobeSession));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Synchronously executes the media information session provided.
     *
     * @param mediaInformationSession media information session to execute
     * @param waitTimeout max time to wait until media information is
     * transmitted
     */
    static void getMediaInformationExecute(
        const std::shared_ptr<ffmpegkit::MediaInformationSession>
            mediaInformationSession,
        const int waitTimeout) {
        ffk_config_get_media_information_execute(
            sessionHandle(mediaInformationSession), waitTimeout);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Starts an asynchronous FFmpeg execution for the given session.
     *
     * @param ffmpegSession FFmpeg session to execute
     */
    static void asyncFFmpegExecute(
        const std::shared_ptr<ffmpegkit::FFmpegSession> ffmpegSession) {
        ffk_config_async_ffmpeg_execute(sessionHandle(ffmpegSession));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Starts an asynchronous FFprobe execution for the given session.
     *
     * @param ffprobeSession FFprobe session to execute
     */
    static void asyncFFprobeExecute(
        const std::shared_ptr<ffmpegkit::FFprobeSession> ffprobeSession) {
        ffk_config_async_ffprobe_execute(sessionHandle(ffprobeSession));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Starts an asynchronous media information execution for the given
     * session.
     *
     * @param mediaInformationSession media information session to execute
     * @param waitTimeout max time to wait until media information is
     * transmitted
     */
    static void asyncGetMediaInformationExecute(
        const std::shared_ptr<ffmpegkit::MediaInformationSession>
            mediaInformationSession,
        int waitTimeout) {
        ffk_config_async_get_media_information_execute(
            sessionHandle(mediaInformationSession), waitTimeout);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Sets a global callback to redirect FFmpeg/FFprobe logs.
     *
     * @param logCallback log callback or nullptr to disable a previously
     * defined callback
     */
    static void enableLogCallback(const ffmpegkit::LogCallback logCallback) {
        ffk_config_enable_log_callback(
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Sets a global callback to redirect FFmpeg statistics.
     *
     * @param statisticsCallback statistics callback or nullptr to disable a
     * previously defined callback
     */
    static void enableStatisticsCallback(
        const ffmpegkit::StatisticsCallback statisticsCallback) {
        ffk_config_enable_statistics_callback(
            ffmpegkit::detail::statisticsCallbackFunction(statisticsCallback),
            ffmpegkit::detail::makeCallbackHolder(statisticsCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::StatisticsCallback>());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Sets a global FFmpegSessionCompleteCallback.
     *
     * @param ffmpegSessionCompleteCallback complete callback or nullptr to
     * disable a previously defined callback
     */
    static void enableFFmpegSessionCompleteCallback(
        const FFmpegSessionCompleteCallback ffmpegSessionCompleteCallback) {
        ffk_config_enable_ffmpeg_session_complete_callback(
            ffmpegSessionCompleteCallback
                ? &ffmpegkit::detail::ffmpegSessionTrampoline
                : nullptr,
            ffmpegkit::detail::makeCallbackHolder(
                ffmpegSessionCompleteCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFmpegSessionCompleteCallback>());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the global FFmpegSessionCompleteCallback.
     *
     * @return global FFmpegSessionCompleteCallback
     */
    static FFmpegSessionCompleteCallback getFFmpegSessionCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered = ffk_config_get_ffmpeg_session_complete_callback(
            &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::FFmpegSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::ffmpegSessionTrampoline);
    }

    /**
     * <p>Sets a global FFprobeSessionCompleteCallback.
     *
     * @param ffprobeSessionCompleteCallback complete callback or nullptr to
     * disable a previously defined callback
     */
    static void enableFFprobeSessionCompleteCallback(
        const FFprobeSessionCompleteCallback ffprobeSessionCompleteCallback) {
        ffk_config_enable_ffprobe_session_complete_callback(
            ffprobeSessionCompleteCallback
                ? &ffmpegkit::detail::ffprobeSessionTrampoline
                : nullptr,
            ffmpegkit::detail::makeCallbackHolder(
                ffprobeSessionCompleteCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFprobeSessionCompleteCallback>());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the global FFprobeSessionCompleteCallback.
     *
     * @return global FFprobeSessionCompleteCallback
     */
    static FFprobeSessionCompleteCallback getFFprobeSessionCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered = ffk_config_get_ffprobe_session_complete_callback(
            &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::FFprobeSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::ffprobeSessionTrampoline);
    }

    /**
     * <p>Sets a global MediaInformationSessionCompleteCallback.
     *
     * @param mediaInformationSessionCompleteCallback complete callback or
     * nullptr to disable a previously defined callback
     */
    static void enableMediaInformationSessionCompleteCallback(
        const MediaInformationSessionCompleteCallback
            mediaInformationSessionCompleteCallback) {
        ffk_config_enable_media_information_session_complete_callback(
            mediaInformationSessionCompleteCallback
                ? &ffmpegkit::detail::mediaInformationSessionTrampoline
                : nullptr,
            ffmpegkit::detail::makeCallbackHolder(
                mediaInformationSessionCompleteCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::MediaInformationSessionCompleteCallback>());
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the global MediaInformationSessionCompleteCallback.
     *
     * @return global MediaInformationSessionCompleteCallback
     */
    static MediaInformationSessionCompleteCallback
    getMediaInformationSessionCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered =
            ffk_config_get_media_information_session_complete_callback(
                &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::MediaInformationSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::mediaInformationSessionTrampoline);
    }

    /**
     * <p>Returns the current log level.
     *
     * @return current log level
     */
    static ffmpegkit::Level getLogLevel() {
        return static_cast<ffmpegkit::Level>(ffk_config_get_log_level());
    }

    /**
     * <p>Sets the log level.
     *
     * @param level new log level
     */
    static void setLogLevel(const ffmpegkit::Level level) {
        ffk_config_set_log_level(static_cast<int>(level));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Converts a log level into its string representation.
     *
     * @param level log level
     * @return string representation of the level
     */
    static std::string logLevelToString(const ffmpegkit::Level level) {
        return ffmpegkit::detail::takeString(
            ffk_config_log_level_to_string(static_cast<int>(level)));
    }

    /**
     * <p>Returns the maximum number of sessions kept in the session history.
     *
     * @return session history size
     */
    static int getSessionHistorySize() {
        return ffk_config_get_session_history_size();
    }

    /**
     * <p>Sets the maximum number of sessions kept in the session history.
     *
     * @param sessionHistorySize new session history size
     */
    static void setSessionHistorySize(const int sessionHistorySize) {
        ffk_config_set_session_history_size(sessionHistorySize);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the session in the session history with the given id.
     *
     * @param sessionId session id
     * @return session or nullptr if it is not found
     */
    static std::shared_ptr<ffmpegkit::Session> getSession(const long sessionId) {
        FFKSession *handle = ffk_config_get_session(sessionId);
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptAnySession(handle);
    }

    /**
     * <p>Deletes the session with the given id from the session history.
     *
     * @param sessionId session id
     */
    static void deleteSession(const long sessionId) {
        ffk_config_delete_session(sessionId);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Adds a session delete listener.
     *
     * @param listener listener to add
     */
    static void addSessionDeleteListener(
        const std::shared_ptr<ffmpegkit::SessionDeleteListener> listener) {
        if (listener == nullptr) {
            return;
        }
        ffmpegkit::detail::SessionDeleteListenerHolder *holder =
            new ffmpegkit::detail::SessionDeleteListenerHolder();
        holder->listener = listener;
        const long token = ffk_config_add_session_delete_listener(
            &ffmpegkit::detail::sessionDeleteTrampoline, holder,
            &ffmpegkit::detail::freeSessionDeleteListenerHolder);
        if (ffk_has_error()) {
            ffmpegkit::detail::checkError();
        }
        if (token == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(
            ffmpegkit::detail::sessionDeleteTokenMutex());
        ffmpegkit::detail::sessionDeleteTokens()[listener.get()] = token;
    }

    /**
     * <p>Removes a session delete listener.
     *
     * @param listener listener to remove
     */
    static void removeSessionDeleteListener(
        const std::shared_ptr<ffmpegkit::SessionDeleteListener> listener) {
        if (listener == nullptr) {
            return;
        }
        long token = 0;
        {
            std::lock_guard<std::mutex> lock(
                ffmpegkit::detail::sessionDeleteTokenMutex());
            auto &tokens = ffmpegkit::detail::sessionDeleteTokens();
            auto entry = tokens.find(listener.get());
            if (entry == tokens.end()) {
                return;
            }
            token = entry->second;
            tokens.erase(entry);
        }
        ffk_config_remove_session_delete_listener(token);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the last session created.
     *
     * @return last session or nullptr if the session history is empty
     */
    static std::shared_ptr<ffmpegkit::Session> getLastSession() {
        FFKSession *handle = ffk_config_get_last_session();
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptAnySession(handle);
    }

    /**
     * <p>Returns the last session completed.
     *
     * @return last completed session or nullptr if no session has completed
     */
    static std::shared_ptr<ffmpegkit::Session> getLastCompletedSession() {
        FFKSession *handle = ffk_config_get_last_completed_session();
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptAnySession(handle);
    }

    /**
     * <p>Returns all sessions in the session history.
     *
     * @return all sessions in the session history
     */
    static std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Session>>>
    getSessions() {
        return ffmpegkit::detail::takeAnySessionList(ffk_config_get_sessions());
    }

    /**
     * <p>Clears the session history.
     */
    static void clearSessions() {
        ffk_config_clear_sessions();
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns all FFmpeg sessions in the session history.
     *
     * @return all FFmpeg sessions in the session history
     */
    static std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::FFmpegSession>>>
    getFFmpegSessions() {
        return ffmpegkit::detail::takeSessionList<ffmpegkit::FFmpegSession>(
            ffk_config_get_ffmpeg_sessions());
    }

    /**
     * <p>Returns all FFprobe sessions in the session history.
     *
     * @return all FFprobe sessions in the session history
     */
    static std::shared_ptr<
        std::list<std::shared_ptr<ffmpegkit::FFprobeSession>>>
    getFFprobeSessions() {
        return ffmpegkit::detail::takeSessionList<ffmpegkit::FFprobeSession>(
            ffk_config_get_ffprobe_sessions());
    }

    /**
     * <p>Returns all MediaInformation sessions in the session history.
     *
     * @return all MediaInformation sessions in the session history
     */
    static std::shared_ptr<
        std::list<std::shared_ptr<ffmpegkit::MediaInformationSession>>>
    getMediaInformationSessions() {
        return ffmpegkit::detail::takeSessionList<
            ffmpegkit::MediaInformationSession>(
            ffk_config_get_media_information_sessions());
    }

    /**
     * <p>Returns sessions that have the given state.
     *
     * @param state session state
     * @return sessions that have the given state
     */
    static std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Session>>>
    getSessionsByState(const SessionState state) {
        return ffmpegkit::detail::takeAnySessionList(
            ffk_config_get_sessions_by_state(static_cast<int>(state)));
    }

    /**
     * <p>Returns the active log redirection strategy.
     *
     * @return log redirection strategy
     */
    static LogRedirectionStrategy getLogRedirectionStrategy() {
        return static_cast<LogRedirectionStrategy>(
            ffk_config_get_log_redirection_strategy());
    }

    /**
     * <p>Sets the log redirection strategy.
     *
     * @param logRedirectionStrategy log redirection strategy
     */
    static void setLogRedirectionStrategy(
        const LogRedirectionStrategy logRedirectionStrategy) {
        ffk_config_set_log_redirection_strategy(
            static_cast<int>(logRedirectionStrategy));
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Returns the number of async messages that are not transmitted to the
     * callbacks for this session.
     *
     * @param sessionId session id
     * @return number of async messages that are not transmitted yet
     */
    static int messagesInTransmit(const long sessionId) {
        const int count = ffk_config_messages_in_transmit(sessionId);
        ffmpegkit::detail::checkError();
        return count;
    }

    /**
     * <p>Converts a session state into its string representation.
     *
     * @param state session state
     * @return string representation of the state
     */
    static std::string sessionStateToString(SessionState state) {
        return ffmpegkit::detail::takeString(
            ffk_config_session_state_to_string(static_cast<int>(state)));
    }

    /**
     * <p>Parses the given command into arguments. Uses space character to
     * split the arguments. Supports single and double quote characters.
     *
     * @param command string command
     * @return list of arguments
     */
    static std::list<std::string> parseArguments(const std::string &command) {
        FFKStringList *list = ffk_config_parse_arguments(command.c_str());
        ffmpegkit::detail::checkError();
        return *ffmpegkit::detail::takeStringList(list);
    }

    /**
     * <p>Concatenates arguments into a string adding a space character between
     * two arguments.
     *
     * @param arguments arguments
     * @return concatenated string containing all arguments
     */
    static std::string
    argumentsToString(std::shared_ptr<std::list<std::string>> arguments) {
        if (arguments == nullptr) {
            return std::string();
        }
        const ffmpegkit::detail::ArgumentArray argumentArray(*arguments);
        return ffmpegkit::detail::takeString(ffk_config_arguments_to_string(
            argumentArray.data(), argumentArray.size()));
    }

  private:
    /** Borrows the library handle a facade session wraps. */
    template <typename SessionType>
    static FFKSession *
    sessionHandle(const std::shared_ptr<SessionType> &session) {
        return session == nullptr
                   ? nullptr
                   : ffmpegkit::detail::sessionHandleOf(*session);
    }
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_CONFIG_H
