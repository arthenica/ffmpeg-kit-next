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

#ifndef FFPROBE_KIT_H
#define FFPROBE_KIT_H

#include "FFprobeSession.h"
#include "MediaInformationJsonParser.h"
#include "MediaInformationSession.h"
#include "ffmpegkit_facade.h"
#include <stdlib.h>
#include <string.h>

namespace ffmpegkit {

/**
 * <p>Main class to run <code>FFprobe</code> commands. Supports executing
 * commands both synchronously and asynchronously.
 */
class FFprobeKit {
  public:
    /**
     * <p>Synchronously executes FFprobe with arguments provided.
     *
     * @param arguments FFprobe command options/arguments as string list
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    executeWithArguments(const std::list<std::string> &arguments) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffprobekit_execute_with_arguments(
            argumentArray.data(), argumentArray.size());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFprobeSession>(handle);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution with arguments provided.
     *
     * @param arguments FFprobe command options/arguments as string list
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    executeWithArgumentsAsync(const std::list<std::string> &arguments,
                              FFprobeSessionCompleteCallback completeCallback) {
        return executeWithArgumentsAsync(arguments, completeCallback, nullptr);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution with arguments provided.
     *
     * @param arguments FFprobe command options/arguments as string list
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @param logCallback callback that will receive logs
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    executeWithArgumentsAsync(const std::list<std::string> &arguments,
                              FFprobeSessionCompleteCallback completeCallback,
                              ffmpegkit::LogCallback logCallback) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffprobekit_execute_with_arguments_async(
            argumentArray.data(), argumentArray.size(),
            completeCallback ? &ffmpegkit::detail::ffprobeSessionTrampoline
                             : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFprobeSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFprobeSession>(handle);
    }

    /**
     * <p>Synchronously executes FFprobe command provided.
     *
     * @param command FFprobe command
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    execute(const std::string command) {
        FFKSession *handle = ffk_ffprobekit_execute(command.c_str());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFprobeSession>(handle);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution for the given command.
     *
     * @param command FFprobe command
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    executeAsync(const std::string command,
                 FFprobeSessionCompleteCallback completeCallback) {
        return executeAsync(command, completeCallback, nullptr);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution for the given command.
     *
     * @param command FFprobe command
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @param logCallback callback that will receive logs
     * @return FFprobe session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFprobeSession>
    executeAsync(const std::string command,
                 FFprobeSessionCompleteCallback completeCallback,
                 ffmpegkit::LogCallback logCallback) {
        FFKSession *handle = ffk_ffprobekit_execute_async(
            command.c_str(),
            completeCallback ? &ffmpegkit::detail::ffprobeSessionTrampoline
                             : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::FFprobeSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFprobeSession>(handle);
    }

    /**
     * <p>Extracts media information for the file specified with path.
     *
     * @param path path or uri of a media file
     * @return media information session created for this execution
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    getMediaInformation(const std::string path) {
        return getMediaInformation(
            path,
            ffmpegkit::AbstractSession::
                DefaultTimeoutForAsynchronousMessagesInTransmit);
    }

    /**
     * <p>Extracts media information for the file specified with path.
     *
     * @param path path or uri of a media file
     * @param waitTimeout max time to wait until media information is
     * transmitted
     * @return media information session created for this execution
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    getMediaInformation(const std::string path, const int waitTimeout) {
        FFKSession *handle =
            ffk_ffprobekit_get_media_information(path.c_str(), waitTimeout);
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<
            ffmpegkit::MediaInformationSession>(handle);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution to extract media information
     * for the specified file.
     *
     * @param path path or uri of a media file
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @return media information session created for this execution
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    getMediaInformationAsync(
        const std::string path,
        MediaInformationSessionCompleteCallback completeCallback) {
        return getMediaInformationAsync(
            path, completeCallback, nullptr,
            ffmpegkit::AbstractSession::
                DefaultTimeoutForAsynchronousMessagesInTransmit);
    }

    /**
     * <p>Starts an asynchronous FFprobe execution to extract media information
     * for the specified file.
     *
     * @param path path or uri of a media file
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @param logCallback callback that will receive logs
     * @param waitTimeout max time to wait until media information is
     * transmitted
     * @return media information session created for this execution
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    getMediaInformationAsync(
        const std::string path,
        MediaInformationSessionCompleteCallback completeCallback,
        ffmpegkit::LogCallback logCallback, const int waitTimeout) {
        FFKSession *handle = ffk_ffprobekit_get_media_information_async(
            path.c_str(),
            completeCallback
                ? &ffmpegkit::detail::mediaInformationSessionTrampoline
                : nullptr,
            ffmpegkit::detail::makeCallbackHolder(completeCallback),
            ffmpegkit::detail::callbackHolderDeleter<
                ffmpegkit::MediaInformationSessionCompleteCallback>(),
            ffmpegkit::detail::logCallbackFunction(logCallback),
            ffmpegkit::detail::makeCallbackHolder(logCallback),
            ffmpegkit::detail::callbackHolderDeleter<ffmpegkit::LogCallback>(),
            waitTimeout);
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<
            ffmpegkit::MediaInformationSession>(handle);
    }

    /**
     * <p>Extracts media information using the command provided asynchronously.
     *
     * @param command FFprobe command that prints media information for a file
     * in JSON format
     * @return media information session created for this execution
     */
    static std::shared_ptr<ffmpegkit::MediaInformationSession>
    getMediaInformationFromCommand(const std::string command) {
        FFKSession *handle =
            ffk_ffprobekit_get_media_information_from_command(command.c_str());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<
            ffmpegkit::MediaInformationSession>(handle);
    }

    /**
     * <p>Lists all FFprobe sessions in the session history.
     *
     * @return all FFprobe sessions in the session history
     */
    static std::shared_ptr<
        std::list<std::shared_ptr<ffmpegkit::FFprobeSession>>>
    listFFprobeSessions() {
        return ffmpegkit::detail::takeSessionList<ffmpegkit::FFprobeSession>(
            ffk_ffprobekit_list_ffprobe_sessions());
    }

    /**
     * <p>Lists all MediaInformation sessions in the session history.
     *
     * @return all MediaInformation sessions in the session history
     */
    static std::shared_ptr<
        std::list<std::shared_ptr<ffmpegkit::MediaInformationSession>>>
    listMediaInformationSessions() {
        return ffmpegkit::detail::takeSessionList<
            ffmpegkit::MediaInformationSession>(
            ffk_ffprobekit_list_media_information_sessions());
    }
};

} // namespace ffmpegkit

#endif // FFPROBE_KIT_H
