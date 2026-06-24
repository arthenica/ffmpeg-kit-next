/*
 * Copyright (c) 2021 Taner Sener
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
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with FFmpegKit.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit

import org.junit.Assert
import org.junit.Test

class FFprobeSessionTest {

    @Test
    fun constructorTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        // 1. getCompleteCallback
        Assert.assertNull(ffprobeSession.getCompleteCallback())

        // 2. getLogCallback
        Assert.assertNull(ffprobeSession.getLogCallback())

        // 3. getSessionId
        Assert.assertTrue(ffprobeSession.getSessionId() > 0)

        // 4. getCreateTime
        Assert.assertTrue(ffprobeSession.getCreateTime().time <= System.currentTimeMillis())

        // 5. getStartTime
        Assert.assertNull(ffprobeSession.getStartTime())

        // 6. getEndTime
        Assert.assertNull(ffprobeSession.getEndTime())

        // 7. getDuration
        Assert.assertEquals(0L, ffprobeSession.getDuration())

        // 8. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffprobeSession.getArguments())

        // 9. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffprobeSession.getCommand())

        // 10. getLogs
        Assert.assertEquals(0, ffprobeSession.getLogs().size)

        // 11. getLogsAsString
        Assert.assertEquals("", ffprobeSession.getLogsAsString())

        // 12. getState
        Assert.assertEquals(SessionState.CREATED, ffprobeSession.getState())

        // 13. getReturnCode
        Assert.assertNull(ffprobeSession.getReturnCode())

        // 14. getFailStackTrace
        Assert.assertNull(ffprobeSession.getFailStackTrace())

        // 15. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffprobeSession.getLogRedirectionStrategy())

        // 16. getFuture
        Assert.assertNull(ffprobeSession.getFuture())
    }

    @Test
    fun constructorTest2() {
        val completeCallback = FFprobeSessionCompleteCallback { }

        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS, completeCallback)

        // 1. getCompleteCallback
        Assert.assertEquals(ffprobeSession.getCompleteCallback(), completeCallback)

        // 2. getLogCallback
        Assert.assertNull(ffprobeSession.getLogCallback())

        // 3. getSessionId
        Assert.assertTrue(ffprobeSession.getSessionId() > 0)

        // 4. getCreateTime
        Assert.assertTrue(ffprobeSession.getCreateTime().time <= System.currentTimeMillis())

        // 5. getStartTime
        Assert.assertNull(ffprobeSession.getStartTime())

        // 6. getEndTime
        Assert.assertNull(ffprobeSession.getEndTime())

        // 7. getDuration
        Assert.assertEquals(0L, ffprobeSession.getDuration())

        // 8. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffprobeSession.getArguments())

        // 9. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffprobeSession.getCommand())

        // 10. getLogs
        Assert.assertEquals(0, ffprobeSession.getLogs().size)

        // 11. getLogsAsString
        Assert.assertEquals("", ffprobeSession.getLogsAsString())

        // 12. getState
        Assert.assertEquals(SessionState.CREATED, ffprobeSession.getState())

        // 13. getReturnCode
        Assert.assertNull(ffprobeSession.getReturnCode())

        // 14. getFailStackTrace
        Assert.assertNull(ffprobeSession.getFailStackTrace())

        // 15. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffprobeSession.getLogRedirectionStrategy())

        // 16. getFuture
        Assert.assertNull(ffprobeSession.getFuture())
    }

    @Test
    fun constructorTest3() {
        val completeCallback = FFprobeSessionCompleteCallback { }
        val logCallback = LogCallback { }

        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS, completeCallback, logCallback)

        // 1. getCompleteCallback
        Assert.assertEquals(ffprobeSession.getCompleteCallback(), completeCallback)

        // 2. getLogCallback
        Assert.assertEquals(ffprobeSession.getLogCallback(), logCallback)

        // 3. getSessionId
        Assert.assertTrue(ffprobeSession.getSessionId() > 0)

        // 4. getCreateTime
        Assert.assertTrue(ffprobeSession.getCreateTime().time <= System.currentTimeMillis())

        // 5. getStartTime
        Assert.assertNull(ffprobeSession.getStartTime())

        // 6. getEndTime
        Assert.assertNull(ffprobeSession.getEndTime())

        // 7. getDuration
        Assert.assertEquals(0L, ffprobeSession.getDuration())

        // 8. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffprobeSession.getArguments())

        // 9. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffprobeSession.getCommand())

        // 10. getLogs
        Assert.assertEquals(0, ffprobeSession.getLogs().size)

        // 11. getLogsAsString
        Assert.assertEquals("", ffprobeSession.getLogsAsString())

        // 12. getState
        Assert.assertEquals(SessionState.CREATED, ffprobeSession.getState())

        // 13. getReturnCode
        Assert.assertNull(ffprobeSession.getReturnCode())

        // 14. getFailStackTrace
        Assert.assertNull(ffprobeSession.getFailStackTrace())

        // 15. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffprobeSession.getLogRedirectionStrategy())

        // 16. getFuture
        Assert.assertNull(ffprobeSession.getFuture())
    }

    @Test
    fun getSessionIdTest() {
        val ffprobeSession1 = FFprobeSession.create(TEST_ARGUMENTS)
        val ffprobeSession2 = FFprobeSession.create(TEST_ARGUMENTS)
        val ffprobeSession3 = FFprobeSession.create(TEST_ARGUMENTS)

        Assert.assertTrue(ffprobeSession3.getSessionId() > ffprobeSession2.getSessionId())
        Assert.assertTrue(ffprobeSession3.getSessionId() > ffprobeSession1.getSessionId())
        Assert.assertTrue(ffprobeSession2.getSessionId() > ffprobeSession1.getSessionId())

        Assert.assertTrue(ffprobeSession1.getSessionId() > 0)
        Assert.assertTrue(ffprobeSession2.getSessionId() > 0)
        Assert.assertTrue(ffprobeSession3.getSessionId() > 0)
    }

    @Test
    fun getLogs() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        val logMessage1 = "i am log one"
        val logMessage2 = "i am log two"
        val logMessage3 = "i am log three"

        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_INFO, logMessage1))
        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage2))
        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_TRACE, logMessage3))

        val logs = ffprobeSession.getLogs()

        Assert.assertEquals(3, logs.size)
    }

    @Test
    fun getLogsReturnsSnapshot() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)
        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_INFO, "first"))

        val logs = ffprobeSession.getLogs()
        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_INFO, "second"))

        Assert.assertEquals(1, logs.size)
        Assert.assertEquals(2, ffprobeSession.getLogs().size)
    }

    @Test
    fun getLogsAsStringTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        val logMessage1 = "i am log one"
        val logMessage2 = "i am log two"

        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage1))
        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage2))

        val logsAsString = ffprobeSession.getLogsAsString()

        Assert.assertEquals(logMessage1 + logMessage2, logsAsString)
    }

    @Test
    fun typeFlagsTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        Assert.assertFalse(ffprobeSession.isFFmpeg())
        Assert.assertTrue(ffprobeSession.isFFprobe())
        Assert.assertFalse(ffprobeSession.isMediaInformation())
    }

    @Test
    fun explicitLogRedirectionStrategyTest() {
        val ffprobeSession = FFprobeSession.create(
            TEST_ARGUMENTS,
            null,
            null,
            LogRedirectionStrategy.ALWAYS_PRINT_LOGS
        )

        Assert.assertEquals(LogRedirectionStrategy.ALWAYS_PRINT_LOGS, ffprobeSession.getLogRedirectionStrategy())
    }

    @Test
    fun toStringIncludesSessionDetails() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        ffprobeSession.addLog(Log(ffprobeSession.getSessionId(), Level.AV_LOG_INFO, "hello"))
        ffprobeSession.complete(ReturnCode(0))

        val text = ffprobeSession.toString()
        Assert.assertTrue(text.contains("FFprobeSession{"))
        Assert.assertTrue(text.contains("sessionId=${ffprobeSession.getSessionId()}"))
        Assert.assertTrue(text.contains("arguments=argument1 argument2"))
        Assert.assertTrue(text.contains("logs=hello"))
        Assert.assertTrue(text.contains("state=COMPLETED"))
        Assert.assertTrue(text.contains("returnCode=0"))
    }

    @Test
    fun getLogRedirectionStrategy() {
        FFmpegKitConfig.setLogRedirectionStrategy(LogRedirectionStrategy.NEVER_PRINT_LOGS)

        val ffprobeSession1 = FFprobeSession.create(TEST_ARGUMENTS)
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffprobeSession1.getLogRedirectionStrategy())

        FFmpegKitConfig.setLogRedirectionStrategy(LogRedirectionStrategy.PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED)

        val ffprobeSession2 = FFprobeSession.create(TEST_ARGUMENTS)
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffprobeSession2.getLogRedirectionStrategy())
    }

    @Test
    fun startRunningTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        ffprobeSession.startRunning()

        Assert.assertEquals(SessionState.RUNNING, ffprobeSession.getState())
        Assert.assertTrue(ffprobeSession.getStartTime()!!.time <= System.currentTimeMillis())
        Assert.assertTrue(ffprobeSession.getCreateTime().time <= ffprobeSession.getStartTime()!!.time)
    }

    @Test
    fun completeTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        ffprobeSession.startRunning()
        ffprobeSession.complete(ReturnCode(100))

        Assert.assertEquals(SessionState.COMPLETED, ffprobeSession.getState())
        Assert.assertEquals(100, ffprobeSession.getReturnCode()!!.value)
        Assert.assertTrue(ffprobeSession.getStartTime()!!.time <= ffprobeSession.getEndTime()!!.time)
        Assert.assertTrue(ffprobeSession.getDuration() >= 0)
    }

    @Test
    fun failTest() {
        val ffprobeSession = FFprobeSession.create(TEST_ARGUMENTS)

        ffprobeSession.startRunning()
        ffprobeSession.fail(Exception(""))

        Assert.assertEquals(SessionState.FAILED, ffprobeSession.getState())
        Assert.assertNull(ffprobeSession.getReturnCode())
        Assert.assertTrue(ffprobeSession.getStartTime()!!.time <= ffprobeSession.getEndTime()!!.time)
        Assert.assertTrue(ffprobeSession.getDuration() >= 0)
        Assert.assertNotNull(ffprobeSession.getFailStackTrace())
    }

    companion object {
        private val TEST_ARGUMENTS = arrayOf("argument1", "argument2")
    }
}
