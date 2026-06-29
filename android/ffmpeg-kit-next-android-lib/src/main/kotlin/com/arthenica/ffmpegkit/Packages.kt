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
         * @return predicted FFmpegKit binary package name
         */
        @JvmStatic
        @Deprecated("FFmpegKitNext doesnt provide any packages.")
        fun getPackageName(): String {
            val nativePackageName = FFmpegKitConfig.getNativePackageName()
            if (nativePackageName.isNotEmpty()) {
                return nativePackageName
            }
            val externalLibraryList = getExternalLibraries()
            val speex = externalLibraryList.contains("speex")
            val fribidi = externalLibraryList.contains("fribidi")
            val gnutls = externalLibraryList.contains("gnutls")
            val xvid = externalLibraryList.contains("xvid")

            var minGpl = false
            var https = false
            var httpsGpl = false
            var audio = false
            var video = false
            var full = false
            var fullGpl = false

            if (speex && fribidi) {
                if (xvid) {
                    fullGpl = true
                } else {
                    full = true
                }
            } else if (speex) {
                audio = true
            } else if (fribidi) {
                video = true
            } else if (xvid) {
                if (gnutls) {
                    httpsGpl = true
                } else {
                    minGpl = true
                }
            } else {
                if (gnutls) {
                    https = true
                }
            }

            if (fullGpl) {
                return if (externalLibraryList.contains("dav1d") &&
                    externalLibraryList.contains("fontconfig") &&
                    externalLibraryList.contains("freetype") &&
                    externalLibraryList.contains("fribidi") &&
                    externalLibraryList.contains("gmp") &&
                    externalLibraryList.contains("gnutls") &&
                    externalLibraryList.contains("kvazaar") &&
                    externalLibraryList.contains("mp3lame") &&
                    externalLibraryList.contains("libass") &&
                    externalLibraryList.contains("iconv") &&
                    externalLibraryList.contains("libilbc") &&
                    externalLibraryList.contains("libtheora") &&
                    externalLibraryList.contains("libvidstab") &&
                    externalLibraryList.contains("libvorbis") &&
                    externalLibraryList.contains("libvpx") &&
                    externalLibraryList.contains("libwebp") &&
                    externalLibraryList.contains("libxml2") &&
                    externalLibraryList.contains("opencore-amr") &&
                    externalLibraryList.contains("opus") &&
                    externalLibraryList.contains("shine") &&
                    externalLibraryList.contains("snappy") &&
                    externalLibraryList.contains("soxr") &&
                    externalLibraryList.contains("speex") &&
                    externalLibraryList.contains("twolame") &&
                    externalLibraryList.contains("x264") &&
                    externalLibraryList.contains("x265") &&
                    externalLibraryList.contains("xvid") &&
                    externalLibraryList.contains("zimg")
                ) {
                    "full-gpl"
                } else {
                    "custom"
                }
            }

            if (full) {
                return if (externalLibraryList.contains("dav1d") &&
                    externalLibraryList.contains("fontconfig") &&
                    externalLibraryList.contains("freetype") &&
                    externalLibraryList.contains("fribidi") &&
                    externalLibraryList.contains("gmp") &&
                    externalLibraryList.contains("gnutls") &&
                    externalLibraryList.contains("kvazaar") &&
                    externalLibraryList.contains("mp3lame") &&
                    externalLibraryList.contains("libass") &&
                    externalLibraryList.contains("iconv") &&
                    externalLibraryList.contains("libilbc") &&
                    externalLibraryList.contains("libtheora") &&
                    externalLibraryList.contains("libvorbis") &&
                    externalLibraryList.contains("libvpx") &&
                    externalLibraryList.contains("libwebp") &&
                    externalLibraryList.contains("libxml2") &&
                    externalLibraryList.contains("opencore-amr") &&
                    externalLibraryList.contains("opus") &&
                    externalLibraryList.contains("shine") &&
                    externalLibraryList.contains("snappy") &&
                    externalLibraryList.contains("soxr") &&
                    externalLibraryList.contains("speex") &&
                    externalLibraryList.contains("twolame") &&
                    externalLibraryList.contains("zimg")
                ) {
                    "full"
                } else {
                    "custom"
                }
            }

            if (video) {
                return if (externalLibraryList.contains("dav1d") &&
                    externalLibraryList.contains("fontconfig") &&
                    externalLibraryList.contains("freetype") &&
                    externalLibraryList.contains("fribidi") &&
                    externalLibraryList.contains("kvazaar") &&
                    externalLibraryList.contains("libass") &&
                    externalLibraryList.contains("iconv") &&
                    externalLibraryList.contains("libtheora") &&
                    externalLibraryList.contains("libvpx") &&
                    externalLibraryList.contains("libwebp") &&
                    externalLibraryList.contains("snappy") &&
                    externalLibraryList.contains("zimg")
                ) {
                    "video"
                } else {
                    "custom"
                }
            }

            if (audio) {
                return if (externalLibraryList.contains("mp3lame") &&
                    externalLibraryList.contains("libilbc") &&
                    externalLibraryList.contains("libvorbis") &&
                    externalLibraryList.contains("opencore-amr") &&
                    externalLibraryList.contains("opus") &&
                    externalLibraryList.contains("shine") &&
                    externalLibraryList.contains("soxr") &&
                    externalLibraryList.contains("speex") &&
                    externalLibraryList.contains("twolame")
                ) {
                    "audio"
                } else {
                    "custom"
                }
            }

            if (httpsGpl) {
                return if (externalLibraryList.contains("gmp") &&
                    externalLibraryList.contains("gnutls") &&
                    externalLibraryList.contains("libvidstab") &&
                    externalLibraryList.contains("x264") &&
                    externalLibraryList.contains("x265") &&
                    externalLibraryList.contains("xvid")
                ) {
                    "https-gpl"
                } else {
                    "custom"
                }
            }

            if (https) {
                return if (externalLibraryList.contains("gmp") &&
                    externalLibraryList.contains("gnutls")
                ) {
                    "https"
                } else {
                    "custom"
                }
            }

            if (minGpl) {
                return if (externalLibraryList.contains("libvidstab") &&
                    externalLibraryList.contains("x264") &&
                    externalLibraryList.contains("x265") &&
                    externalLibraryList.contains("xvid")
                ) {
                    "min-gpl"
                } else {
                    "custom"
                }
            }

            return if (externalLibraryList.size == 0) {
                "min"
            } else {
                "custom"
            }
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
