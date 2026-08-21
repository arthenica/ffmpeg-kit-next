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

#include "MediaInformationSession.h"
#include "LogCallback.h"
#include "MediaInformation.h"

extern void
addSessionToSessionHistory(const std::shared_ptr<ffmpegkit::internal::Session> session);

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::MediaInformationSession::create(
    const std::list<std::string> &arguments) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::MediaInformationSession>(
        std::make_shared<
            ffmpegkit::internal::MediaInformationSession::PublicMediaInformationSession>(
            arguments, nullptr, nullptr));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::MediaInformationSession::create(
    const std::list<std::string> &arguments,
    ffmpegkit::internal::MediaInformationSessionCompleteCallback completeCallback) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::MediaInformationSession>(
        std::make_shared<
            ffmpegkit::internal::MediaInformationSession::PublicMediaInformationSession>(
            arguments, completeCallback, nullptr));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::MediaInformationSession::create(
    const std::list<std::string> &arguments,
    ffmpegkit::internal::MediaInformationSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback) {
    auto session = std::static_pointer_cast<ffmpegkit::internal::MediaInformationSession>(
        std::make_shared<
            ffmpegkit::internal::MediaInformationSession::PublicMediaInformationSession>(
            arguments, completeCallback, logCallback));
    addSessionToSessionHistory(session);
    return session;
}

struct ffmpegkit::internal::MediaInformationSession::PublicMediaInformationSession
    : public ffmpegkit::internal::MediaInformationSession {
    PublicMediaInformationSession(
        const std::list<std::string> &arguments,
        ffmpegkit::internal::MediaInformationSessionCompleteCallback completeCallback,
        ffmpegkit::internal::LogCallback logCallback)
        : MediaInformationSession(arguments, completeCallback, logCallback) {}
};

ffmpegkit::internal::MediaInformationSession::MediaInformationSession(
    const std::list<std::string> &arguments,
    ffmpegkit::internal::MediaInformationSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback)
    : ffmpegkit::internal::AbstractSession(
          arguments, logCallback,
          ffmpegkit::internal::LogRedirectionStrategyNeverPrintLogs),
      _completeCallback{completeCallback}, _mediaInformation{nullptr} {}

std::shared_ptr<ffmpegkit::internal::MediaInformation>
ffmpegkit::internal::MediaInformationSession::getMediaInformation() {
    return _mediaInformation;
}

void ffmpegkit::internal::MediaInformationSession::setMediaInformation(
    const std::shared_ptr<ffmpegkit::internal::MediaInformation> mediaInformation) {
    _mediaInformation = mediaInformation;
}

ffmpegkit::internal::MediaInformationSessionCompleteCallback
ffmpegkit::internal::MediaInformationSession::getCompleteCallback() {
    return _completeCallback;
}

bool ffmpegkit::internal::MediaInformationSession::isFFmpeg() const { return false; }

bool ffmpegkit::internal::MediaInformationSession::isFFprobe() const { return false; }

bool ffmpegkit::internal::MediaInformationSession::isMediaInformation() const {
    return true;
}
