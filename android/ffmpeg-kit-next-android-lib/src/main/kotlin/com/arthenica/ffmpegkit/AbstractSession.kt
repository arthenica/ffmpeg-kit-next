/*
 * Copyright (c) 2021, 2026 Taner Sener
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

import com.arthenica.ffmpegkit.AbstractSession.Companion.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT
import com.arthenica.smartexception.java.Exceptions
import java.util.Date
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicLong

/**
 * Abstract session implementation which includes common features shared by `FFmpeg`,
 * `FFprobe` and `MediaInformation` sessions.
 *
 * @param arguments command arguments as an array
 * @param logCallback session specific log callback
 * @param logRedirectionStrategy session specific log redirection strategy
 */
abstract class AbstractSession protected constructor(
    arguments: Array<String>,
    @JvmField protected val logCallback: LogCallback?,
    @JvmField protected val logRedirectionStrategy: LogRedirectionStrategy
) : Session {

    @JvmField
    protected val arguments: Array<String> = arguments.copyOf()

    /**
     * Session identifier.
     */
    @JvmField
    protected val sessionId: Long = sessionIdGenerator.getAndIncrement()

    /**
     * Date and time the session was created.
     */
    @JvmField
    protected val createTime: Date = Date()

    /**
     * Date and time the session was started.
     */
    @JvmField
    protected var startTime: Date? = null

    /**
     * Date and time the session has ended.
     */
    @JvmField
    protected var endTime: Date? = null

    /**
     * Log entries received for this session.
     */
    @JvmField
    protected val logs: MutableList<Log> = ArrayList()

    /**
     * Log entry lock.
     */
    @JvmField
    protected val logsLock: Any = Any()

    /**
     * Future created for sessions executed asynchronously.
     */
    @JvmField
    protected var future: Future<*>? = null

    /**
     * State of the session.
     */
    @JvmField
    protected var state: SessionState = SessionState.CREATED

    /**
     * Return code for the completed sessions.
     */
    @JvmField
    protected var returnCode: ReturnCode? = null

    /**
     * Stack trace of the error received while trying to execute this session.
     */
    @JvmField
    protected var failStackTrace: String? = null

    init {
        FFmpegKitConfig.addSession(this)
    }

    override fun getLogCallback(): LogCallback? = logCallback

    override fun getSessionId(): Long = sessionId

    override fun getCreateTime(): Date = Date(createTime.time)

    override fun getStartTime(): Date? = startTime?.let { Date(it.time) }

    override fun getEndTime(): Date? = endTime?.let { Date(it.time) }

    override fun getDuration(): Long {
        val startTime = this.startTime
        val endTime = this.endTime
        if (startTime != null && endTime != null) {
            return endTime.time - startTime.time
        }

        return 0
    }

    override fun getArguments(): Array<String> = arguments.copyOf()

    override fun getCommand(): String = FFmpegKitConfig.argumentsToString(arguments)

    override fun getAllLogs(waitTimeout: Int): List<Log> {
        waitForAsynchronousMessagesInTransmit(waitTimeout)

        if (thereAreAsynchronousMessagesInTransmit()) {
            android.util.Log.i(
                FFmpegKitConfig.TAG,
                String.format(
                    "getAllLogs was called to return all logs but there are still logs being transmitted for session id %d.",
                    sessionId
                )
            )
        }

        return getLogs()
    }

    /**
     * Returns all log entries generated for this session. If there are asynchronous
     * messages that are not delivered yet, this method waits for them until
     * [DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT] expires.
     *
     * @return list of log entries generated for this session
     */
    override fun getAllLogs(): List<Log> =
        getAllLogs(DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

    override fun getLogs(): List<Log> {
        synchronized(logsLock) {
            return ArrayList(logs)
        }
    }

    override fun getAllLogsAsString(waitTimeout: Int): String {
        waitForAsynchronousMessagesInTransmit(waitTimeout)

        if (thereAreAsynchronousMessagesInTransmit()) {
            android.util.Log.i(
                FFmpegKitConfig.TAG,
                String.format(
                    "getAllLogsAsString was called to return all logs but there are still logs being transmitted for session id %d.",
                    sessionId
                )
            )
        }

        return getLogsAsString()
    }

    /**
     * Returns all log entries generated for this session as a concatenated string. If there are
     * asynchronous messages that are not delivered yet, this method waits for them until
     * [DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT] expires.
     *
     * @return all log entries generated for this session as a concatenated string
     */
    override fun getAllLogsAsString(): String =
        getAllLogsAsString(DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT)

    override fun getLogsAsString(): String {
        val concatenatedString = StringBuilder()

        synchronized(logsLock) {
            for (log in logs) {
                concatenatedString.append(log.message)
            }
        }

        return concatenatedString.toString()
    }

    override fun getOutput(): String = getAllLogsAsString()

    override fun getState(): SessionState = state

    override fun getReturnCode(): ReturnCode? = returnCode

    override fun getFailStackTrace(): String? = failStackTrace

    override fun getLogRedirectionStrategy(): LogRedirectionStrategy = logRedirectionStrategy

    override fun thereAreAsynchronousMessagesInTransmit(): Boolean =
        FFmpegKitConfig.messagesInTransmit(sessionId) != 0

    override fun addLog(log: Log) {
        synchronized(logsLock) {
            this.logs.add(log)
        }
    }

    override fun getFuture(): Future<*>? = future

    override fun cancel() {
        if (state == SessionState.RUNNING) {
            FFmpegKit.cancel(sessionId)
        }
    }

    /**
     * Waits for all asynchronous messages to be transmitted until the given timeout.
     *
     * @param timeout wait timeout in milliseconds
     */
    protected open fun waitForAsynchronousMessagesInTransmit(timeout: Int) {
        val start = System.currentTimeMillis()

        while (thereAreAsynchronousMessagesInTransmit() && (System.currentTimeMillis() < (start + timeout))) {
            try {
                Thread.sleep(100)
            } catch (_: InterruptedException) {
                Thread.currentThread().interrupt()
                return
            }
        }
    }

    /**
     * Sets the future created for this session.
     *
     * @param future future that runs this session asynchronously
     */
    internal fun setFuture(future: Future<*>?) {
        this.future = future
    }

    /**
     * Starts running the session.
     */
    fun startRunning() {
        this.state = SessionState.RUNNING
        this.startTime = Date()
    }

    /**
     * Completes running the session with the provided return code.
     *
     * @param returnCode return code of the execution
     */
    fun complete(returnCode: ReturnCode?) {
        this.returnCode = returnCode
        this.state = SessionState.COMPLETED
        this.endTime = Date()
    }

    /**
     * Ends running the session with a failure.
     *
     * @param exception execution received
     */
    fun fail(exception: Exception?) {
        this.failStackTrace = Exceptions.getStackTraceString(exception)
        this.state = SessionState.FAILED
        this.endTime = Date()
    }

    companion object {

        /**
         * Generates unique ids for sessions.
         */
        @JvmField
        protected val sessionIdGenerator = AtomicLong(1)

        /**
         * Defines how long default "getAll" methods wait, in milliseconds.
         */
        const val DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT = 5000
    }
}
