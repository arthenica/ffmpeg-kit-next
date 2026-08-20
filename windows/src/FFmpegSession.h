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

#ifndef FFMPEG_KIT_FFMPEG_SESSION_H
#define FFMPEG_KIT_FFMPEG_SESSION_H

#include "AbstractSession.h"
#include "FFmpegSessionCompleteCallback.h"
#include "StatisticsCallback.h"

namespace ffmpegkit {

class FFmpegSession;

namespace detail {
inline void ffmpegSessionTrampoline(FFKSession *session, void *userData);
}

/**
 * <p>An <code>FFmpeg</code> session.
 */
class FFmpegSession : public AbstractSession {
  public:
    /**
     * Builds a new FFmpeg session.
     *
     * @param arguments command arguments
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    create(const std::list<std::string> &arguments) {
        return create(arguments, nullptr, nullptr, nullptr);
    }

    /**
     * Builds a new FFmpeg session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    create(const std::list<std::string> &arguments,
           ffmpegkit::FFmpegSessionCompleteCallback completeCallback) {
        return create(arguments, completeCallback, nullptr, nullptr);
    }

    /**
     * Builds a new FFmpeg session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @param logCallback session specific log callback
     * @param statisticsCallback session specific statistics callback
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    create(const std::list<std::string> &arguments,
           ffmpegkit::FFmpegSessionCompleteCallback completeCallback,
           ffmpegkit::LogCallback logCallback,
           ffmpegkit::StatisticsCallback statisticsCallback) {
        return createInternal(arguments, completeCallback, logCallback,
                              statisticsCallback, -1);
    }

    /**
     * Builds a new FFmpeg session.
     *
     * @param arguments command arguments
     * @param completeCallback session specific complete callback
     * @param logCallback session specific log callback
     * @param statisticsCallback session specific statistics callback
     * @param logRedirectionStrategy session specific log redirection strategy
     * @return created session
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    create(const std::list<std::string> &arguments,
           ffmpegkit::FFmpegSessionCompleteCallback completeCallback,
           ffmpegkit::LogCallback logCallback,
           ffmpegkit::StatisticsCallback statisticsCallback,
           ffmpegkit::LogRedirectionStrategy logRedirectionStrategy) {
        return createInternal(arguments, completeCallback, logCallback,
                              statisticsCallback,
                              static_cast<int>(logRedirectionStrategy));
    }

    /**
     * Returns the session specific statistics callback.
     *
     * @return session specific statistics callback
     */
    ffmpegkit::StatisticsCallback getStatisticsCallback() {
        ffk_statistics_cb callback = nullptr;
        void *userData = nullptr;
        const int registered = ffk_ffmpeg_session_get_statistics_callback(
            handle(), &callback, &userData);
        return ffmpegkit::detail::recoverCallback<ffmpegkit::StatisticsCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::statisticsTrampoline);
    }

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    ffmpegkit::FFmpegSessionCompleteCallback getCompleteCallback() {
        ffk_session_cb callback = nullptr;
        void *userData = nullptr;
        const int registered =
            ffk_session_get_complete_callback(handle(), &callback, &userData);
        return ffmpegkit::detail::recoverCallback<
            ffmpegkit::FFmpegSessionCompleteCallback>(
            registered, callback, userData,
            &ffmpegkit::detail::ffmpegSessionTrampoline);
    }

    /**
     * Returns all statistics entries generated for this session, waiting for
     * the given timeout.
     *
     * @param waitTimeout wait timeout for statistics in milliseconds
     * @return list of statistics entries
     */
    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Statistics>>>
    getAllStatisticsWithTimeout(const int waitTimeout) {
        return ffmpegkit::detail::takeStatisticsList(
            ffk_ffmpeg_session_get_all_statistics_with_timeout(handle(),
                                                               waitTimeout));
    }

    /**
     * Returns all statistics entries generated for this session.
     *
     * @return list of statistics entries
     */
    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Statistics>>>
    getAllStatistics() {
        return ffmpegkit::detail::takeStatisticsList(
            ffk_ffmpeg_session_get_all_statistics(handle()));
    }

    /**
     * Returns all statistics entries delivered for this session so far.
     *
     * @return list of statistics entries
     */
    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Statistics>>>
    getStatistics() {
        return ffmpegkit::detail::takeStatisticsList(
            ffk_ffmpeg_session_get_statistics(handle()));
    }

    /**
     * Returns the last received statistics entry.
     *
     * @return last received statistics entry or nullptr
     */
    std::shared_ptr<ffmpegkit::Statistics> getLastReceivedStatistics() {
        FFKStatistics *statistics =
            ffk_ffmpeg_session_get_last_received_statistics(handle());
        auto result = ffmpegkit::detail::takeStatistics(statistics);
        ffk_statistics_free(statistics);
        return result;
    }

    /**
     * Adds a new statistics entry for this session.
     *
     * @param statistics statistics entry
     */
    void addStatistics(const std::shared_ptr<ffmpegkit::Statistics> statistics) {
        FFKStatistics *statisticsHandle =
            ffmpegkit::detail::makeStatisticsHandle(statistics);
        if (statisticsHandle == nullptr) {
            return;
        }
        ffk_ffmpeg_session_add_statistics(handle(), statisticsHandle);
        ffk_statistics_free(statisticsHandle);
        ffmpegkit::detail::checkError();
    }

    bool isFFmpeg() const override { return true; }

    bool isFFprobe() const override { return false; }

    bool isMediaInformation() const override { return false; }

  private:
    template <typename SessionType>
    friend std::shared_ptr<SessionType>
        ffmpegkit::detail::adoptSession(FFKSession *);

    explicit FFmpegSession(FFKSession *handle) : AbstractSession(handle) {}

    static std::shared_ptr<ffmpegkit::FFmpegSession>
    createInternal(const std::list<std::string> &arguments,
                   const ffmpegkit::FFmpegSessionCompleteCallback &completeCallback,
                   const ffmpegkit::LogCallback &logCallback,
                   const ffmpegkit::StatisticsCallback &statisticsCallback,
                   const int logRedirectionStrategy) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffmpeg_session_create(
            argumentArray.data(), argumentArray.size(),
            completeCallback ? &ffmpegkit::detail::ffmpegSessionTrampoline
                             : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFmpegSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>(),
            ffmpegkit::detail::statisticsCallbackFunction(statisticsCallback),
            ffmpegkit::detail::makeCallbackHolder(statisticsCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::StatisticsCallback>(),
            logRedirectionStrategy);
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFmpegSession>(handle);
    }
};

namespace detail {

/** @see logTrampoline */
inline void ffmpegSessionTrampoline(FFKSession *session, void *userData) {
    CallbackHolder<ffmpegkit::FFmpegSessionCompleteCallback> *holder =
        static_cast<CallbackHolder<ffmpegkit::FFmpegSessionCompleteCallback> *>(
            userData);
    if (holder == nullptr || !holder->function) {
        return;
    }
    try {
        // The handle belongs to the library and only lives for this call, so
        // the session handed to the callback gets one of its own
        holder->function(adoptSession<ffmpegkit::FFmpegSession>(
            ffk_session_retain(session)));
    } catch (...) {
    }
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_FFMPEG_SESSION_H
