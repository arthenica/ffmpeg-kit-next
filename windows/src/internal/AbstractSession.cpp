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

#include "AbstractSession.h"
#include "FFmpegKit.h"
#include "FFmpegKitConfig.h"
#include "LogCallback.h"
#include "ReturnCode.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

static std::atomic<long> sessionIdGenerator(1);

extern void
addSessionToSessionHistory(const std::shared_ptr<ffmpegkit::internal::Session> session);

ffmpegkit::internal::AbstractSession::AbstractSession(
    const std::list<std::string> &arguments,
    const ffmpegkit::internal::LogCallback logCallback,
    const LogRedirectionStrategy logRedirectionStrategy)
    : _arguments{std::make_shared<std::list<std::string>>(arguments)},
      _sessionId{sessionIdGenerator++}, _logCallback{logCallback},
      _createTime{std::chrono::system_clock::now()},
      _logs{std::make_shared<std::list<std::shared_ptr<ffmpegkit::internal::Log>>>()},
      _state{SessionStateCreated}, _returnCode{nullptr},
      _logRedirectionStrategy{logRedirectionStrategy} {}

void ffmpegkit::internal::AbstractSession::waitForAsynchronousMessagesInTransmit(
    const int timeout) const {
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    std::condition_variable condition_variable;
    const std::chrono::time_point<std::chrono::system_clock> expireTime =
        std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

    while (this->thereAreAsynchronousMessagesInTransmit() &&
           (std::chrono::system_clock::now() < expireTime)) {
        condition_variable.wait_for(lock, std::chrono::milliseconds(100));
    }
}

ffmpegkit::internal::LogCallback ffmpegkit::internal::AbstractSession::getLogCallback() const {
    return _logCallback;
}

long ffmpegkit::internal::AbstractSession::getSessionId() const { return _sessionId; }

std::chrono::time_point<std::chrono::system_clock>
ffmpegkit::internal::AbstractSession::getCreateTime() const {
    return _createTime;
}

std::chrono::time_point<std::chrono::system_clock>
ffmpegkit::internal::AbstractSession::getStartTime() const {
    return _startTime;
}

std::chrono::time_point<std::chrono::system_clock>
ffmpegkit::internal::AbstractSession::getEndTime() const {
    return _endTime;
}

long ffmpegkit::internal::AbstractSession::getDuration() const {
    const std::chrono::time_point<std::chrono::system_clock> startTime =
        _startTime;
    const std::chrono::time_point<std::chrono::system_clock> endTime = _endTime;

    if (startTime.time_since_epoch() != std::chrono::microseconds(0) &&
        endTime.time_since_epoch() != std::chrono::microseconds(0)) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                     startTime)
            .count();
    }

    return 0;
}

std::shared_ptr<std::list<std::string>>
ffmpegkit::internal::AbstractSession::getArguments() const {
    return _arguments;
}

std::string ffmpegkit::internal::AbstractSession::getCommand() const {
    return ffmpegkit::internal::FFmpegKitConfig::argumentsToString(_arguments);
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Log>>>
ffmpegkit::internal::AbstractSession::getAllLogsWithTimeout(const int waitTimeout) const {
    this->waitForAsynchronousMessagesInTransmit(waitTimeout);

    if (this->thereAreAsynchronousMessagesInTransmit()) {
        std::cout << "getAllLogsWithTimeout was called to return all logs but "
                     "there are still logs being transmitted for session id "
                  << _sessionId << "." << std::endl;
    }

    return this->getLogs();
}
std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Log>>>
ffmpegkit::internal::AbstractSession::getAllLogs() const {
    return this->getAllLogsWithTimeout(
        ffmpegkit::internal::AbstractSession::
            DefaultTimeoutForAsynchronousMessagesInTransmit);
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::Log>>>
ffmpegkit::internal::AbstractSession::getLogs() const {
    return _logs;
}

std::string ffmpegkit::internal::AbstractSession::getAllLogsAsStringWithTimeout(
    const int waitTimeout) const {
    this->waitForAsynchronousMessagesInTransmit(waitTimeout);

    if (this->thereAreAsynchronousMessagesInTransmit()) {
        std::cout
            << "getAllLogsAsStringWithTimeout was called to return all logs "
               "but there are still logs being transmitted for session id "
            << _sessionId << "." << std::endl;
    }

    return this->getLogsAsString();
}

std::string ffmpegkit::internal::AbstractSession::getAllLogsAsString() const {
    return this->getAllLogsAsStringWithTimeout(
        ffmpegkit::internal::AbstractSession::
            DefaultTimeoutForAsynchronousMessagesInTransmit);
}

std::string ffmpegkit::internal::AbstractSession::getLogsAsString() const {
    std::string concatenatedString;

    std::for_each(_logs->cbegin(), _logs->cend(),
                  [&](std::shared_ptr<ffmpegkit::internal::Log> log) {
                      concatenatedString.append(log->getMessage());
                  });

    return concatenatedString;
}

std::string ffmpegkit::internal::AbstractSession::getOutput() const {
    return this->getAllLogsAsString();
}

ffmpegkit::internal::SessionState ffmpegkit::internal::AbstractSession::getState() const {
    return _state;
}

std::shared_ptr<ffmpegkit::internal::ReturnCode>
ffmpegkit::internal::AbstractSession::getReturnCode() const {
    return _returnCode;
}

std::string ffmpegkit::internal::AbstractSession::getFailStackTrace() const {
    return _failStackTrace;
}

ffmpegkit::internal::LogRedirectionStrategy
ffmpegkit::internal::AbstractSession::getLogRedirectionStrategy() const {
    return _logRedirectionStrategy;
}

bool ffmpegkit::internal::AbstractSession::thereAreAsynchronousMessagesInTransmit()
    const {
    return (FFmpegKitConfig::messagesInTransmit(_sessionId) > 0);
}

void ffmpegkit::internal::AbstractSession::addLog(
    const std::shared_ptr<ffmpegkit::internal::Log> log) {
    _logs->push_back(log);
}

void ffmpegkit::internal::AbstractSession::startRunning() {
    _state = SessionStateRunning;
    _startTime = std::chrono::system_clock::now();
}

void ffmpegkit::internal::AbstractSession::complete(
    const std::shared_ptr<ffmpegkit::internal::ReturnCode> returnCode) {
    _returnCode = returnCode;
    _state = SessionStateCompleted;
    _endTime = std::chrono::system_clock::now();
}

void ffmpegkit::internal::AbstractSession::fail(const char *error) {
    _failStackTrace = error;
    _state = SessionStateFailed;
    _endTime = std::chrono::system_clock::now();
}

bool ffmpegkit::internal::AbstractSession::isFFmpeg() const {
    // IMPLEMENTED IN SUBCLASSES
    return false;
}

bool ffmpegkit::internal::AbstractSession::isFFprobe() const {
    // IMPLEMENTED IN SUBCLASSES
    return false;
}

bool ffmpegkit::internal::AbstractSession::isMediaInformation() const {
    // IMPLEMENTED IN SUBCLASSES
    return false;
}

void ffmpegkit::internal::AbstractSession::cancel() {
    if (_state == SessionStateRunning) {
        FFmpegKit::cancel(_sessionId);
    }
}
