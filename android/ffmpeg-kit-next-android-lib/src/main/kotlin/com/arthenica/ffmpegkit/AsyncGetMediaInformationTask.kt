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

import com.arthenica.smartexception.java.Exceptions

/**
 * <p>Executes a MediaInformation session asynchronously.
 */
open class AsyncGetMediaInformationTask @JvmOverloads constructor(
    private val mediaInformationSession: MediaInformationSession,
    private val waitTimeout: Int = AbstractSession.DEFAULT_TIMEOUT_FOR_ASYNCHRONOUS_MESSAGES_IN_TRANSMIT
) : Runnable {

    private val completeCallback: MediaInformationSessionCompleteCallback? = mediaInformationSession.getCompleteCallback()

    override fun run() {
        FFmpegKitConfig.getMediaInformationExecute(mediaInformationSession, waitTimeout)

        if (completeCallback != null) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback.apply(mediaInformationSession)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside session complete callback.%s", Exceptions.getStackTraceString(e)))
            }
        }

        val globalMediaInformationSessionCompleteCallback = FFmpegKitConfig.getMediaInformationSessionCompleteCallback()
        if (globalMediaInformationSessionCompleteCallback != null) {
            try {
                // NOTIFY GLOBAL CALLBACK DEFINEDs
                globalMediaInformationSessionCompleteCallback.apply(mediaInformationSession)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside global complete callback.%s", Exceptions.getStackTraceString(e)))
            }
        }
    }
}
