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

#include "FFmpegKit.h"
#include "ArchDetect.h"
#include "FFmpegKitConfig.h"
#include "Packages.h"

extern "C" {
void cancel_operation(long id);
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession>
ffmpegkit::internal::FFmpegKit::executeWithArguments(
    const std::list<std::string> &arguments) {
    auto session = ffmpegkit::internal::FFmpegSession::create(arguments);
    ffmpegkit::internal::FFmpegKitConfig::ffmpegExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession>
ffmpegkit::internal::FFmpegKit::executeWithArgumentsAsync(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback) {
    auto session =
        ffmpegkit::internal::FFmpegSession::create(arguments, completeCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFmpegExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession>
ffmpegkit::internal::FFmpegKit::executeWithArgumentsAsync(
    const std::list<std::string> &arguments,
    FFmpegSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback,
    ffmpegkit::internal::StatisticsCallback statisticsCallback) {
    auto session = ffmpegkit::internal::FFmpegSession::create(
        arguments, completeCallback, logCallback, statisticsCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFmpegExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession>
ffmpegkit::internal::FFmpegKit::execute(const std::string command) {
    auto session = ffmpegkit::internal::FFmpegSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()));
    ffmpegkit::internal::FFmpegKitConfig::ffmpegExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession> ffmpegkit::internal::FFmpegKit::executeAsync(
    const std::string command, FFmpegSessionCompleteCallback completeCallback) {
    auto session = ffmpegkit::internal::FFmpegSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()), completeCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFmpegExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFmpegSession> ffmpegkit::internal::FFmpegKit::executeAsync(
    const std::string command, FFmpegSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback,
    ffmpegkit::internal::StatisticsCallback statisticsCallback) {
    auto session = ffmpegkit::internal::FFmpegSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()), completeCallback,
        logCallback, statisticsCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFmpegExecute(session);
    return session;
}

void ffmpegkit::internal::FFmpegKit::cancel() {

    /*
     * ZERO (0) IS A SPECIAL SESSION ID
     * WHEN IT IS PASSED TO THIS METHOD, A SIGINT IS GENERATED WHICH CANCELS ALL
     * ONGOING SESSIONS
     */
    cancel_operation(0);
}

void ffmpegkit::internal::FFmpegKit::cancel(const long sessionId) {
    cancel_operation(sessionId);
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::FFmpegSession>>>
ffmpegkit::internal::FFmpegKit::listSessions() {
    return ffmpegkit::internal::FFmpegKitConfig::getFFmpegSessions();
}
