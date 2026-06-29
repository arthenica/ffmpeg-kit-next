/*
 * Copyright (c) 2018-2021 Taner Sener
 * Copyright (c) 2024 ARTHENICA LTD
 *
 * This file is part of FFmpegKit.
 *
 * FFmpegKit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKit.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit

import org.junit.Assert
import org.junit.Test

/**
 * Tests for [FFmpegKitConfig] class.
 */
class FFmpegKitConfigTest {

    @Test
    fun getExternalLibraries() {
        val supportedExternalLibraries = listOf(
            "chromaprint", "dav1d", "fontconfig", "freetype", "fribidi", "gmp", "gnutls", "kvazaar",
            "lame", "libaom", "libass", "libjxl", "liblc3", "libsvtav1", "libiconv", "libilbc", "libtheora", "libvidstab", "libvorbis",
            "libvpx", "libwebp", "libxml2", "opencore-amr", "opus", "shine", "sdl", "snappy", "soxr",
            "speex", "tesseract", "twolame", "vvenc", "x264", "x265", "xvidcore", "android-zlib",
            "android-media-codec"
        )

        val enabledList = mutableListOf<String>()
        for (supportedExternalLibrary in supportedExternalLibraries) {
            if (externalLibrariesCommandOutput.contains("enable-$supportedExternalLibrary") ||
                externalLibrariesCommandOutput.contains("enable-lib$supportedExternalLibrary")
            ) {
                enabledList.add(supportedExternalLibrary)
            }
        }

        enabledList.sort()

        Assert.assertNotNull(enabledList)
        Assert.assertEquals(1, enabledList.size)
    }

    @Test
    fun getPackageName() {
        Assert.assertEquals("min", listToPackageName(listOf("")))
        Assert.assertEquals("min-gpl", listToPackageName(listOf("xvidcore")))
        Assert.assertEquals("full-gpl", listToPackageName(listOf("gnutls", "speex", "fribidi", "xvidcore")))
        Assert.assertEquals("full", listToPackageName(listOf("fribidi", "speex")))
        Assert.assertEquals("video", listToPackageName(listOf("fribidi")))
        Assert.assertEquals("audio", listToPackageName(listOf("speex")))
        Assert.assertEquals("https", listToPackageName(listOf("gnutls")))
        Assert.assertEquals("https-gpl", listToPackageName(listOf("gnutls", "xvidcore")))
    }

    @Test
    fun extractExtensionFromSafDisplayName() {
        var extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("video.mp4 (2)")
        Assert.assertEquals("mp4", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("video file name.mp3 (2)")
        Assert.assertEquals("mp3", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("file.mp4")
        Assert.assertEquals("mp4", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("file name.mp4")
        Assert.assertEquals("mp4", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("archive.tar.gz (3)")
        Assert.assertEquals("gz", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName(".nomedia")
        Assert.assertEquals("nomedia", extension)

        extension = FFmpegKitConfig.extractExtensionFromSafDisplayName("")
        Assert.assertEquals("raw", extension)
    }

    @Test
    fun setSessionHistorySize() {
        val originalSize = FFmpegKitConfig.getSessionHistorySize()

        try {
            FFmpegKitConfig.clearSessions()
            var newSize = 15
            FFmpegKitConfig.setSessionHistorySize(newSize)

            for (i in 1..(newSize + 5)) {
                FFmpegSession.create(FFmpegSessionTest.TEST_ARGUMENTS)
                Assert.assertTrue(FFmpegKitConfig.getSessions().size <= newSize)
            }

            FFmpegKitConfig.clearSessions()
            newSize = 3
            FFmpegKitConfig.setSessionHistorySize(newSize)
            for (i in 1..(newSize + 5)) {
                FFmpegSession.create(FFmpegSessionTest.TEST_ARGUMENTS)
                Assert.assertTrue(FFmpegKitConfig.getSessions().size <= newSize)
            }
        } finally {
            FFmpegKitConfig.clearSessions()
            FFmpegKitConfig.setSessionHistorySize(originalSize)
        }
    }

    @Test
    fun setSessionHistorySizeRejectsHardLimitAndIgnoresNonPositiveValues() {
        val originalSize = FFmpegKitConfig.getSessionHistorySize()

        try {
            FFmpegKitConfig.setSessionHistorySize(0)
            Assert.assertEquals(originalSize, FFmpegKitConfig.getSessionHistorySize())

            try {
                FFmpegKitConfig.setSessionHistorySize(1000)
                Assert.fail("Expected IllegalArgumentException")
            } catch (e: IllegalArgumentException) {
                Assert.assertEquals("Session history size must not exceed the hard limit!", e.message)
            }
        } finally {
            FFmpegKitConfig.setSessionHistorySize(originalSize)
        }
    }

    @Test
    fun sessionHistoryQueriesAndDeletion() {
        val originalSize = FFmpegKitConfig.getSessionHistorySize()

        try {
            FFmpegKitConfig.clearSessions()
            FFmpegKitConfig.setSessionHistorySize(10)

            val ffmpegSession = FFmpegSession.create(FFmpegSessionTest.TEST_ARGUMENTS)
            val ffprobeSession = FFprobeSession.create(arrayOf("-show_format", "sample.mp4"))
            val mediaInformationSession = MediaInformationSession.create(arrayOf("-print_format", "json", "sample.mp4"))

            ffmpegSession.complete(ReturnCode(0))
            ffprobeSession.fail(Exception("probe failed"))

            Assert.assertSame(mediaInformationSession, FFmpegKitConfig.getLastSession())
            Assert.assertSame(ffmpegSession, FFmpegKitConfig.getLastCompletedSession())
            Assert.assertSame(ffmpegSession, FFmpegKitConfig.getSession(ffmpegSession.getSessionId()))
            Assert.assertEquals(
                listOf(ffmpegSession.getSessionId(), ffprobeSession.getSessionId(), mediaInformationSession.getSessionId()),
                FFmpegKitConfig.getSessions().map { it.getSessionId() }
            )

            Assert.assertEquals(listOf(ffmpegSession.getSessionId()), FFmpegKitConfig.getFFmpegSessions().map { it.getSessionId() })
            Assert.assertEquals(listOf(ffprobeSession.getSessionId()), FFmpegKitConfig.getFFprobeSessions().map { it.getSessionId() })
            Assert.assertEquals(listOf(mediaInformationSession.getSessionId()), FFmpegKitConfig.getMediaInformationSessions().map { it.getSessionId() })
            Assert.assertEquals(listOf(ffmpegSession.getSessionId()), FFmpegKitConfig.getSessionsByState(SessionState.COMPLETED).map { it.getSessionId() })
            Assert.assertEquals(listOf(ffprobeSession.getSessionId()), FFmpegKitConfig.getSessionsByState(SessionState.FAILED).map { it.getSessionId() })
            Assert.assertEquals(listOf(mediaInformationSession.getSessionId()), FFmpegKitConfig.getSessionsByState(SessionState.CREATED).map { it.getSessionId() })

            FFmpegKitConfig.deleteSession(ffprobeSession.getSessionId())

            Assert.assertNull(FFmpegKitConfig.getSession(ffprobeSession.getSessionId()))
            Assert.assertEquals(2, FFmpegKitConfig.getSessions().size)

            FFmpegKitConfig.clearSessions()

            Assert.assertTrue(FFmpegKitConfig.getSessions().isEmpty())
            Assert.assertNull(FFmpegKitConfig.getLastSession())
            Assert.assertNull(FFmpegKitConfig.getLastCompletedSession())
        } finally {
            FFmpegKitConfig.clearSessions()
            FFmpegKitConfig.setSessionHistorySize(originalSize)
        }
    }

    @Test
    fun globalCompleteCallbacksCanBeSetAndCleared() {
        val ffmpegCallback = FFmpegSessionCompleteCallback { }
        val ffprobeCallback = FFprobeSessionCompleteCallback { }
        val mediaInformationCallback = MediaInformationSessionCompleteCallback { }

        try {
            FFmpegKitConfig.enableFFmpegSessionCompleteCallback(ffmpegCallback)
            FFmpegKitConfig.enableFFprobeSessionCompleteCallback(ffprobeCallback)
            FFmpegKitConfig.enableMediaInformationSessionCompleteCallback(mediaInformationCallback)

            Assert.assertSame(ffmpegCallback, FFmpegKitConfig.getFFmpegSessionCompleteCallback())
            Assert.assertSame(ffprobeCallback, FFmpegKitConfig.getFFprobeSessionCompleteCallback())
            Assert.assertSame(mediaInformationCallback, FFmpegKitConfig.getMediaInformationSessionCompleteCallback())
        } finally {
            FFmpegKitConfig.enableFFmpegSessionCompleteCallback(null)
            FFmpegKitConfig.enableFFprobeSessionCompleteCallback(null)
            FFmpegKitConfig.enableMediaInformationSessionCompleteCallback(null)
        }

        Assert.assertNull(FFmpegKitConfig.getFFmpegSessionCompleteCallback())
        Assert.assertNull(FFmpegKitConfig.getFFprobeSessionCompleteCallback())
        Assert.assertNull(FFmpegKitConfig.getMediaInformationSessionCompleteCallback())
    }

    @Test
    fun safUrlsReusableCanBeToggled() {
        FFmpegKitConfig.setSafUrlsReusable(true)
        Assert.assertTrue(FFmpegKitConfig.getSafUrlsReusable())

        FFmpegKitConfig.setSafUrlsReusable(false)
        Assert.assertFalse(FFmpegKitConfig.getSafUrlsReusable())
    }

    @Test
    fun sessionStateToStringUsesEnumName() {
        Assert.assertEquals("CREATED", FFmpegKitConfig.sessionStateToString(SessionState.CREATED))
        Assert.assertEquals("RUNNING", FFmpegKitConfig.sessionStateToString(SessionState.RUNNING))
        Assert.assertEquals("COMPLETED", FFmpegKitConfig.sessionStateToString(SessionState.COMPLETED))
        Assert.assertEquals("FAILED", FFmpegKitConfig.sessionStateToString(SessionState.FAILED))
    }

    @Test
    fun unregisterSafProtocolUrl() {
        FFmpegKitConfig.unregisterSafProtocolUrl("ffkitsaf:1.mp4")
    }

    private fun listToPackageName(externalLibraryList: List<String>): String {
        val speex = externalLibraryList.contains("speex")
        val fribidi = externalLibraryList.contains("fribidi")
        val gnutls = externalLibraryList.contains("gnutls")
        val xvidcore = externalLibraryList.contains("xvidcore")

        return if (speex && fribidi) {
            if (xvidcore) "full-gpl" else "full"
        } else if (speex) {
            "audio"
        } else if (fribidi) {
            "video"
        } else if (xvidcore) {
            if (gnutls) "https-gpl" else "min-gpl"
        } else {
            if (gnutls) "https" else "min"
        }
    }

    companion object {
        private val externalLibrariesCommandOutput = "   configuration:\n" +
                "                          --cross-prefix=i686-linux-android-\n" +
                "                          --sysroot=/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/sysroot\n" +
                "                          --prefix=/Users/taner/Projects/ffmpeg-kit/prebuilt/android-x86/ffmpeg\n" +
                "                          --pkg-config=/usr/local/bin/pkg-config --extra-cflags='-march=i686 -mtune=intel -mssse3 -mfpmath=sse -m32 -Wno-unused-function -fstrict-aliasing -fPIC -DANDROID -D__ANDROID__ -D__ANDROID_API__=21 -O2 -I/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/sysroot/usr/include -I/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/sysroot/usr/local/include'\n" +
                "                          --extra-cxxflags='-std=c++11 -fno-exceptions -fno-rtti'\n" +
                "                          --extra-ldflags='-march=i686 -Wl,--gc-sections,--icf=safe -lc -lm -ldl -llog -lc++_shared -L/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/i686-linux-android/lib -L/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/sysroot/usr/lib -L/Users/taner/Library/Android/sdk/ndk-bundle/toolchains/ffmpeg-kit-i686/lib -L/Users/taner/Library/Android/sdk/ndk-bundle/platforms/android-21/arch-x86/usr/lib'\n" +
                "                          --enable-version3\n" +
                "                          --arch=i686\n" +
                "                          --cpu=i686\n" +
                "                          --target-os=android\n" +
                "                          --disable-neon\n" +
                "                          --disable-asm\n" +
                "                          --disable-inline-asm\n" +
                "                          --enable-cross-compile\n" +
                "                          --enable-pic\n" +
                "                          --enable-jni\n" +
                "                          --enable-libvorbis\n" +
                "                          --enable-optimizations\n" +
                "                          --enable-swscale\n" +
                "                          --enable-shared\n" +
                "                          --enable-v4l2-m2m\n" +
                "                          --enable-small\n" +
                "                          --disable-openssl\n" +
                "                          --disable-xmm-clobber-test\n" +
                "                          --disable-debug\n" +
                "                          --disable-neon-clobber-test\n" +
                "                          --disable-programs\n" +
                "                          --disable-postproc\n" +
                "                          --disable-doc\n" +
                "                          --disable-htmlpages\n" +
                "                          --disable-manpages\n" +
                "                          --disable-podpages\n" +
                "                          --disable-txtpages\n" +
                "                          --disable-static\n" +
                "                          --disable-sndio\n" +
                "                          --disable-schannel\n" +
                "                          --disable-securetransport\n" +
                "                          --disable-xlib\n" +
                "                          --disable-cuda\n" +
                "                          --disable-cuvid\n" +
                "                          --disable-nvenc\n" +
                "                          --disable-vaapi\n" +
                "                          --disable-vdpau\n" +
                "                          --disable-videotoolbox\n" +
                "                          --disable-audiotoolbox\n" +
                "                          --disable-appkit\n" +
                "                          --disable-alsa\n" +
                "                          --disable-cuda\n" +
                "                          --disable-cuvid\n" +
                "                          --disable-nvenc\n" +
                "                          --disable-vaapi\n" +
                "                          --disable-vdpau\n" +
                "                          --disable-zlib\n"
    }
}
