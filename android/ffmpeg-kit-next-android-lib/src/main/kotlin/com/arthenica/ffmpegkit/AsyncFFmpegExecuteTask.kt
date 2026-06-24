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
 * <p>Executes an FFmpeg session asynchronously.
 */
open class AsyncFFmpegExecuteTask(private val ffmpegSession: FFmpegSession) : Runnable {

    private val completeCallback: FFmpegSessionCompleteCallback? = ffmpegSession.getCompleteCallback()

    override fun run() {
        FFmpegKitConfig.ffmpegExecute(ffmpegSession)

        if (completeCallback != null) {
            try {
                // NOTIFY SESSION CALLBACK DEFINED
                completeCallback.apply(ffmpegSession)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside session complete callback.%s", Exceptions.getStackTraceString(e)))
            }
        }

        val globalFFmpegSessionCompleteCallback = FFmpegKitConfig.getFFmpegSessionCompleteCallback()
        if (globalFFmpegSessionCompleteCallback != null) {
            try {
                // NOTIFY GLOBAL CALLBACK DEFINED
                globalFFmpegSessionCompleteCallback.apply(ffmpegSession)
            } catch (e: Exception) {
                android.util.Log.e(FFmpegKitConfig.TAG, String.format("Exception thrown inside global complete callback.%s", Exceptions.getStackTraceString(e)))
            }
        }
    }
}
