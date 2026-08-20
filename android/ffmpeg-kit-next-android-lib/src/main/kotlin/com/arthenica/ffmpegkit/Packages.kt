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
 * <p>Helper class to extract binary package information.
 */
open class Packages {

    companion object {

        private val supportedExternalLibraries: List<String> = listOf(
            "dav1d",
            "fontconfig",
            "freetype",
            "fribidi",
            "gmp",
            "gnutls",
            "harfbuzz",
            "kvazaar",
            "mp3lame",
            "libass",
            "libjxl",
            "liblc3",
            "libsvtav1",
            "iconv",
            "libilbc",
            "libtheora",
            "libvidstab",
            "libvorbis",
            "libvpx",
            "libwebp",
            "libxml2",
            "opencore-amr",
            "openh264",
            "openssl",
            "opus",
            "rubberband",
            "sdl2",
            "shine",
            "snappy",
            "soxr",
            "speex",
            "srt",
            "tesseract",
            "twolame",
            "vvenc",
            "x264",
            "x265",
            "xvid",
            "zimg"
        )

        /**
         * Returns the FFmpegKit binary package name.
         *
         * @return FFmpegKit binary package name
         */
        @JvmStatic
        fun getPackageName(): String {
            return NativeLoader.loadPackageName()
        }

        /**
         * Returns enabled external libraries by FFmpeg.
         *
         * @return enabled external libraries
         */
        @JvmStatic
        fun getExternalLibraries(): List<String> {
            val buildConfiguration = AbiDetect.getNativeBuildConf()

            val enabledLibraryList = ArrayList<String>()
            for (supportedExternalLibrary in supportedExternalLibraries) {
                if (buildConfiguration.contains("enable-$supportedExternalLibrary") ||
                    buildConfiguration.contains("enable-lib$supportedExternalLibrary")
                ) {
                    enabledLibraryList.add(supportedExternalLibrary)
                }
            }

            enabledLibraryList.sort()

            return enabledLibraryList
        }
    }
}
