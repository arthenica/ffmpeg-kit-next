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
 * You should have received a copy of the GNU Lesser General License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit

import androidx.annotation.NonNull
import androidx.annotation.Nullable
import java.nio.ByteBuffer

/**
 * Seekable in-memory output that can be used in FFmpeg commands with an
 * {@code ffkitmem:} URL.
 */
class FFmpegKitOutputBuffer private constructor(private val id: Long, extension: String?) : AutoCloseable {

    private val url: String = FFmpegKitInputBuffer.FFmpegKitUrlSupport.buildUrl("ffkitmem", id, extension)
    private var closed: Boolean = false

    @NonNull
    fun getUrl(): String {
        ensureOpen()
        return url
    }

    fun getSize(): Long {
        ensureOpen()
        return FFmpegKitConfig.getFFmpegKitBufferSize(id)
    }

    @NonNull
    fun toByteArray(): ByteArray {
        ensureOpen()
        val output = FFmpegKitConfig.getFFmpegKitOutputBuffer(id)
            ?: throw IllegalStateException("Failed to read FFmpegKit output buffer.")
        return output
    }

    @NonNull
    fun asDirectByteBuffer(): ByteBuffer {
        ensureOpen()
        val output = FFmpegKitConfig.getFFmpegKitOutputBufferDirect(id)
        return if (output == null) ByteBuffer.allocateDirect(0) else output.asReadOnlyBuffer()
    }

    override fun close() {
        if (!closed) {
            FFmpegKitConfig.unregisterFFmpegKitBuffer(id)
            closed = true
        }
    }

    private fun ensureOpen() {
        if (closed) {
            throw IllegalStateException("FFmpegKit output buffer is closed.")
        }
    }

    companion object {

        private const val DEFAULT_INITIAL_CAPACITY: Long = 4096

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?): FFmpegKitOutputBuffer {
            return create(extension, DEFAULT_INITIAL_CAPACITY, 0)
        }

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?, initialCapacity: Long, maxCapacity: Long): FFmpegKitOutputBuffer {
            if (initialCapacity < 0 || maxCapacity < 0) {
                throw IllegalArgumentException("capacities must not be negative")
            }

            val id = FFmpegKitConfig.registerFFmpegKitOutputBuffer(initialCapacity, maxCapacity)
            if (id == 0L) {
                throw IllegalStateException("Failed to register FFmpegKit output buffer.")
            }

            return FFmpegKitOutputBuffer(id, extension)
        }
    }
}
