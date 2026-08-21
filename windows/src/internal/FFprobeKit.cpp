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

#include "FFprobeKit.h"
#include "FFmpegKit.h"
#include "FFmpegKitConfig.h"

static std::list<std::string>
defaultGetMediaInformationCommandArguments(const std::string &path) {
    return std::list<std::string>{"-v",
                                  "error",
                                  "-hide_banner",
                                  "-print_format",
                                  "json",
                                  "-show_format",
                                  "-show_streams",
                                  "-show_chapters",
                                  "-i",
                                  path};
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession>
ffmpegkit::internal::FFprobeKit::executeWithArguments(
    const std::list<std::string> &arguments) {
    auto session = ffmpegkit::internal::FFprobeSession::create(arguments);
    ffmpegkit::internal::FFmpegKitConfig::ffprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession>
ffmpegkit::internal::FFprobeKit::executeWithArgumentsAsync(
    const std::list<std::string> &arguments,
    FFprobeSessionCompleteCallback completeCallback) {
    auto session =
        ffmpegkit::internal::FFprobeSession::create(arguments, completeCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession>
ffmpegkit::internal::FFprobeKit::executeWithArgumentsAsync(
    const std::list<std::string> &arguments,
    FFprobeSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback) {
    auto session = ffmpegkit::internal::FFprobeSession::create(
        arguments, completeCallback, logCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession>
ffmpegkit::internal::FFprobeKit::execute(const std::string command) {
    auto session = ffmpegkit::internal::FFprobeSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()));
    ffmpegkit::internal::FFmpegKitConfig::ffprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession> ffmpegkit::internal::FFprobeKit::executeAsync(
    const std::string command,
    FFprobeSessionCompleteCallback completeCallback) {
    auto session = ffmpegkit::internal::FFprobeSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()), completeCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::FFprobeSession> ffmpegkit::internal::FFprobeKit::executeAsync(
    const std::string command, FFprobeSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback) {
    auto session = ffmpegkit::internal::FFprobeSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()), completeCallback,
        logCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncFFprobeExecute(session);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::FFprobeKit::getMediaInformation(const std::string path) {
    auto arguments = defaultGetMediaInformationCommandArguments(path);
    auto session = ffmpegkit::internal::MediaInformationSession::create(arguments);
    ffmpegkit::internal::FFmpegKitConfig::getMediaInformationExecute(
        session, ffmpegkit::internal::AbstractSession::
                     DefaultTimeoutForAsynchronousMessagesInTransmit);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::FFprobeKit::getMediaInformation(const std::string path,
                                           const int waitTimeout) {
    auto arguments = defaultGetMediaInformationCommandArguments(path);
    auto session = ffmpegkit::internal::MediaInformationSession::create(arguments);
    ffmpegkit::internal::FFmpegKitConfig::getMediaInformationExecute(session,
                                                           waitTimeout);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::FFprobeKit::getMediaInformationAsync(
    const std::string path,
    MediaInformationSessionCompleteCallback completeCallback) {
    auto arguments = defaultGetMediaInformationCommandArguments(path);
    auto session =
        ffmpegkit::internal::MediaInformationSession::create(arguments, completeCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncGetMediaInformationExecute(
        session, ffmpegkit::internal::AbstractSession::
                     DefaultTimeoutForAsynchronousMessagesInTransmit);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::FFprobeKit::getMediaInformationAsync(
    const std::string path,
    MediaInformationSessionCompleteCallback completeCallback,
    ffmpegkit::internal::LogCallback logCallback, const int waitTimeout) {
    auto arguments = defaultGetMediaInformationCommandArguments(path);
    auto session = ffmpegkit::internal::MediaInformationSession::create(
        arguments, completeCallback, logCallback);
    ffmpegkit::internal::FFmpegKitConfig::asyncGetMediaInformationExecute(session,
                                                                waitTimeout);
    return session;
}

std::shared_ptr<ffmpegkit::internal::MediaInformationSession>
ffmpegkit::internal::FFprobeKit::getMediaInformationFromCommand(
    const std::string command) {
    auto session = ffmpegkit::internal::MediaInformationSession::create(
        FFmpegKitConfig::parseArguments(command.c_str()));
    ffmpegkit::internal::FFmpegKitConfig::getMediaInformationExecute(
        session, ffmpegkit::internal::AbstractSession::
                     DefaultTimeoutForAsynchronousMessagesInTransmit);
    return session;
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::FFprobeSession>>>
ffmpegkit::internal::FFprobeKit::listFFprobeSessions() {
    return ffmpegkit::internal::FFmpegKitConfig::getFFprobeSessions();
}

std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::internal::MediaInformationSession>>>
ffmpegkit::internal::FFprobeKit::listMediaInformationSessions() {
    return ffmpegkit::internal::FFmpegKitConfig::getMediaInformationSessions();
}
