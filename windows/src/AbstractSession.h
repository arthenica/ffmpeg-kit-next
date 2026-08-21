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

#ifndef FFMPEG_KIT_ABSTRACT_SESSION_H
#define FFMPEG_KIT_ABSTRACT_SESSION_H

#include "Session.h"
#include "ffmpegkit_facade.h"

namespace ffmpegkit {

class AbstractSession;

namespace detail {
inline FFKSession *sessionHandleOf(const ffmpegkit::AbstractSession &session);
}

/**
 * <p>Abstract session implementation which includes common features shared by
 * <code>FFmpeg</code>, <code>FFprobe</code> and <code>MediaInformation</code>
 * sessions.
 *
 * <p>Every method here forwards to the library over the flat C API, so the
 * session state itself lives inside the library and this object is only a
 * handle to it.
 */
class AbstractSession : public Session {
  public:
    /**
     * Defines how long default "getAll" methods wait, in milliseconds.
     */
    static constexpr int DefaultTimeoutForAsynchronousMessagesInTransmit = 5000;

    /**
     * Creates a new session.
     *
     * @param arguments command arguments
     * @param logCallback session specific log callback
     * @param logRedirectionStrategy session specific log redirection strategy
     */
    AbstractSession(const std::list<std::string> &arguments,
                    const ffmpegkit::LogCallback logCallback,
                    const LogRedirectionStrategy logRedirectionStrategy)
        : _handle(nullptr, ffk_session_free) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_abstract_session_create(
            argumentArray.data(), argumentArray.size(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>(),
            static_cast<int>(logRedirectionStrategy));
        ffmpegkit::detail::checkError();
        _handle.reset(handle, ffk_session_free);
    }

    virtual ~AbstractSession() = default;

    /**
     * Waits for all asynchronous messages to be transmitted until the given
     * timeout.
     *
     * @param timeout wait timeout in milliseconds
     */
    void waitForAsynchronousMessagesInTransmit(const int timeout) const {
        ffk_session_wait_for_asynchronous_messages_in_transmit(handle(),
                                                               timeout);
        ffmpegkit::detail::checkError();
    }

    ffmpegkit::LogCallback getLogCallback() const override {
        ffk_log_cb callback = nullptr;
        void *userData = nullptr;
        const int registered =
            ffk_session_get_log_callback(handle(), &callback, &userData);
        return ffmpegkit::detail::recoverCallback<ffmpegkit::LogCallback>(
            registered, callback, userData, &ffmpegkit::detail::logTrampoline);
    }

    long getSessionId() const override {
        const long sessionId = ffk_session_get_session_id(handle());
        ffmpegkit::detail::checkError();
        return sessionId;
    }

    std::chrono::time_point<std::chrono::system_clock>
    getCreateTime() const override {
        return ffmpegkit::detail::toTimePoint(
            ffk_session_get_create_time(handle()));
    }

    std::chrono::time_point<std::chrono::system_clock>
    getStartTime() const override {
        return ffmpegkit::detail::toTimePoint(
            ffk_session_get_start_time(handle()));
    }

    std::chrono::time_point<std::chrono::system_clock>
    getEndTime() const override {
        return ffmpegkit::detail::toTimePoint(
            ffk_session_get_end_time(handle()));
    }

    long getDuration() const override {
        return ffk_session_get_duration(handle());
    }

    std::shared_ptr<std::list<std::string>> getArguments() const override {
        return ffmpegkit::detail::takeStringList(
            ffk_session_get_arguments(handle()));
    }

    std::string getCommand() const override {
        return ffmpegkit::detail::takeString(ffk_session_get_command(handle()));
    }

    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Log>>>
    getAllLogsWithTimeout(const int waitTimeout) const override {
        return ffmpegkit::detail::takeLogList(
            ffk_session_get_all_logs_with_timeout(handle(), waitTimeout));
    }

    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Log>>>
    getAllLogs() const override {
        return ffmpegkit::detail::takeLogList(
            ffk_session_get_all_logs(handle()));
    }

    std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Log>>>
    getLogs() const override {
        return ffmpegkit::detail::takeLogList(ffk_session_get_logs(handle()));
    }

    std::string
    getAllLogsAsStringWithTimeout(const int waitTimeout) const override {
        return ffmpegkit::detail::takeString(
            ffk_session_get_all_logs_as_string_with_timeout(handle(),
                                                            waitTimeout));
    }

    std::string getAllLogsAsString() const override {
        return ffmpegkit::detail::takeString(
            ffk_session_get_all_logs_as_string(handle()));
    }

    std::string getLogsAsString() const override {
        return ffmpegkit::detail::takeString(
            ffk_session_get_logs_as_string(handle()));
    }

    std::string getOutput() const override {
        return ffmpegkit::detail::takeString(ffk_session_get_output(handle()));
    }

    ffmpegkit::SessionState getState() const override {
        return static_cast<ffmpegkit::SessionState>(
            ffk_session_get_state(handle()));
    }

    std::shared_ptr<ffmpegkit::ReturnCode> getReturnCode() const override {
        int value = 0;
        if (ffk_session_get_return_code(handle(), &value) == 0) {
            return nullptr;
        }
        return std::make_shared<ffmpegkit::ReturnCode>(value);
    }

    std::string getFailStackTrace() const override {
        return ffmpegkit::detail::takeString(
            ffk_session_get_fail_stack_trace(handle()));
    }

    ffmpegkit::LogRedirectionStrategy
    getLogRedirectionStrategy() const override {
        return static_cast<ffmpegkit::LogRedirectionStrategy>(
            ffk_session_get_log_redirection_strategy(handle()));
    }

    bool thereAreAsynchronousMessagesInTransmit() const override {
        return ffk_session_there_are_asynchronous_messages_in_transmit(
                   handle()) != 0;
    }

    void addLog(const std::shared_ptr<ffmpegkit::Log> log) override {
        if (log == nullptr) {
            return;
        }
        const std::string message = log->getMessage();
        ffk_session_add_log(handle(), log->getSessionId(),
                            static_cast<int>(log->getLevel()),
                            message.c_str());
        ffmpegkit::detail::checkError();
    }

    void startRunning() override {
        ffk_session_start_running(handle());
        ffmpegkit::detail::checkError();
    }

    void
    complete(const std::shared_ptr<ffmpegkit::ReturnCode> returnCode) override {
        if (returnCode == nullptr) {
            return;
        }
        ffk_session_complete(handle(), returnCode->getValue());
        ffmpegkit::detail::checkError();
    }

    void fail(const char *error) override {
        ffk_session_fail(handle(), error);
        ffmpegkit::detail::checkError();
    }

    bool isFFmpeg() const override {
        return ffk_session_is_ffmpeg(handle()) != 0;
    }

    bool isFFprobe() const override {
        return ffk_session_is_ffprobe(handle()) != 0;
    }

    bool isMediaInformation() const override {
        return ffk_session_is_media_information(handle()) != 0;
    }

    void cancel() override {
        ffk_session_cancel(handle());
        ffmpegkit::detail::checkError();
    }

  protected:
    template <typename SessionType>
    friend std::shared_ptr<SessionType>
        ffmpegkit::detail::adoptSession(FFKSession *);
    friend FFKSession *
    ffmpegkit::detail::sessionHandleOf(const ffmpegkit::AbstractSession &);

    /** Adopts a library session handle. */
    explicit AbstractSession(FFKSession *handle)
        : _handle(handle, ffk_session_free) {}

    FFKSession *handle() const { return _handle.get(); }

  private:
    std::shared_ptr<FFKSession> _handle;
};

namespace detail {

/** Borrows the library handle a session wraps. */
inline FFKSession *sessionHandleOf(const ffmpegkit::AbstractSession &session) {
    return session.handle();
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_ABSTRACT_SESSION_H
