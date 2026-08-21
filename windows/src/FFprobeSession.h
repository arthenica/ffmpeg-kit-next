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

#ifndef FFMPEG_KIT_FFPROBE_SESSION_H
#define FFMPEG_KIT_FFPROBE_SESSION_H

#include "AbstractSession.h"
#include "FFprobeSessionCompleteCallback.h"

namespace ffmpegkit {

class FFprobeSession;

namespace detail {
inline void ffprobeSessionTrampoline(FFKSession *session, void *userData);
}

/**
 * <p>An <code>FFprobe</code> session.
 */
class FFprobeSession : public AbstractSession {
  public:
    /**
     * Builds a new FFprobe session.
     *
     * @param arguments command arguments
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    create(const std::list<std::string> &arguments) {
        return createInternal(arguments, nullptr, nullptr, -1);
    }

    /**
     * Builds a new FFprobe session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    create(const std::list<std::string> &arguments,
           const FFprobeSessionCompleteCallback completeCallback) {
        return createInternal(arguments, completeCallback, nullptr, -1);
    }

    /**
     * Builds a new FFprobe session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @param logCallback session specific log callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    create(const std::list<std::string> &arguments,
           const FFprobeSessionCompleteCallback completeCallback,
           const ffmpegkit::LogCallback logCallback) {
        return createInternal(arguments, completeCallback, logCallback, -1);
    }

    /**
     * Builds a new FFprobe session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @param logCallback session specific log callback
     * @param logRedirectionStrategy session specific log redirection strategy
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    create(const std::list<std::string> &arguments,
           const FFprobeSessionCompleteCallback completeCallback,
           const ffmpegkit::LogCallback logCallback,
           const LogRedirectionStrategy logRedirectionStrategy) {
        return createInternal(arguments, completeCallback, logCallback,
                              static_cast<int>(logRedirectionStrategy));
    }

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    ffmpegkit::FFprobeSessionCompleteCallback getCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered =
            ffk_session_get_complete_callback(handle(), &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::FFprobeSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::ffprobeSessionTrampoline);
    }

    bool isFFmpeg() const override { return false; }

    bool isFFprobe() const override { return true; }

    bool isMediaInformation() const override { return false; }

  private:
    template <typename SessionType>
    friend std::shared_ptr<SessionType>
        ffmpegkit::detail::adoptSession(FFKSession *);

    explicit FFprobeSession(FFKSession *handle) : AbstractSession(handle) {}

    static std::shared_ptr<ffmpegkit::FFprobeSession> createInternal(
        const std::list<std::string> &arguments,
        const ffmpegkit::FFprobeSessionCompleteCallback &completeCallback,
        const ffmpegkit::LogCallback &logCallback,
        const int logRedirectionStrategy) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffprobe_session_create(
            argumentArray.data(), argumentArray.size(),
            completeCallback ? &ffmpegkit::detail::ffprobeSessionTrampoline
                             : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFprobeSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>(),
            logRedirectionStrategy);
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFprobeSession>(handle);
    }
};

namespace detail {

/** @see logTrampoline */
inline void ffprobeSessionTrampoline(FFKSession *session, void *userData) {
    CallbackHolder<ffmpegkit::FFprobeSessionCompleteCallback> *holder =
        static_cast<CallbackHolder<ffmpegkit::FFprobeSessionCompleteCallback> *>(
            userData);
    if (holder == nullptr || !holder->function) {
        return;
    }
    try {
        holder->function(adoptSession<ffmpegkit::FFprobeSession>(
            ffk_session_retain(session)));
    } catch (...) {
    }
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_FFPROBE_SESSION_H
