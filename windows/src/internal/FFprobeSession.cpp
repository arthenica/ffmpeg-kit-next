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

#include "FFprobeSession.h"
#include "FFmpegKitConfig.h"
#include "LogCallback.h"

extern void
addSessionToSessionHistory(const std::shared_ptr<ffmpegkit::internal::Session> session);

std::shared_ptr<ffmpegkit::internal::FFprobeSession>
ffmpegkit::internal::FFprobeSession::create(const std::list<std::string> &arguments) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::FFprobeSession>(
        std::make_shared<ffmpegkit::internal::FFprobeSession::PublicFFprobeSession>(
            arguments, nullptr, nullptr,
            ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession> ffmpegkit::internal::FFprobeSession::create(
    const std::list<std::string> &arguments,
    const FFprobeSessionCompleteCallback completeCallback) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::FFprobeSession>(
        std::make_shared<ffmpegkit::internal::FFprobeSession::PublicFFprobeSession>(
            arguments, completeCallback, nullptr,
            ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession> ffmpegkit::internal::FFprobeSession::create(
    const std::list<std::string> &arguments,
    const FFprobeSessionCompleteCallback completeCallback,
    const ffmpegkit::internal::LogCallback logCallback) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::FFprobeSession>(
        std::make_shared<ffmpegkit::internal::FFprobeSession::PublicFFprobeSession>(
            arguments, completeCallback, logCallback,
            ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession> ffmpegkit::internal::FFprobeSession::create(
    const std::list<std::string> &arguments,
    const FFprobeSessionCompleteCallback completeCallback,
    const ffmpegkit::internal::LogCallback logCallback,
    const LogRedirectionStrategy logRedirectionStrategy) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::FFprobeSession>(
        std::make_shared<ffmpegkit::internal::FFprobeSession::PublicFFprobeSession>(
            arguments, completeCallback, logCallback, logRedirectionStrategy));
    addSessionToSessionHistory(session);
    return session;
}

struct ffmpegkit::internal::FFprobeSession::PublicFFprobeSession
    : public ffmpegkit::internal::FFprobeSession {
    PublicFFprobeSession(const std::list<std::string> &arguments,
                         const FFprobeSessionCompleteCallback completeCallback,
                         const ffmpegkit::internal::LogCallback logCallback,
                         const LogRedirectionStrategy logRedirectionStrategy)
        : FFprobeSession(arguments, completeCallback, logCallback,
                         logRedirectionStrategy) {}
};

ffmpegkit::internal::FFprobeSession::FFprobeSession(
    const std::list<std::string> &arguments,
    const FFprobeSessionCompleteCallback completeCallback,
    const ffmpegkit::internal::LogCallback logCallback,
    const LogRedirectionStrategy logRedirectionStrategy)
    : ffmpegkit::internal::AbstractSession(arguments, logCallback,
                                 logRedirectionStrategy),
      _completeCallback{completeCallback} {}

ffmpegkit::internal::FFprobeSessionCompleteCallback
ffmpegkit::internal::FFprobeSession::getCompleteCallback() {
    return _completeCallback;
}

bool ffmpegkit::internal::FFprobeSession::isFFmpeg() const { return false; }

bool ffmpegkit::internal::FFprobeSession::isFFprobe() const { return true; }

bool ffmpegkit::internal::FFprobeSession::isMediaInformation() const { return false; }
