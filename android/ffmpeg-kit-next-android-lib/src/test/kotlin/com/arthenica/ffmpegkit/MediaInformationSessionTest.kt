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

import org.json.JSONObject
import org.junit.Assert
import org.junit.Test

class MediaInformationSessionTest {

    @Test
    fun constructorTest() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)

        Assert.assertNull(mediaInformationSession.getCompleteCallback())
        Assert.assertNull(mediaInformationSession.getLogCallback())
        Assert.assertNull(mediaInformationSession.getMediaInformation())
        Assert.assertTrue(mediaInformationSession.getSessionId() > 0)
        Assert.assertTrue(mediaInformationSession.getCreateTime().time <= System.currentTimeMillis())
        Assert.assertNull(mediaInformationSession.getStartTime())
        Assert.assertNull(mediaInformationSession.getEndTime())
        Assert.assertEquals(0L, mediaInformationSession.getDuration())
        Assert.assertArrayEquals(TEST_ARGUMENTS, mediaInformationSession.getArguments())
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), mediaInformationSession.getCommand())
        Assert.assertEquals(0, mediaInformationSession.getLogs().size)
        Assert.assertEquals("", mediaInformationSession.getLogsAsString())
        Assert.assertEquals(SessionState.CREATED, mediaInformationSession.getState())
        Assert.assertNull(mediaInformationSession.getReturnCode())
        Assert.assertNull(mediaInformationSession.getFailStackTrace())
        Assert.assertEquals(LogRedirectionStrategy.NEVER_PRINT_LOGS, mediaInformationSession.getLogRedirectionStrategy())
        Assert.assertNull(mediaInformationSession.getFuture())
    }

    @Test
    fun constructorWithCallbacksTest() {
        val completeCallback = MediaInformationSessionCompleteCallback { }
        val logCallback = LogCallback { }

        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS, completeCallback, logCallback)

        Assert.assertSame(completeCallback, mediaInformationSession.getCompleteCallback())
        Assert.assertSame(logCallback, mediaInformationSession.getLogCallback())
    }

    @Test
    fun mediaInformationCanBeUpdated() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)
        val mediaInformation = MediaInformation(
            JSONObject("""{"format":{"filename":"sample.mp4","format_name":"mov,mp4"}}"""),
            emptyList(),
            emptyList()
        )

        mediaInformationSession.setMediaInformation(mediaInformation)

        Assert.assertSame(mediaInformation, mediaInformationSession.getMediaInformation())
        Assert.assertEquals("sample.mp4", mediaInformationSession.getMediaInformation()!!.getFilename())
        Assert.assertEquals("mov,mp4", mediaInformationSession.getMediaInformation()!!.getFormat())

        mediaInformationSession.setMediaInformation(null)
        Assert.assertNull(mediaInformationSession.getMediaInformation())
    }

    @Test
    fun typeFlagsTest() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)

        Assert.assertFalse(mediaInformationSession.isFFmpeg())
        Assert.assertFalse(mediaInformationSession.isFFprobe())
        Assert.assertTrue(mediaInformationSession.isMediaInformation())
    }

    @Test
    fun getLogsReturnsSnapshot() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)
        mediaInformationSession.addLog(Log(mediaInformationSession.getSessionId(), Level.AV_LOG_STDERR, "first"))

        val logs = mediaInformationSession.getLogs()
        mediaInformationSession.addLog(Log(mediaInformationSession.getSessionId(), Level.AV_LOG_STDERR, "second"))

        Assert.assertEquals(1, logs.size)
        Assert.assertEquals(2, mediaInformationSession.getLogs().size)
    }

    @Test
    fun completeTest() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)

        mediaInformationSession.startRunning()
        mediaInformationSession.complete(ReturnCode(100))

        Assert.assertEquals(SessionState.COMPLETED, mediaInformationSession.getState())
        Assert.assertEquals(100, mediaInformationSession.getReturnCode()!!.value)
        Assert.assertTrue(mediaInformationSession.getStartTime()!!.time <= mediaInformationSession.getEndTime()!!.time)
        Assert.assertTrue(mediaInformationSession.getDuration() >= 0)
    }

    @Test
    fun failTest() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)

        mediaInformationSession.startRunning()
        mediaInformationSession.fail(Exception("failure"))

        Assert.assertEquals(SessionState.FAILED, mediaInformationSession.getState())
        Assert.assertNull(mediaInformationSession.getReturnCode())
        Assert.assertTrue(mediaInformationSession.getStartTime()!!.time <= mediaInformationSession.getEndTime()!!.time)
        Assert.assertTrue(mediaInformationSession.getDuration() >= 0)
        Assert.assertNotNull(mediaInformationSession.getFailStackTrace())
        Assert.assertTrue(mediaInformationSession.getFailStackTrace()!!.contains("failure"))
    }

    @Test
    fun toStringIncludesSessionDetails() {
        val mediaInformationSession = MediaInformationSession.create(TEST_ARGUMENTS)

        mediaInformationSession.addLog(Log(mediaInformationSession.getSessionId(), Level.AV_LOG_STDERR, "json"))
        mediaInformationSession.complete(ReturnCode(0))

        val text = mediaInformationSession.toString()
        Assert.assertTrue(text.contains("MediaInformationSession{"))
        Assert.assertTrue(text.contains("sessionId=${mediaInformationSession.getSessionId()}"))
        Assert.assertTrue(text.contains("arguments=-v quiet -print_format json -show_format sample.mp4"))
        Assert.assertTrue(text.contains("logs=json"))
        Assert.assertTrue(text.contains("state=COMPLETED"))
        Assert.assertTrue(text.contains("returnCode=0"))
    }

    companion object {
        private val TEST_ARGUMENTS = arrayOf("-v", "quiet", "-print_format", "json", "-show_format", "sample.mp4")
    }
}
