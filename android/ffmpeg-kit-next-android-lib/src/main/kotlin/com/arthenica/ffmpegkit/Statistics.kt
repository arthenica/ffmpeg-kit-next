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

/**
 * <p>Statistics entry for an FFmpeg execute session.
 */
open class Statistics(
    open var sessionId: Long,
    open var videoFrameNumber: Int,
    open var videoFps: Float,
    open var videoQuality: Float,
    open var size: Long,
    open var time: Double,
    open var bitrate: Double,
    open var speed: Double
) {

    override fun toString(): String = buildString {
        append("Statistics{")
        append("sessionId=")
        append(sessionId)
        append(", videoFrameNumber=")
        append(videoFrameNumber)
        append(", videoFps=")
        append(videoFps)
        append(", videoQuality=")
        append(videoQuality)
        append(", size=")
        append(size)
        append(", time=")
        append(time)
        append(", bitrate=")
        append(bitrate)
        append(", speed=")
        append(speed)
        append('}')
    }
}
