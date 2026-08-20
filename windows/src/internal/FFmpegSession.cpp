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

#include "FFmpegSession.h"
#include "FFmpegKitConfig.h"
#include "LogCallback.h"
#include "StatisticsCallback.h"

extern void
addSessionToSessionHistory(const std::shared_ptr<ffmpegkit::internal::Session> session);

std::shared_ptr<ffmpegkit::internal::FFmpegSession>
ffmpegkit::internal::FFmpegSession::create(const std::list<std::string> &arguments) {
    std::shared_ptr<ffmpegkit::internal::FFmpegSession> session =
        std::static_pointer_cast<ffmpegkit::internal::FFmpegSession>(
            std::make_shared<ffmpegkit::internal::FFmpegSession::PublicFFmpegSession>(
                arguments, nullptr, nullptr, nullptr,
                ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession> ffmpegkit::internal::FFmpegSession::create(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback) {
    std::shared_ptr<ffmpegkit::internal::FFmpegSession> session =
        std::static_pointer_cast<ffmpegkit::internal::FFmpegSession>(
            std::make_shared<ffmpegkit::internal::FFmpegSession::PublicFFmpegSession>(
                arguments, completeCallback, nullptr, nullptr,
                ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession> ffmpegkit::internal::FFmpegSession::create(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback,
    ffmpegkit::internal::StatisticsCallback statisticsCallback) {
    std::shared_ptr<ffmpegkit::internal::FFmpegSession> session =
        std::static_pointer_cast<ffmpegkit::internal::FFmpegSession>(
            std::make_shared<ffmpegkit::internal::FFmpegSession::PublicFFmpegSession>(
                arguments, completeCallback, logCallback, statisticsCallback,
                ffmpegkit::internal::FFmpegKitConfig::getLogRedirectionStrategy()));
    addSessionToSessionHistory(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession> ffmpegkit::internal::FFmpegSession::create(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback,
    ffmpegkit::internal::StatisticsCallback statisticsCallback,
    LogRedirectionStrategy logRedirectionStrategy) {
    std::shared_ptr<ffmpegkit::internal::FFmpegSession> session =
        std::static_pointer_cast<ffmpegkit::internal::FFmpegSession>(
            std::make_shared<ffmpegkit::internal::FFmpegSession::PublicFFmpegSession>(
                arguments, completeCallback, logCallback, statisticsCallback,
                logRedirectionStrategy));
    addSessionToSessionHistory(session);
    return session;
}

struct ffmpegkit::internal::FFmpegSession::PublicFFmpegSession
    : public ffmpegkit::internal::FFmpegSession {
    PublicFFmpegSession(const std::list<std::string> &arguments,
                        FFmpegSessionCompleteCallback completeCallback,
                        ffmpegkit::internal::LogCallback logCallback,
                        ffmpegkit::internal::StatisticsCallback statisticsCallback,
                        LogRedirectionStrategy logRedirectionStrategy)
        : FFmpegSession(arguments, completeCallback, logCallback,
                        statisticsCallback, logRedirectionStrategy) {}
};

ffmpegkit::internal::FFmpegSession::FFmpegSession(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback,
    ffmpegkit::internal::StatisticsCallback statisticsCallback,
    LogRedirectionStrategy logRedirectionStrategy)
    : ffmpegkit::internal::AbstractSession(arguments, logCallback,
                                 logRedirectionStrategy),
      _completeCallback{completeCallback},
      _statisticsCallback{statisticsCallback},
      _statistics{std::make_shared<
          std::list<std::shared_ptr<ffmpegkit::internal::Statistics>>>()} {}

ffmpegkit::internal::StatisticsCallback
ffmpegkit::internal::FFmpegSession::getStatisticsCallback() {
    return _statisticsCallback;
}

ffmpegkit::internal::FFmpegSessionCompleteCallback
ffmpegkit::internal::FFmpegSession::getCompleteCallback() {
    return _completeCallback;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Statistics>>>
ffmpegkit::internal::FFmpegSession::getAllStatisticsWithTimeout(const int waitTimeout) {
    this->waitForAsynchronousMessagesInTransmit(waitTimeout);

    if (this->thereAreAsynchronousMessagesInTransmit()) {
        std::cout << "getAllStatisticsWithTimeout was called to return all "
                     "statistics but there are still statistics being "
                     "transmitted for session id "
                  << this->getSessionId() << "." << std::endl;
    }

    return this->getStatistics();
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Statistics>>>
ffmpegkit::internal::FFmpegSession::getAllStatistics() {
    return this->getAllStatisticsWithTimeout(
        ffmpegkit::internal::AbstractSession::
            DefaultTimeoutForAsynchronousMessagesInTransmit);
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Statistics>>>
ffmpegkit::internal::FFmpegSession::getStatistics() {
    return _statistics;
}

std::shared_ptr<ffmpegkit::internal::Statistics>
ffmpegkit::internal::FFmpegSession::getLastReceivedStatistics() {
    if (_statistics->size() > 0) {
        return _statistics->back();
    } else {
        return nullptr;
    }
}

void ffmpegkit::internal::FFmpegSession::addStatistics(
    const std::shared_ptr<ffmpegkit::internal::Statistics> statistics) {
    _statistics->push_back(statistics);
}

bool ffmpegkit::internal::FFmpegSession::isFFmpeg() const { return true; }

bool ffmpegkit::internal::FFmpegSession::isFFprobe() const { return false; }

bool ffmpegkit::internal::FFmpegSession::isMediaInformation() const { return false; }
