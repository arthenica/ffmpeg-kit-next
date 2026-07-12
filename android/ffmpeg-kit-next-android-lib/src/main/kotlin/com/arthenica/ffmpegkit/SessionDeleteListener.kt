/*
 * Copyright (c) 2026 Taner Sener
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
 * You should have received a copy of FFmpegKitNext. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit

/**
 * Listener notified when a session is deleted from the native session history.
 */
interface SessionDeleteListener {
    /**
     * Called after the session identified by [sessionId] has been deleted from session history.
     */
    fun sessionDeleted(sessionId: Long)
}
