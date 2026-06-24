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

class FFmpegSessionTest {

    @Test
    fun constructorTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        // 1. getCompleteCallback
        Assert.assertNull(ffmpegSession.getCompleteCallback())

        // 2. getLogCallback
        Assert.assertNull(ffmpegSession.getLogCallback())

        // 3. getStatisticsCallback
        Assert.assertNull(ffmpegSession.getStatisticsCallback())

        // 4. getSessionId
        Assert.assertTrue(ffmpegSession.getSessionId() > 0)

        // 5. getCreateTime
        Assert.assertTrue(ffmpegSession.getCreateTime().time <= System.currentTimeMillis())

        // 6. getStartTime
        Assert.assertNull(ffmpegSession.getStartTime())

        // 7. getEndTime
        Assert.assertNull(ffmpegSession.getEndTime())

        // 8. getDuration
        Assert.assertEquals(0L, ffmpegSession.getDuration())

        // 9. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffmpegSession.getArguments())

        // 10. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffmpegSession.getCommand())

        // 11. getLogs
        Assert.assertEquals(0, ffmpegSession.getLogs().size)

        // 12. getLogsAsString
        Assert.assertEquals("", ffmpegSession.getLogsAsString())

        // 13. getState
        Assert.assertEquals(SessionState.CREATED, ffmpegSession.getState())

        // 14. getReturnCode
        Assert.assertNull(ffmpegSession.getReturnCode())

        // 15. getFailStackTrace
        Assert.assertNull(ffmpegSession.getFailStackTrace())

        // 16. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffmpegSession.getLogRedirectionStrategy())

        // 17. getFuture
        Assert.assertNull(ffmpegSession.getFuture())
    }

    @Test
    fun constructorTest2() {
        val completeCallback = FFmpegSessionCompleteCallback { }

        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS, completeCallback)

        // 1. getCompleteCallback
        Assert.assertEquals(ffmpegSession.getCompleteCallback(), completeCallback)

        // 2. getLogCallback
        Assert.assertNull(ffmpegSession.getLogCallback())

        // 3. getStatisticsCallback
        Assert.assertNull(ffmpegSession.getStatisticsCallback())

        // 4. getSessionId
        Assert.assertTrue(ffmpegSession.getSessionId() > 0)

        // 5. getCreateTime
        Assert.assertTrue(ffmpegSession.getCreateTime().time <= System.currentTimeMillis())

        // 6. getStartTime
        Assert.assertNull(ffmpegSession.getStartTime())

        // 7. getEndTime
        Assert.assertNull(ffmpegSession.getEndTime())

        // 8. getDuration
        Assert.assertEquals(0L, ffmpegSession.getDuration())

        // 9. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffmpegSession.getArguments())

        // 10. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffmpegSession.getCommand())

        // 11. getLogs
        Assert.assertEquals(0, ffmpegSession.getLogs().size)

        // 12. getLogsAsString
        Assert.assertEquals("", ffmpegSession.getLogsAsString())

        // 13. getState
        Assert.assertEquals(SessionState.CREATED, ffmpegSession.getState())

        // 14. getReturnCode
        Assert.assertNull(ffmpegSession.getReturnCode())

        // 15. getFailStackTrace
        Assert.assertNull(ffmpegSession.getFailStackTrace())

        // 16. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffmpegSession.getLogRedirectionStrategy())

        // 17. getFuture
        Assert.assertNull(ffmpegSession.getFuture())
    }

    @Test
    fun constructorTest3() {
        val completeCallback = FFmpegSessionCompleteCallback { }
        val logCallback = LogCallback { }
        val statisticsCallback = StatisticsCallback { }

        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS, completeCallback, logCallback, statisticsCallback)

        // 1. getCompleteCallback
        Assert.assertEquals(ffmpegSession.getCompleteCallback(), completeCallback)

        // 2. getLogCallback
        Assert.assertEquals(ffmpegSession.getLogCallback(), logCallback)

        // 3. getStatisticsCallback
        Assert.assertEquals(ffmpegSession.getStatisticsCallback(), statisticsCallback)

        // 4. getSessionId
        Assert.assertTrue(ffmpegSession.getSessionId() > 0)

        // 5. getCreateTime
        Assert.assertTrue(ffmpegSession.getCreateTime().time <= System.currentTimeMillis())

        // 6. getStartTime
        Assert.assertNull(ffmpegSession.getStartTime())

        // 7. getEndTime
        Assert.assertNull(ffmpegSession.getEndTime())

        // 8. getDuration
        Assert.assertEquals(0L, ffmpegSession.getDuration())

        // 9. getArguments
        Assert.assertArrayEquals(TEST_ARGUMENTS, ffmpegSession.getArguments())

        // 10. getCommand
        Assert.assertEquals(TEST_ARGUMENTS.joinToString(" "), ffmpegSession.getCommand())

        // 11. getLogs
        Assert.assertEquals(0, ffmpegSession.getLogs().size)

        // 12. getLogsAsString
        Assert.assertEquals("", ffmpegSession.getLogsAsString())

        // 13. getState
        Assert.assertEquals(SessionState.CREATED, ffmpegSession.getState())

        // 14. getReturnCode
        Assert.assertNull(ffmpegSession.getReturnCode())

        // 15. getFailStackTrace
        Assert.assertNull(ffmpegSession.getFailStackTrace())

        // 16. getLogRedirectionStrategy
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffmpegSession.getLogRedirectionStrategy())

        // 17. getFuture
        Assert.assertNull(ffmpegSession.getFuture())
    }

    @Test
    fun getSessionIdTest() {
        val ffmpegSession1 = FFmpegSession.create(TEST_ARGUMENTS)
        val ffmpegSession2 = FFmpegSession.create(TEST_ARGUMENTS)
        val ffmpegSession3 = FFmpegSession.create(TEST_ARGUMENTS)

        Assert.assertTrue(ffmpegSession3.getSessionId() > ffmpegSession2.getSessionId())
        Assert.assertTrue(ffmpegSession3.getSessionId() > ffmpegSession1.getSessionId())
        Assert.assertTrue(ffmpegSession2.getSessionId() > ffmpegSession1.getSessionId())

        Assert.assertTrue(ffmpegSession1.getSessionId() > 0)
        Assert.assertTrue(ffmpegSession2.getSessionId() > 0)
        Assert.assertTrue(ffmpegSession3.getSessionId() > 0)
    }

    @Test
    fun getLogs() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        val logMessage1 = "i am log one"
        val logMessage2 = "i am log two"
        val logMessage3 = "i am log three"

        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_INFO, logMessage1))
        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage2))
        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_TRACE, logMessage3))

        val logs = ffmpegSession.getLogs()

        Assert.assertEquals(3, logs.size)
    }

    @Test
    fun getLogsReturnsSnapshot() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)
        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_INFO, "first"))

        val logs = ffmpegSession.getLogs()
        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_INFO, "second"))

        Assert.assertEquals(1, logs.size)
        Assert.assertEquals(2, ffmpegSession.getLogs().size)
    }

    @Test
    fun getLogsAsStringTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        val logMessage1 = "i am log one"
        val logMessage2 = "i am log two"

        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage1))
        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_DEBUG, logMessage2))

        val logsAsString = ffmpegSession.getLogsAsString()

        Assert.assertEquals(logMessage1 + logMessage2, logsAsString)
    }

    @Test
    fun statisticsTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)
        val statistics1 = Statistics(ffmpegSession.getSessionId(), 1, 24.5f, 28.2f, 1024L, 10.5, 900.0, 1.25)
        val statistics2 = Statistics(ffmpegSession.getSessionId(), 2, 25.0f, 27.8f, 2048L, 20.0, 950.5, 1.5)

        Assert.assertNull(ffmpegSession.getLastReceivedStatistics())

        ffmpegSession.addStatistics(statistics1)
        ffmpegSession.addStatistics(statistics2)

        Assert.assertEquals(2, ffmpegSession.getStatistics().size)
        Assert.assertSame(statistics2, ffmpegSession.getLastReceivedStatistics())
        Assert.assertEquals(
            "Statistics{sessionId=${ffmpegSession.getSessionId()}, videoFrameNumber=2, videoFps=25.0, videoQuality=27.8, size=2048, time=20.0, bitrate=950.5, speed=1.5}",
            statistics2.toString()
        )
    }

    @Test
    fun typeFlagsTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        Assert.assertTrue(ffmpegSession.isFFmpeg())
        Assert.assertFalse(ffmpegSession.isFFprobe())
        Assert.assertFalse(ffmpegSession.isMediaInformation())
    }

    @Test
    fun explicitLogRedirectionStrategyTest() {
        val ffmpegSession = FFmpegSession.create(
            TEST_ARGUMENTS,
            null,
            null,
            null,
            LogRedirectionStrategy.ALWAYS_PRINT_LOGS
        )

        Assert.assertEquals(LogRedirectionStrategy.ALWAYS_PRINT_LOGS, ffmpegSession.getLogRedirectionStrategy())
    }

    @Test
    fun toStringIncludesSessionDetails() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        ffmpegSession.addLog(Log(ffmpegSession.getSessionId(), Level.AV_LOG_INFO, "hello"))
        ffmpegSession.complete(ReturnCode(0))

        val text = ffmpegSession.toString()
        Assert.assertTrue(text.contains("FFmpegSession{"))
        Assert.assertTrue(text.contains("sessionId=${ffmpegSession.getSessionId()}"))
        Assert.assertTrue(text.contains("arguments=argument1 argument2"))
        Assert.assertTrue(text.contains("logs=hello"))
        Assert.assertTrue(text.contains("state=COMPLETED"))
        Assert.assertTrue(text.contains("returnCode=0"))
    }

    @Test
    fun getLogRedirectionStrategy() {
        FFmpegKitConfig.setLogRedirectionStrategy(LogRedirectionStrategy.NEVER_PRINT_LOGS)

        val ffmpegSession1 = FFmpegSession.create(TEST_ARGUMENTS)
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffmpegSession1.getLogRedirectionStrategy())

        FFmpegKitConfig.setLogRedirectionStrategy(LogRedirectionStrategy.PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED)

        val ffmpegSession2 = FFmpegSession.create(TEST_ARGUMENTS)
        Assert.assertEquals(FFmpegKitConfig.getLogRedirectionStrategy(), ffmpegSession2.getLogRedirectionStrategy())
    }

    @Test
    fun startRunningTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        ffmpegSession.startRunning()

        Assert.assertEquals(SessionState.RUNNING, ffmpegSession.getState())
        Assert.assertTrue(ffmpegSession.getStartTime()!!.time <= System.currentTimeMillis())
        Assert.assertTrue(ffmpegSession.getCreateTime().time <= ffmpegSession.getStartTime()!!.time)
    }

    @Test
    fun completeTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        ffmpegSession.startRunning()
        ffmpegSession.complete(ReturnCode(100))

        Assert.assertEquals(SessionState.COMPLETED, ffmpegSession.getState())
        Assert.assertEquals(100, ffmpegSession.getReturnCode()!!.value)
        Assert.assertTrue(ffmpegSession.getStartTime()!!.time <= ffmpegSession.getEndTime()!!.time)
        Assert.assertTrue(ffmpegSession.getDuration() >= 0)
    }

    @Test
    fun failTest() {
        val ffmpegSession = FFmpegSession.create(TEST_ARGUMENTS)

        ffmpegSession.startRunning()
        ffmpegSession.fail(Exception(""))

        Assert.assertEquals(SessionState.FAILED, ffmpegSession.getState())
        Assert.assertNull(ffmpegSession.getReturnCode())
        Assert.assertTrue(ffmpegSession.getStartTime()!!.time <= ffmpegSession.getEndTime()!!.time)
        Assert.assertTrue(ffmpegSession.getDuration() >= 0)
        Assert.assertNotNull(ffmpegSession.getFailStackTrace())
    }

    companion object {
        @JvmField
        val TEST_ARGUMENTS = arrayOf("argument1", "argument2")
    }
}
