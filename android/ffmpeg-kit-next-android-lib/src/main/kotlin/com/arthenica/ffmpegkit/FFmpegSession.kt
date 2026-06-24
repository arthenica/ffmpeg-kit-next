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

/**
 * <p>An FFmpeg session.
 *
 * @param arguments command arguments as an array
 * @param completeCallback specific complete callback
 * @param logCallback session specific log callback
 * @param statisticsCallback specific statistics callback
 * @param logRedirectionStrategy session specific log redirection strategy
 */
open class FFmpegSession private constructor(
    arguments: Array<String>,
    private val completeCallback: FFmpegSessionCompleteCallback?,
    logCallback: LogCallback?,
    private val statisticsCallback: StatisticsCallback?,
    logRedirectionStrategy: LogRedirectionStrategy
) : AbstractSession(arguments, logCallback, logRedirectionStrategy), Session {

    /**
     * Statistics entries received for this session.
     */
    private val statistics: MutableList<Statistics> = ArrayList()

    /**
     * Statistics entry lock.
     */
    private val statisticsLock = Any()

    /**
     * Returns the session specific statistics callback.
     *
     * @return session specific statistics callback
     */
    open fun getStatisticsCallback(): StatisticsCallback? = statisticsCallback

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    open fun getCompleteCallback(): FFmpegSessionCompleteCallback? = completeCallback

    /**
     * Returns all statistics entries generated for this session. If there are asynchronous
     * messages that are not delivered yet, this method waits for them until the given timeout.
     *
     * @param waitTimeout wait timeout for asynchronous messages in milliseconds
     * @return list of statistics entries generated for this session
     */
    open fun getAllStatistics(waitTimeout: Int): List<Statistics> {
        waitForAsynchronousMessagesInTransmit(waitTimeout)

        if (thereAreAsynchronousMessagesInTransmit()) {
            android.util.Log.i(
                FFmpegKitConfig.TAG,
                String.format(
                    "getAllStatistics was called to return all statistics but there are still statistics being transmitted for session id %d.",
                    sessionId
                )
            )
        }

        return getStatistics()
    }

    /**
     * Returns all statistics entries generated for this session. If there are asynchronous
     * messages that are not delivered yet, this method waits for them until
     * [DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT] expires.
     *
     * @return list of statistics entries generated for this session
     */
    open fun getAllStatistics(): List<Statistics> =
        getAllStatistics(DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

    /**
     * Returns all statistics entries delivered for this session. Note that if there are
     * asynchronous messages that are not delivered yet, this method will not wait for
     * them and will return immediately.
     *
     * @return list of statistics entries received for this session
     */
    open fun getStatistics(): List<Statistics> {
        synchronized(statisticsLock) {
            return ArrayList(statistics)
        }
    }

    /**
     * Returns the last received statistics entry.
     *
     * @return the last received statistics entry or null if there are not any statistics entries
     * received
     */
    open fun getLastReceivedStatistics(): Statistics? {
        synchronized(statisticsLock) {
            return if (statistics.isNotEmpty()) statistics[statistics.size - 1] else null
        }
    }

    /**
     * Adds a new statistics entry for this session. It is invoked internally by
     * `FFmpegKit` library methods. Must not be used by user applications.
     *
     * @param statistics statistics entry
     */
    open fun addStatistics(statistics: Statistics) {
        synchronized(statisticsLock) {
            this.statistics.add(statistics)
        }
    }

    override fun isFFmpeg(): Boolean = true

    override fun isFFprobe(): Boolean = false

    override fun isMediaInformation(): Boolean = false

    override fun toString(): String = buildString {
        append("FFmpegSession{")
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
         * Builds a new FFmpeg session.
         *
         * @param arguments command arguments
         * @return created session
         */
        @JvmStatic
        fun create(arguments: Array<String>): FFmpegSession =
            FFmpegSession(arguments, null, null, null, FFmpegKitConfig.getLogRedirectionStrategy())

        /**
         * Builds a new FFmpeg session.
         *
         * @param arguments        command arguments
         * @param completeCallback session specific complete callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFmpegSessionCompleteCallback?
        ): FFmpegSession =
            FFmpegSession(
                arguments,
                completeCallback,
                null,
                null,
                FFmpegKitConfig.getLogRedirectionStrategy()
            )

        /**
         * Builds a new FFmpeg session.
         *
         * @param arguments          command arguments
         * @param completeCallback   session specific complete callback
         * @param logCallback        session specific log callback
         * @param statisticsCallback session specific statistics callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFmpegSessionCompleteCallback?,
            logCallback: LogCallback?,
            statisticsCallback: StatisticsCallback?
        ): FFmpegSession =
            FFmpegSession(
                arguments,
                completeCallback,
                logCallback,
                statisticsCallback,
                FFmpegKitConfig.getLogRedirectionStrategy()
            )

        /**
         * Builds a new FFmpeg session.
         *
         * @param arguments              command arguments
         * @param completeCallback       session specific complete callback
         * @param logCallback            session specific log callback
         * @param statisticsCallback     session specific statistics callback
         * @param logRedirectionStrategy session specific log redirection strategy
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: FFmpegSessionCompleteCallback?,
            logCallback: LogCallback?,
            statisticsCallback: StatisticsCallback?,
            logRedirectionStrategy: LogRedirectionStrategy
        ): FFmpegSession =
            FFmpegSession(
                arguments,
                completeCallback,
                logCallback,
                statisticsCallback,
                logRedirectionStrategy
            )
    }
}
