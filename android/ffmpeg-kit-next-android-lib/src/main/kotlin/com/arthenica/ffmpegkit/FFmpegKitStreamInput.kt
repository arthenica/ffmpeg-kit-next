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

/**
 * Non-seekable streaming input that can be used in FFmpeg commands with an
 * {@code ffkitstream:} URL.
 */
class FFmpegKitStreamInput private constructor(private val id: Long, extension: String?) : AutoCloseable {

    private val url: String = FFmpegKitInputBuffer.FFmpegKitUrlSupport.buildUrl("ffkitstream", id, extension)
    private var closed: Boolean = false
    private var inputClosed: Boolean = false

    @NonNull
    fun getUrl(): String {
        ensureOpen()
        return url
    }

    fun write(@NonNull data: ByteArray): Int {
        return write(data, 0, data.size, -1)
    }

    fun write(@NonNull data: ByteArray, timeoutMs: Int): Int {
        return write(data, 0, data.size, timeoutMs)
    }

    fun write(@NonNull data: ByteArray, offset: Int, length: Int, timeoutMs: Int): Int {
        ensureOpen()
        if (inputClosed) {
            throw IllegalStateException("FFmpegKit input stream is closed for writing.")
        }
        if (offset < 0 || length < 0 || offset + length > data.size) {
            throw IllegalArgumentException("offset and length must fit inside data")
        }

        val written = FFmpegKitConfig.writeFFmpegKitStream(id, data, offset, length, timeoutMs)
        if (written < 0) {
            throw IllegalStateException(String.format("Failed to write FFmpegKit input stream: %d.", written))
        }
        return written
    }

    fun closeInput() {
        if (!closed && !inputClosed) {
            FFmpegKitConfig.closeFFmpegKitStreamInput(id)
            inputClosed = true
        }
    }

    override fun close() {
        if (!closed) {
            closeInput()
            FFmpegKitConfig.unregisterFFmpegKitStream(id)
            closed = true
        }
    }

    private fun ensureOpen() {
        if (closed) {
            throw IllegalStateException("FFmpegKit input stream is closed.")
        }
    }

    companion object {

        private const val DEFAULT_CAPACITY: Long = 1024 * 1024

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?): FFmpegKitStreamInput {
            return create(extension, DEFAULT_CAPACITY)
        }

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?, capacity: Long): FFmpegKitStreamInput {
            if (capacity <= 0) {
                throw IllegalArgumentException("capacity must be positive")
            }

            val id = FFmpegKitConfig.registerFFmpegKitStream(capacity, FFmpegKitConfig.FFMPEG_KIT_STREAM_TYPE_INPUT)
            if (id == 0L) {
                throw IllegalStateException("Failed to register FFmpegKit input stream.")
            }

            return FFmpegKitStreamInput(id, extension)
        }
    }
}
