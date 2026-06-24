/*
 * Copyright (c) 2020-2021, 2026 Taner Sener
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
 * <p>Main class to run <code>FFprobe</code> commands. Supports executing commands both
 * synchronously and asynchronously.
 * <pre>
 * FFprobeSession session = FFprobeKit.execute("-hide_banner -v error -show_entries format=size -of default=noprint_wrappers=1 file1.mp4");
 *
 * FFprobeSession asyncSession = FFprobeKit.executeAsync("-hide_banner -v error -show_entries format=size -of default=noprint_wrappers=1 file1.mp4", completeCallback);
 * </pre>
 * <p>Provides overloaded <code>execute</code> methods to define session specific callbacks.
 * <pre>
 * FFprobeSession session = FFprobeKit.executeAsync("-hide_banner -v error -show_entries format=size -of default=noprint_wrappers=1 file1.mp4", completeCallback, logCallback);
 * </pre>
 * <p>It can extract media information for a file or a url, using [getMediaInformation] method.
 * <pre>
 * MediaInformationSession session = FFprobeKit.getMediaInformation("file1.mp4");
 * </pre>
 */
open class FFprobeKit private constructor() {
    companion object {
        init {
            AbiDetect::class.java.name
            FFmpegKitConfig::class.java.name
        }

        /**
         * <p>Builds the default command used to get media information for a file.
         *
         * @param path file path to use in the command
         * @return default command arguments to get media information
         */
        private fun defaultGetMediaInformationCommandArguments(path: String): Array<String> =
            arrayOf("-v", "error", "-hide_banner", "-print_format", "json", "-show_format", "-show_streams", "-show_chapters", "-i", path)

        /**
         * <p>Synchronously executes FFprobe with arguments provided.
         *
         * @param arguments FFprobe command options/arguments as string array
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeWithArguments(arguments: Array<String>): FFprobeSession {
            val session = FFprobeSession.create(arguments)

            FFmpegKitConfig.ffprobeExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFprobe command options/arguments as string array
         * @param completeCallback callback that will be called when the execution has completed
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFprobeSessionCompleteCallback?): FFprobeSession {
            val session = FFprobeSession.create(arguments, completeCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFprobe command options/arguments as string array
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFprobeSessionCompleteCallback?, logCallback: LogCallback?): FFprobeSession {
            val session = FFprobeSession.create(arguments, completeCallback, logCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFprobe command options/arguments as string array
         * @param completeCallback callback that will be called when the execution has completed
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFprobeSessionCompleteCallback?, executorService: ExecutorService): FFprobeSession {
            val session = FFprobeSession.create(arguments, completeCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session, executorService)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution with arguments provided.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param arguments        FFprobe command options/arguments as string array
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeWithArgumentsAsync(arguments: Array<String>, completeCallback: FFprobeSessionCompleteCallback?, logCallback: LogCallback?, executorService: ExecutorService): FFprobeSession {
            val session = FFprobeSession.create(arguments, completeCallback, logCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session, executorService)

            return session
        }

        /**
         * <p>Synchronously executes FFprobe command provided. Space character is used to split command
         * into arguments. You can use single or double quote characters to specify arguments inside
         * your command.
         *
         * @param command FFprobe command
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun execute(command: String): FFprobeSession =
            executeWithArguments(FFmpegKitConfig.parseArguments(command))

        /**
         * <p>Starts an asynchronous FFprobe execution for the given command. Space character is used
         * to split the command into arguments. You can use single or double quote characters to
         * specify arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFprobe command
         * @param completeCallback callback that will be called when the execution has completed
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFprobeSessionCompleteCallback?): FFprobeSession =
            executeWithArgumentsAsync(FFmpegKitConfig.parseArguments(command), completeCallback)

        /**
         * <p>Starts an asynchronous FFprobe execution for the given command. Space character is used
         * to split the command into arguments. You can use single or double quote characters to
         * specify arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFprobe command
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFprobeSessionCompleteCallback?, logCallback: LogCallback?): FFprobeSession =
            executeWithArgumentsAsync(FFmpegKitConfig.parseArguments(command), completeCallback, logCallback)

        /**
         * <p>Starts an asynchronous FFprobe execution for the given command. Space character is used
         * to split the command into arguments. You can use single or double quote characters to
         * specify arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFprobe command
         * @param completeCallback callback that will be called when the execution has completed
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFprobeSessionCompleteCallback?, executorService: ExecutorService): FFprobeSession {
            val session = FFprobeSession.create(FFmpegKitConfig.parseArguments(command), completeCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session, executorService)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution for the given command. Space character is used
         * to split the command into arguments. You can use single or double quote characters to
         * specify arguments inside your command.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an [FFprobeSessionCompleteCallback] if you want to be notified about the
         * result.
         *
         * @param command          FFprobe command
         * @param completeCallback callback that will be called when the execution has completed
         * @param logCallback      callback that will receive logs
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return FFprobe session created for this execution
         */
        @JvmStatic
        fun executeAsync(command: String, completeCallback: FFprobeSessionCompleteCallback?, logCallback: LogCallback?, executorService: ExecutorService): FFprobeSession {
            val session = FFprobeSession.create(FFmpegKitConfig.parseArguments(command), completeCallback, logCallback)

            FFmpegKitConfig.asyncFFprobeExecute(session, executorService)

            return session
        }

        /**
         * <p>Extracts media information for the file specified with path.
         *
         * @param path path or uri of a media file
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformation(path: String): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path))

            FFmpegKitConfig.getMediaInformationExecute(session, AbstractSession.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

            return session
        }

        /**
         * <p>Extracts media information for the file specified with path.
         *
         * @param path        path or uri of a media file
         * @param waitTimeout max time to wait until media information is transmitted
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformation(path: String, waitTimeout: Int): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path))

            FFmpegKitConfig.getMediaInformationExecute(session, waitTimeout)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution to extract the media information for the
         * specified file.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param path             path or uri of a media file
         * @param completeCallback callback that will be called when the execution has completed
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationAsync(path: String, completeCallback: MediaInformationSessionCompleteCallback?): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path), completeCallback)

            FFmpegKitConfig.asyncGetMediaInformationExecute(session, AbstractSession.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution to extract the media information for the
         * specified file.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param path             path or uri of a media file
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @param waitTimeout      max time to wait until media information is transmitted
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationAsync(path: String, completeCallback: MediaInformationSessionCompleteCallback?, logCallback: LogCallback?, waitTimeout: Int): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path), completeCallback, logCallback)

            FFmpegKitConfig.asyncGetMediaInformationExecute(session, waitTimeout)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution to extract the media information for the
         * specified file.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param path             path or uri of a media file
         * @param completeCallback callback that will be called when the execution has completed
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationAsync(path: String, completeCallback: MediaInformationSessionCompleteCallback?, executorService: ExecutorService): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path), completeCallback)

            FFmpegKitConfig.asyncGetMediaInformationExecute(session, executorService, AbstractSession.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution to extract the media information for the
         * specified file.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param path             path or uri of a media file
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @param executorService  executor service that will be used to run this asynchronous operation
         * @param waitTimeout      max time to wait until media information is transmitted
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationAsync(path: String, completeCallback: MediaInformationSessionCompleteCallback?, logCallback: LogCallback?, executorService: ExecutorService, waitTimeout: Int): MediaInformationSession {
            val session = MediaInformationSession.create(defaultGetMediaInformationCommandArguments(path), completeCallback, logCallback)

            FFmpegKitConfig.asyncGetMediaInformationExecute(session, executorService, waitTimeout)

            return session
        }

        /**
         * <p>Extracts media information using the command provided.
         *
         * @param command FFprobe command that prints media information for a file in JSON format
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationFromCommand(command: String): MediaInformationSession {
            val session = MediaInformationSession.create(FFmpegKitConfig.parseArguments(command))

            FFmpegKitConfig.getMediaInformationExecute(session, AbstractSession.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

            return session
        }

        /**
         * <p>Starts an asynchronous FFprobe execution to extract media information using a command.
         * The command passed to this method must generate the output in JSON format in order to
         * successfully extract media information from it.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param command          FFprobe command that prints media information for a file in JSON
         *                         format
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @param waitTimeout      max time to wait until media information is transmitted
         * @return media information session created for this execution
         */
        @JvmStatic
        fun getMediaInformationFromCommandAsync(command: String, completeCallback: MediaInformationSessionCompleteCallback?, logCallback: LogCallback?, waitTimeout: Int): MediaInformationSession =
            getMediaInformationFromCommandArgumentsAsync(FFmpegKitConfig.parseArguments(command), completeCallback, logCallback, waitTimeout)

        /**
         * <p>Starts an asynchronous FFprobe execution to extract media information using command
         * arguments. The command passed to this method must generate the output in JSON format in
         * order to successfully extract media information from it.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a [MediaInformationSessionCompleteCallback] if you want to be notified
         * about the result.
         *
         * @param arguments        FFprobe command arguments that print media information for a file in
         *                         JSON format
         * @param completeCallback callback that will be notified when execution has completed
         * @param logCallback      callback that will receive logs
         * @param waitTimeout      max time to wait until media information is transmitted
         * @return media information session created for this execution
         */
        private fun getMediaInformationFromCommandArgumentsAsync(arguments: Array<String>, completeCallback: MediaInformationSessionCompleteCallback?, logCallback: LogCallback?, waitTimeout: Int): MediaInformationSession {
            val session = MediaInformationSession.create(arguments, completeCallback, logCallback)

            FFmpegKitConfig.asyncGetMediaInformationExecute(session, waitTimeout)

            return session
        }

        /**
         * <p>Lists all FFprobe sessions in the session history.
         *
         * @return all FFprobe sessions in the session history
         */
        @JvmStatic
        fun listFFprobeSessions(): List<FFprobeSession> = FFmpegKitConfig.getFFprobeSessions()

        /**
         * <p>Lists all MediaInformation sessions in the session history.
         *
         * @return all MediaInformation sessions in the session history
         */
        @JvmStatic
        fun listMediaInformationSessions(): List<MediaInformationSession> = FFmpegKitConfig.getMediaInformationSessions()
    }
}
