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
 * <p>Enumeration type for log levels.
 */
enum class Level(open val value: Int) {

    /**
     * This log level is defined by FFmpegKit. It is used to specify logs printed to stderr by
     * FFmpeg. Logs that has this level are not filtered and always redirected.
     */
    AV_LOG_STDERR(-16),

    /**
     * Print no output.
     */
    AV_LOG_QUIET(-8),

    /**
     * Something went really wrong and we will crash now.
     */
    AV_LOG_PANIC(0),

    /**
     * Something went wrong and recovery is not possible.
     * For example, no header was found for a format which depends
     * on headers or an illegal combination of parameters is used.
     */
    AV_LOG_FATAL(8),

    /**
     * Something went wrong and cannot losslessly be recovered.
     * However, not all future data is affected.
     */
    AV_LOG_ERROR(16),

    /**
     * Something somehow does not look correct. This may or may not
     * lead to problems. An example would be the use of '-vstrict -2'.
     */
    AV_LOG_WARNING(24),

    /**
     * Standard information.
     */
    AV_LOG_INFO(32),

    /**
     * Detailed information.
     */
    AV_LOG_VERBOSE(40),

    /**
     * Stuff which is only useful for libav* developers.
     */
    AV_LOG_DEBUG(48),

    /**
     * Extremely verbose debugging, useful for libav* development.
     */
    AV_LOG_TRACE(56);

    companion object {
        /**
         * <p>Returns the enumeration defined by provided value.
         *
         * @param value level value
         * @return enumeration defined by value
         */
        @JvmStatic
        fun from(value: Int): Level = when (value) {
            AV_LOG_STDERR.value -> AV_LOG_STDERR
            AV_LOG_QUIET.value -> AV_LOG_QUIET
            AV_LOG_PANIC.value -> AV_LOG_PANIC
            AV_LOG_FATAL.value -> AV_LOG_FATAL
            AV_LOG_ERROR.value -> AV_LOG_ERROR
            AV_LOG_WARNING.value -> AV_LOG_WARNING
            AV_LOG_INFO.value -> AV_LOG_INFO
            AV_LOG_VERBOSE.value -> AV_LOG_VERBOSE
            AV_LOG_DEBUG.value -> AV_LOG_DEBUG
            else -> AV_LOG_TRACE
        }
    }
}
