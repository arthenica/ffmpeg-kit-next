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

#ifndef FFMPEG_KIT_MEDIA_INFORMATION_SESSION_H
#define FFMPEG_KIT_MEDIA_INFORMATION_SESSION_H

#include "AbstractSession.h"
#include "MediaInformation.h"
#include "MediaInformationSessionCompleteCallback.h"

namespace ffmpegkit {

class MediaInformationSession;

namespace detail {
inline void mediaInformationSessionTrampoline(FFKSession *session,
                                              void *userData);
}

/**
 * <p>A custom <code>FFprobe</code> session, which produces a
 * <code>MediaInformation</code> object using the <code>FFprobe</code> output.
 */
class MediaInformationSession : public AbstractSession {
  public:
    /**
     * Builds a new media information session.
     *
     * @param arguments command arguments
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    create(const std::list<std::string> &arguments) {
        return createInternal(arguments, nullptr, nullptr);
    }

    /**
     * Builds a new media information session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    create(const std::list<std::string> &arguments,
           ffmpegkit::MediaInformationSessionCompleteCallback completeCallback) {
        return createInternal(arguments, completeCallback, nullptr);
    }

    /**
     * Builds a new media information session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @param logCallback session specific log callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    create(const std::list<std::string> &arguments,
           ffmpegkit::MediaInformationSessionCompleteCallback completeCallback,
           ffmpegkit::LogCallback logCallback) {
        return createInternal(arguments, completeCallback, logCallback);
    }

    /**
     * Returns the media information extracted in this session.
     *
     * @return media information extracted or nullptr if the command failed or
     * the output can not be parsed
     */
    std::shared_ptr<ffmpegkit::MediaInformation> getMediaInformation() {
        FFKMediaInformation *mediaInformation =
            ffk_media_information_session_get_media_information(handle());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptMediaInformation(mediaInformation);
    }

    /**
     * Sets the media information extracted in this session.
     *
     * @param mediaInformation media information extracted
     */
    void setMediaInformation(
        const std::shared_ptr<ffmpegkit::MediaInformation> mediaInformation) {
        ffk_media_information_session_set_media_information(
            handle(), mediaInformation == nullptr
                          ? nullptr
                          : ffmpegkit::detail::mediaInformationHandle(
                                *mediaInformation));
        ffmpegkit::detail::checkError();
    }

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    ffmpegkit::MediaInformationSessionCompleteCallback getCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered =
            ffk_session_get_complete_callback(handle(), &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::MediaInformationSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::mediaInformationSessionTrampoline);
    }

    bool isFFmpeg() const override { return false; }

    bool isFFprobe() const override { return false; }

    bool isMediaInformation() const override { return true; }

  private:
    template <typename SessionType>
    friend std::shared_ptr<SessionType>
        ffmpegkit::detail::adoptSession(FFKSession *);

    explicit MediaInformationSession(FFKSession *handle)
        : AbstractSession(handle) {}

    static std::shared_ptr<ffmpegkit::MediaInformationSession> createInternal(
        const std::list<std::string> &arguments,
        const ffmpegkit::MediaInformationSessionCompleteCallback
            &completeCallback,
        const ffmpegkit::LogCallback &logCallback) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_media_information_session_create(
            argumentArray.data(), argumentArray.size(),
            completeCallback
                ? &ffmpegkit::detail::mediaInformationSessionTrampoline
                : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::MediaInformationSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<
            ffmpegkit::MediaInformationSession>(handle);
    }
};

namespace detail {

/** @see logTrampoline */
inline void mediaInformationSessionTrampoline(FFKSession *session,
                                              void *userData) {
    CallbackHolder<ffmpegkit::MediaInformationSessionCompleteCallback> *holder =
        static_cast<
            CallbackHolder<ffmpegkit::MediaInformationSessionCompleteCallback> *>(
            userData);
    if (holder == nullptr || !holder->function) {
        return;
    }
    try {
        holder->function(adoptSession<ffmpegkit::MediaInformationSession>(
            ffk_session_retain(session)));
    } catch (...) {
    }
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_MEDIA_INFORMATION_SESSION_H
