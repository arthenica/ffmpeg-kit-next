/*
 * Copyright (c) 2018-2021, 2026 Taner Sener
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

package com.arthenica.ffmpegkit

import java.util.concurrent.ExecutorService

/**
 * <p>Main class to run <code>FFmpeg</code> commands. Supports executing commands both
 * synchronously and asynchronously.
 * <pre>
 * FFmpegSession session = FFmpegKit.execute("-i file1.mp4 -c:v libxvid file1.avi");
 *
 * FFmpegSession asyncSession = FFmpegKit.executeAsync("-i file1.mp4 -c:v libxvid file1.avi", completeCallback);
 * </pre>
 * <p>Provides overloaded <code>execute</code> methods to define session specific callbacks.
 * <pre>
 * FFmpegSession asyncSession = FFmpegKit.executeAsync("-i file1.mp4 -c:v libxvid file1.avi", completeCallback, logCallback, statisticsCallback);
 * </pre>
 */
open class FFmpegKit private constructor() {
    companion object {
        init {
            AbiDetect::class.java.name
            FFmpegKitConfig::class.java.name
        }

        /**
         * <p>Synchronously executes FFmpeg with arguments provided.
         *
         * @param arguments FFmpeg command options/arguments as string array
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeWithArguments(arguments: Array<String>): FFmpegSession {
            val session = FFmpegSession.create(arguments)

            FFmpegKitConfig.ffmpegExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFmpeg command options/arguments as string array
         * @param completeCallback callback that will be called when the execution has completed
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFmpegSessionCompleteCallback?): FFmpegSession {
            val session = FFmpegSession.create(arguments, completeCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments          FFmpeg command options/arguments as string array
         * @param completeCallback   callback that will be called when the execution has completed
         * @param logCallback        callback that will receive logs
         * @param statisticsCallback callback that will receive statistics
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFmpegSessionCompleteCallback?, logCallback: LogCallback?, statisticsCallback: StatisticsCallback?): FFmpegSession {
            val session = FFmpegSession.create(arguments, completeCallback, logCallback, statisticsCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFmpeg command options/arguments as string array
         * @param completeCallback callback that will be called when the execution has completed
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFmpegSessionCompleteCallback?, executorService: ExecutorService): FFmpegSession {
            val session = FFmpegSession.create(arguments, completeCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session, executorService)

            return session
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments          FFmpeg command options/arguments as string array
         * @param completeCallback   callback that will be called when the execution has completed
         * @param logCallback        callback that will receive logs
         * @param statisticsCallback callback that will receive statistics
         * @param executorService    executor service that will be used to run this asynchronous
         *                           operation
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFmpegSessionCompleteCallback?, logCallback: LogCallback?, statisticsCallback: StatisticsCallback?, executorService: ExecutorService): FFmpegSession {
            val session = FFmpegSession.create(arguments, completeCallback, logCallback, statisticsCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session, executorService)

            return session
        }

        /**
         * <p>Synchronously executes FFmpeg command provided. Space character is used to split command
         * into arguments. You can use single or double quote characters to specify arguments inside
         * your command.
         *
         * @param command FFmpeg command
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun execute(command: String): FFmpegSession =
            executeWithArguments(FFmpegKitConfig.parseArguments(command))

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given command. Space character is used to
         * split the command into arguments. You can use single or double quote characters to specify
         * arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFmpeg command
         * @param completeCallback callback that will be called when the execution has completed
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFmpegSessionCompleteCallback?): FFmpegSession =
            executeWithArgumentsAsync(FFmpegKitConfig.parseArguments(command), completeCallback)

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given command. Space character is used to
         * split the command into arguments. You can use single or double quote characters to specify
         * arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command            FFmpeg command
         * @param completeCallback   callback that will be called when the execution has completed
         * @param logCallback        callback that will receive logs
         * @param statisticsCallback callback that will receive statistics
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFmpegSessionCompleteCallback?, logCallback: LogCallback?, statisticsCallback: StatisticsCallback?): FFmpegSession =
            executeWithArgumentsAsync(FFmpegKitConfig.parseArguments(command), completeCallback, logCallback, statisticsCallback)

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given command. Space character is used to
         * split the command into arguments. You can use single or double quote characters to specify
         * arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFmpeg command
         * @param completeCallback callback that will be called when the execution has completed
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFmpegSessionCompleteCallback?, executorService: ExecutorService): FFmpegSession {
            val session = FFmpegSession.create(FFmpegKitConfig.parseArguments(command), completeCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session, executorService)

            return session
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given command. Space character is used to
         * split the command into arguments. You can use single or double quote characters to specify
         * arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFmpegSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command            FFmpeg command
         * @param completeCallback   callback that will be called when the execution has completed
         * @param logCallback        callback that will receive logs
         * @param statisticsCallback callback that will receive statistics
         * @param executorService    executor service that will be used to run this asynchronous operation
         * @return FFmpeg session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFmpegSessionCompleteCallback?, logCallback: LogCallback?, statisticsCallback: StatisticsCallback?, executorService: ExecutorService): FFmpegSession {
            val session = FFmpegSession.create(FFmpegKitConfig.parseArguments(command), completeCallback, logCallback, statisticsCallback)

            FFmpegKitConfig.asyncFFmpegExecute(session, executorService)

            return session
        }

        /**
         * <p>Cancels all running sessions.
         *
         * <p>This method does not wait for termination to complete and returns immediately.
         */
        @JvmStatic
        fun cancel() {

            /*
             * ZERO (0) IS A SPECIAL SESSION ID
             * WHEN IT IS PASSED TO THIS METHOD, A SIGINT IS GENERATED WHICH CANCELS ALL ONGOING
             * SESSIONS
             */
            FFmpegKitConfig.nativeFFmpegCancel(0L)
        }

        /**
         * <p>Cancels the session specified with <code>sessionId</code>.
         *
         * <p>This method does not wait for termination to complete and returns immediately.
         *
         * @param sessionId id of the session that will be cancelled
         */
        @JvmStatic
        fun cancel(sessionId: Long) {
            FFmpegKitConfig.nativeFFmpegCancel(sessionId)
        }

        /**
         * <p>Lists all FFmpeg sessions in the session history.
         *
         * @return all FFmpeg sessions in the session history
         */
        @JvmStatic
        fun listSessions(): List<FFmpegSession> = FFmpegKitConfig.getFFmpegSessions()
    }
}
