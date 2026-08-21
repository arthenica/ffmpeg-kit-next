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

#ifndef FFMPEG_KIT_H
#define FFMPEG_KIT_H

#include "FFmpegSession.h"
#include "LogCallback.h"
#include "StatisticsCallback.h"
#include "ffmpegkit_facade.h"
#include <stdlib.h>
#include <string.h>

namespace ffmpegkit {

/**
 * <p>Main class to run <code>FFmpeg</code> commands. Supports executing
 * commands both synchronously and asynchronously. <pre> auto session =
 * FFmpegKit::execute:("-i file1.mp4 -c:v libxvid file1.avi");
 *
 * auto asyncSession = FFmpegKit::executeAsync:("-i file1.mp4 -c:v libxvid
 * file1.avi", [](auto session){ ... });
 * </pre>
 * <p>Provides overloaded <code>execute</code> methods to define session
 * specific callbacks. <pre> auto asyncSession = FFmpegKit::executeAsync:("-i
 * file1.mp4 -c:v libxvid file1.avi, [](auto session){ ... }, [](auto log){ ...
 * }, [](auto statistics){ ... });
 * </pre>
 */
class FFmpegKit {
  public:
    /**
     * <p>Synchronously executes FFmpeg with arguments provided.
     *
     * @param arguments FFmpeg command options/arguments as string list
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    executeWithArguments(const std::list<std::string> &arguments) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffmpegkit_execute_with_arguments(
            argumentArray.data(), argumentArray.size());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFmpegSession>(handle);
    }

    /**
     * <p>Starts an asynchronous FFmpeg execution with arguments provided.
     *
     * @param arguments FFmpeg command options/arguments as string list
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    executeWithArgumentsAsync(const std::list<std::string> &arguments,
                              FFmpegSessionCompleteCallback completeCallback) {
        return executeWithArgumentsAsync(arguments, completeCallback, nullptr,
                                         nullptr);
    }

    /**
     * <p>Starts an asynchronous FFmpeg execution with arguments provided.
     *
     * @param arguments FFmpeg command options/arguments as string list
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @param logCallback callback that will receive logs
     * @param statisticsCallback callback that will receive statistics
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    executeWithArgumentsAsync(const std::list<std::string> &arguments,
                              FFmpegSessionCompleteCallback completeCallback,
                              ffmpegkit::LogCallback logCallback,
                              ffmpegkit::StatisticsCallback statisticsCallback) {
        const ffmpegkit::detail::ArgumentArray argumentArray(arguments);
        FFKSession *handle = ffk_ffmpegkit_execute_with_arguments_async(
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
                ffmpegkit::StatisticsCallback>());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFmpegSession>(handle);
    }

    /**
     * <p>Synchronously executes FFmpeg command provided. Space character is
     * used to split command into arguments. You can use single or double quote
     * characters to specify arguments inside your command.
     *
     * @param command FFmpeg command
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    execute(const std::string command) {
        FFKSession *handle = ffk_ffmpegkit_execute(command.c_str());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFmpegSession>(handle);
    }

    /**
     * <p>Starts an asynchronous FFmpeg execution for the given command.
     *
     * @param command FFmpeg command
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    executeAsync(const std::string command,
                 FFmpegSessionCompleteCallback completeCallback) {
        return executeAsync(command, completeCallback, nullptr, nullptr);
    }

    /**
     * <p>Starts an asynchronous FFmpeg execution for the given command.
     *
     * @param command FFmpeg command
     * @param completeCallback callback that will be called when the execution
     * has completed
     * @param logCallback callback that will receive logs
     * @param statisticsCallback callback that will receive statistics
     * @return FFmpeg session created for this execution
     */
    static std::shared_ptr<ffmpegkit::FFmpegSession>
    executeAsync(const std::string command,
                 FFmpegSessionCompleteCallback completeCallback,
                 ffmpegkit::LogCallback logCallback,
                 ffmpegkit::StatisticsCallback statisticsCallback) {
        FFKSession *handle = ffk_ffmpegkit_execute_async(
            command.c_str(),
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
                ffmpegkit::StatisticsCallback>());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptSession<ffmpegkit::FFmpegSession>(handle);
    }

    /**
     * <p>Cancels all running sessions.
     */
    static void cancel() {
        ffk_ffmpegkit_cancel();
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Cancels the session specified with sessionId.
     *
     * @param sessionId id of the session that will be cancelled
     */
    static void cancel(const long sessionId) {
        ffk_ffmpegkit_cancel_session(sessionId);
        ffmpegkit::detail::checkError();
    }

    /**
     * <p>Lists all FFmpeg sessions in the session history.
     *
     * @return all FFmpeg sessions in the session history
     */
    static std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::FFmpegSession>>>
    listSessions() {
        return ffmpegkit::detail::takeSessionList<ffmpegkit::FFmpegSession>(
            ffk_ffmpegkit_list_sessions());
    }
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_H
