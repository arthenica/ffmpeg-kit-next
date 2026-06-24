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
import java.util.Locale

/**
 * Seekable in-memory input that can be used in FFmpeg commands with an
 * {@code ffkitmem:} URL.
 */
class FFmpegKitInputBuffer private constructor(private val id: Long, extension: String?) : AutoCloseable {

    private val url: String = FFmpegKitUrlSupport.buildUrl("ffkitmem", id, extension)
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

    override fun close() {
        if (!closed) {
            FFmpegKitConfig.unregisterFFmpegKitBuffer(id)
            closed = true
        }
    }

    private fun ensureOpen() {
        if (closed) {
            throw IllegalStateException("FFmpegKit input buffer is closed.")
        }
    }

    companion object {

        @NonNull
        @JvmStatic
        fun fromByteArray(@NonNull data: ByteArray, @Nullable extension: String?): FFmpegKitInputBuffer {
            val id = FFmpegKitConfig.registerFFmpegKitInputBuffer(data)
            if (id == 0L) {
                throw IllegalStateException("Failed to register FFmpegKit input buffer.")
            }

            return FFmpegKitInputBuffer(id, extension)
        }

        @NonNull
        @JvmStatic
        fun fromDirectByteBuffer(@NonNull byteBuffer: ByteBuffer, size: Int, @Nullable extension: String?): FFmpegKitInputBuffer {
            if (!byteBuffer.isDirect) {
                throw IllegalArgumentException("byteBuffer must be a direct ByteBuffer")
            }
            if (size < 0 || size > byteBuffer.remaining()) {
                throw IllegalArgumentException("size must fit inside byteBuffer.remaining()")
            }

            val nativeView = byteBuffer.slice()
            val id = FFmpegKitConfig.registerFFmpegKitInputDirectBuffer(nativeView, size)
            if (id == 0L) {
                throw IllegalStateException("Failed to register FFmpegKit direct input buffer.")
            }

            return FFmpegKitInputBuffer(id, extension)
        }
    }

    internal object FFmpegKitUrlSupport {

        fun buildUrl(protocol: String, id: Long, extension: String?): String {
            val normalizedExtension = normalizeExtension(extension)
            return "$protocol:$id.$normalizedExtension"
        }

        private fun normalizeExtension(extension: String?): String {
            if (extension == null) {
                return "bin"
            }

            var normalized = extension.trim().lowercase(Locale.UK)
            while (normalized.startsWith(".")) {
                normalized = normalized.substring(1)
            }

            if (normalized.isEmpty()) {
                return "bin"
            }

            val safeExtension = StringBuilder()
            for (i in normalized.indices) {
                val c = normalized[i]
                if ((c in 'a'..'z') || (c in '0'..'9')) {
                    safeExtension.append(c)
                }
            }

            return if (safeExtension.isEmpty()) "bin" else safeExtension.toString()
        }
    }
}
