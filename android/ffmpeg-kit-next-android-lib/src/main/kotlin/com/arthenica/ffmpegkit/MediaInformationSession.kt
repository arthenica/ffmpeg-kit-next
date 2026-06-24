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

/**
 * <p>A custom FFprobe session, which produces a <code>MediaInformation</code> object using the
 * FFprobe output.
 *
 * @param completeCallback Session specific complete callback.
 */
open class MediaInformationSession private constructor(
    arguments: Array<String>,
    private val completeCallback: MediaInformationSessionCompleteCallback?,
    logCallback: LogCallback?
) : AbstractSession(arguments, logCallback, LogRedirectionStrategy.NEVER_PRINT_LOGS), Session {

    /**
     * Media information extracted in the session.
     */
    private var mediaInformation: MediaInformation? = null

    /**
     * Returns the media information extracted in this session.
     *
     * @return media information extracted or null if the command failed or the output can not be
     * parsed
     */
    open fun getMediaInformation(): MediaInformation? = mediaInformation

    /**
     * Sets the media information extracted in this session.
     *
     * @param mediaInformation media information extracted
     */
    open fun setMediaInformation(mediaInformation: MediaInformation?) {
        this.mediaInformation = mediaInformation
    }

    /**
     * Returns the session specific complete callback.
     *
     * @return session specific complete callback
     */
    open fun getCompleteCallback(): MediaInformationSessionCompleteCallback? = completeCallback

    override fun isFFmpeg(): Boolean = false

    override fun isFFprobe(): Boolean = false

    override fun isMediaInformation(): Boolean = true

    override fun toString(): String = buildString {
        append("MediaInformationSession{")
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
         * Creates a new media information session.
         *
         * @param arguments command arguments
         * @return created session
         */
        @JvmStatic
        fun create(arguments: Array<String>): MediaInformationSession =
            MediaInformationSession(arguments, null, null)

        /**
         * Creates a new media information session.
         *
         * @param arguments        command arguments
         * @param completeCallback session specific complete callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: MediaInformationSessionCompleteCallback?
        ): MediaInformationSession =
            MediaInformationSession(arguments, completeCallback, null)

        /**
         * Creates a new media information session.
         *
         * @param arguments        command arguments
         * @param completeCallback session specific complete callback
         * @param logCallback      session specific log callback
         * @return created session
         */
        @JvmStatic
        fun create(
            arguments: Array<String>,
            completeCallback: MediaInformationSessionCompleteCallback?,
            logCallback: LogCallback?
        ): MediaInformationSession =
            MediaInformationSession(arguments, completeCallback, logCallback)
    }
}
