/*
 * Copyright (c) 2020-2022, 2026 Taner Sener
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

/**
 * <p>An FFprobe session.
 *
 * @param arguments command arguments as an array
 * @param completeCallback specific complete callback
 * @param logCallback session specific log callback
 * @param logRedirectionStrategy session specific log redirection strategy
 */
open class FFprobeSession private constructor(
    arguments: Array<String>,
    private val completeCallback: FFprobeSessionCompleteCallback?,
    logCallback: LogCallback?,
    logRedirectionStrategy: LogRedirectionStrategy
) : AbstractSession(arguments, logCallback, logRedirectionStrategy), Session {

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    open fun getCompleteCallback(): FFprobeSessionCompleteCallback? = completeCallback

    override fun isFFmpeg(): Boolean = false

    override fun isFFprobe(): Boolean = true

    override fun isMediaInformation(): Boolean = false

    override fun toString(): String = buildString {
        append("FFprobeSession{")
        append("sessionId=")
        append(sessionId)
        append(", createTime=")
        append(createTime)
        append(", startTime=")
        append(startTime)
        append(", endTime=")
        append(endTime)
        append(", arguments=")
        append(FFmpegKitConfig.argumentsToString(arguments))
        append(", logs=")
        append(getLogsAsString())
        append(", state=")
        append(state)
        append(", returnCode=")
        append(returnCode)
        append(", failStackTrace=")
        append('\'')
        append(failStackTrace)
        append('\'')
        append('}')
    }

    companion object {

        /**
         * Builds a new FFprobe session.
         *
         * @param arguments command arguments
         * @return created session
         */
        @JvmStatic
        fun create(arguments: Array<String>): FFprobeSession =
            FFprobeSession(arguments, null, null, FFmpegKitConfig.getLogRedirectionStrategy())

        /**
         * Builds a new FFprobe session.
         *
         * @param arguments        command arguments
         * @param completeCallback session specific complete callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFprobeSessionCompleteCallback?
        ): FFprobeSession =
            FFprobeSession(
                arguments,
                completeCallback,
                null,
                FFmpegKitConfig.getLogRedirectionStrategy()
            )

        /**
         * Builds a new FFprobe session.
         *
         * @param arguments        command arguments
         * @param completeCallback session specific complete callback
         * @param logCallback      session specific log callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFprobeSessionCompleteCallback?,
            logCallback: LogCallback?
        ): FFprobeSession =
            FFprobeSession(
                arguments,
                completeCallback,
                logCallback,
                FFmpegKitConfig.getLogRedirectionStrategy()
            )

        /**
         * Builds a new FFprobe session.
         *
         * @param arguments              command arguments
         * @param completeCallback       session specific complete callback
         * @param logCallback            session specific log callback
         * @param logRedirectionStrategy session specific log redirection strategy
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFprobeSessionCompleteCallback?,
            logCallback: LogCallback?,
            logRedirectionStrategy: LogRedirectionStrategy
        ): FFprobeSession =
            FFprobeSession(arguments, completeCallback, logCallback, logRedirectionStrategy)
    }
}
