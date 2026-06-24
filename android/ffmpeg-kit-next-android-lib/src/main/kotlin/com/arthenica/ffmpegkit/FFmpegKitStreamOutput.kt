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
 * Non-seekable streaming output that can be used in FFmpeg commands with an
 * {@code ffkitstream:} URL.
 */
class FFmpegKitStreamOutput private constructor(private val id: Long, extension: String?) : AutoCloseable {

    private val url: String = FFmpegKitInputBuffer.FFmpegKitUrlSupport.buildUrl("ffkitstream", id, extension)
    private var closed: Boolean = false

    @NonNull
    fun getUrl(): String {
        ensureOpen()
        return url
    }

    @Nullable
    fun read(maxBytes: Int): ByteArray? {
        return read(maxBytes, -1)
    }

    /**
     * Reads at most {@code maxBytes}. Returns {@code null} on timeout and an
     * empty array when FFmpeg closes the output stream.
     */
    @Nullable
    fun read(maxBytes: Int, timeoutMs: Int): ByteArray? {
        ensureOpen()
        if (maxBytes < 0) {
            throw IllegalArgumentException("maxBytes must not be negative")
        }

        return FFmpegKitConfig.readFFmpegKitStream(id, maxBytes, timeoutMs)
    }

    override fun close() {
        if (!closed) {
            FFmpegKitConfig.unregisterFFmpegKitStream(id)
            closed = true
        }
    }

    private fun ensureOpen() {
        if (closed) {
            throw IllegalStateException("FFmpegKit output stream is closed.")
        }
    }

    companion object {

        private const val DEFAULT_CAPACITY: Long = 1024 * 1024

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?): FFmpegKitStreamOutput {
            return create(extension, DEFAULT_CAPACITY)
        }

        @NonNull
        @JvmStatic
        fun create(@Nullable extension: String?, capacity: Long): FFmpegKitStreamOutput {
            if (capacity <= 0) {
                throw IllegalArgumentException("capacity must be positive")
            }

            val id = FFmpegKitConfig.registerFFmpegKitStream(capacity, FFmpegKitConfig.FFMPEG_KIT_STREAM_TYPE_OUTPUT)
            if (id == 0L) {
                throw IllegalStateException("Failed to register FFmpegKit output stream.")
            }

            return FFmpegKitStreamOutput(id, extension)
        }
    }
}
