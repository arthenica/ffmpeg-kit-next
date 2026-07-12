/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
 * Copyright (c) 2024 ARTHENICA LTD
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

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.util.SparseArray
import androidx.annotation.NonNull
import androidx.annotation.Nullable
import com.arthenica.smartexception.java.Exceptions
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.lang.ref.WeakReference
import java.net.URI
import java.net.URISyntaxException
import java.nio.ByteBuffer
import java.text.MessageFormat
import java.util.ArrayList
import java.util.Arrays
import java.util.Collections
import java.util.LinkedHashMap
import java.util.LinkedList
import java.util.StringTokenizer
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference

/**
 * <p>Configuration class of <code>FFmpegKit</code> library.
 */
open class FFmpegKitConfig private constructor() {

    internal class SAFProtocolUrl(
        val safId: Int,
        val uri: Uri,
        val openMode: String,
        val contentResolver: ContentResolver,
        val reusable: Boolean
    ) {
        var parcelFileDescriptor: ParcelFileDescriptor? = null
    }

    companion object {

        /**
         * The tag used for logging.
         */
        internal const val TAG = "ffmpeg-kit-next"

        /**
         * Prefix of named pipes created by ffmpeg kit.
         */
        internal const val FFMPEG_KIT_NAMED_PIPE_PREFIX = "fk_pipe_"

        internal const val FFMPEG_KIT_STREAM_TYPE_INPUT = 1
        internal const val FFMPEG_KIT_STREAM_TYPE_OUTPUT = 2

        /**
         * Generates ids for named ffmpeg kit pipes and saf protocol urls.
         */
        private val uniqueIdGenerator: AtomicInteger

        private var activeLogLevel: Level

        /* Session history variables */
        private var sessionHistorySize: Int
        private val sessionHistoryMap: MutableMap<Long, Session>
        private val sessionHistoryList: ArrayList<Session>
        private val sessionHistoryLock: Any
        private val sessionDeleteListeners: CopyOnWriteArrayList<WeakReference<SessionDeleteListener>>

        private var asyncConcurrencyLimit: Int
        private var asyncExecutorService: ExecutorService
        private val asyncExecutorServiceLock: Any

        /* Global callbacks */
        private var globalLogCallback: LogCallback?
        private var globalStatisticsCallback: StatisticsCallback?
        private var globalFFmpegSessionCompleteCallback: FFmpegSessionCompleteCallback?
        private var globalFFprobeSessionCompleteCallback: FFprobeSessionCompleteCallback?
        private var globalMediaInformationSessionCompleteCallback: MediaInformationSessionCompleteCallback?
        private val safIdMap: SparseArray<SAFProtocolUrl>
        private val safFileDescriptorMap: SparseArray<SAFProtocolUrl>
        private var globalLogRedirectionStrategy: LogRedirectionStrategy
        private val safUrlsReusable: AtomicBoolean

        init {

            Exceptions.registerRootPackage("com.arthenica")

            val nativeFFmpegTriedAndFailed = NativeLoader.loadFFmpeg()

            /* ALL FFMPEG-KIT LIBRARIES LOADED AT STARTUP */
            Abi::class.java.name
            FFmpegKit::class.java.name
            FFprobeKit::class.java.name

            NativeLoader.loadFFmpegKit(nativeFFmpegTriedAndFailed)

            uniqueIdGenerator = AtomicInteger(1)

            /* NATIVE LOG LEVEL IS RECEIVED ONLY ON STARTUP */
            activeLogLevel = Level.from(NativeLoader.loadLogLevel())

            asyncConcurrencyLimit = 10
            asyncExecutorService = Executors.newFixedThreadPool(asyncConcurrencyLimit)
            asyncExecutorServiceLock = Any()

            sessionHistorySize = 10
            sessionHistoryMap = LinkedHashMap()
            sessionHistoryList = ArrayList()
            sessionHistoryLock = Any()
            sessionDeleteListeners = CopyOnWriteArrayList()

            globalLogCallback = null
            globalStatisticsCallback = null
            globalFFmpegSessionCompleteCallback = null
            globalFFprobeSessionCompleteCallback = null
            globalMediaInformationSessionCompleteCallback = null

            safIdMap = SparseArray()
            safFileDescriptorMap = SparseArray()
            globalLogRedirectionStrategy =
                LogRedirectionStrategy.PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED
            safUrlsReusable = AtomicBoolean(false)

            android.util.Log.i(
                TAG,
                String.format(
                    "Loaded ffmpeg-kit-next-%s-%s-api%s-%s.",
                    NativeLoader.loadAbi(),
                    NativeLoader.loadVersion(),
                    NativeLoader.loadMinSdk(),
                    NativeLoader.loadBuildDate()
                )
            )
        }

        /**
         * <p>Enables log and statistics redirection.
         *
         * <p>When redirection is enabled FFmpeg/FFprobe logs are redirected to Logcat and sessions
         * collect log and statistics entries for the executions. It is possible to define global or
         * session specific log/statistics callbacks as well.
         *
         * <p>Note that redirection is enabled by default. If you do not want to use its functionality
         * please use {@link #disableRedirection()} to disable it.
         */
        @JvmStatic
        fun enableRedirection() {
            enableNativeRedirection()
        }

        /**
         * <p>Disables log and statistics redirection.
         *
         * <p>When redirection is disabled logs are printed to stderr, all logs and statistics
         * callbacks are disabled and <code>FFprobe</code>'s <code>getMediaInformation</code> methods
         * do not work.
         */
        @JvmStatic
        fun disableRedirection() {
            disableNativeRedirection()
        }

        /**
         * <p>Log redirection method called by the native library.
         *
         * @param sessionId  id of the session that generated this log, 0 for logs that do not belong
         *                   to a specific session
         * @param levelValue log level as defined in {@link Level}
         * @param logMessage redirected log message data
         */
        @JvmStatic
        private fun log(sessionId: Long, levelValue: Int, logMessage: ByteArray) {
            val level = Level.from(levelValue)
            val text = String(logMessage, Charsets.UTF_8)
            val log = Log(sessionId, level, text)
            var globalCallbackDefined = false
            var sessionCallbackDefined = false
            var activeLogRedirectionStrategy = globalLogRedirectionStrategy

            // AV_LOG_STDERR logs are always redirected
            if ((activeLogLevel == Level.AV_LOG_QUIET && levelValue != Level.AV_LOG_STDERR.value) || levelValue > activeLogLevel.value) {
                // LOG NEITHER PRINTED NOR FORWARDED
                return
            }

            val session = getSession(sessionId)
            if (session != null) {
                activeLogRedirectionStrategy = session.getLogRedirectionStrategy()
                session.addLog(log)

                val sessionLogCallback = session.getLogCallback()
                if (sessionLogCallback != null) {
                    sessionCallbackDefined = true

                    try {
                        // NOTIFY SESSION CALLBACK DEFINED
                        sessionLogCallback.apply(log)
                    } catch (e: Exception) {
                        android.util.Log.e(
                            TAG,
                            String.format(
                                "Exception thrown inside session log callback.%s",
                                Exceptions.getStackTraceString(e)
                            )
                        )
                    }
                }
            }

            val globalLogCallbackFunction = globalLogCallback
            if (globalLogCallbackFunction != null) {
                globalCallbackDefined = true

                try {
                    // NOTIFY GLOBAL CALLBACK DEFINED
                    globalLogCallbackFunction.apply(log)
                } catch (e: Exception) {
                    android.util.Log.e(
                        TAG,
                        String.format(
                            "Exception thrown inside global log callback.%s",
                            Exceptions.getStackTraceString(e)
                        )
                    )
                }
            }

            // EXECUTE THE LOG STRATEGY
            when (activeLogRedirectionStrategy) {
                LogRedirectionStrategy.NEVER_PRINT_LOGS -> {
                    return
                }

                LogRedirectionStrategy.PRINT_LOGS_WHEN_GLOBAL_CALLBACK_NOT_DEFINED -> {
                    if (globalCallbackDefined) {
                        return
                    }
                }

                LogRedirectionStrategy.PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED -> {
                    if (sessionCallbackDefined) {
                        return
                    }
                }

                LogRedirectionStrategy.PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED -> {
                    if (globalCallbackDefined || sessionCallbackDefined) {
                        return
                    }
                }

                LogRedirectionStrategy.ALWAYS_PRINT_LOGS -> {
                }
            }

            // PRINT LOGS
            when (level) {
                Level.AV_LOG_QUIET -> {
                    // PRINT NO OUTPUT
                }

                Level.AV_LOG_TRACE, Level.AV_LOG_DEBUG -> {
                    android.util.Log.d(TAG, text)
                }

                Level.AV_LOG_INFO -> {
                    android.util.Log.i(TAG, text)
                }

                Level.AV_LOG_WARNING -> {
                    android.util.Log.w(TAG, text)
                }

                Level.AV_LOG_ERROR, Level.AV_LOG_FATAL, Level.AV_LOG_PANIC -> {
                    android.util.Log.e(TAG, text)
                }

                Level.AV_LOG_STDERR, Level.AV_LOG_VERBOSE -> {
                    android.util.Log.v(TAG, text)
                }

                else -> {
                    android.util.Log.v(TAG, text)
                }
            }
        }

        /**
         * <p>Statistics redirection method called by the native library.
         *
         * @param sessionId        id of the session that generated this statistics, 0 by default
         * @param videoFrameNumber frame number for videos
         * @param videoFps         frames per second value for videos
         * @param videoQuality     quality of the video stream
         * @param size             size in bytes
         * @param time             processed duration in milliseconds
         * @param bitrate          output bit rate in kbits/s
         * @param speed            processing speed = processed duration / operation duration
         */
        @JvmStatic
        private fun statistics(
            sessionId: Long, videoFrameNumber: Int,
            videoFps: Float, videoQuality: Float, size: Long,
            time: Double, bitrate: Double, speed: Double
        ) {
            val statistics = Statistics(
                sessionId,
                videoFrameNumber,
                videoFps,
                videoQuality,
                size,
                time,
                bitrate,
                speed
            )

            val session = getSession(sessionId)
            if (session != null && session.isFFmpeg()) {
                val ffmpegSession = session as FFmpegSession
                ffmpegSession.addStatistics(statistics)

                val sessionStatisticsCallback = ffmpegSession.getStatisticsCallback()
                if (sessionStatisticsCallback != null) {
                    try {
                        // NOTIFY SESSION CALLBACK IF DEFINED
                        sessionStatisticsCallback.apply(statistics)
                    } catch (e: Exception) {
                        android.util.Log.e(
                            TAG,
                            String.format(
                                "Exception thrown inside session statistics callback.%s",
                                Exceptions.getStackTraceString(e)
                            )
                        )
                    }
                }
            }

            val globalStatisticsCallbackFunction = globalStatisticsCallback
            if (globalStatisticsCallbackFunction != null) {
                try {
                    // NOTIFY GLOBAL CALLBACK IF DEFINED
                    globalStatisticsCallbackFunction.apply(statistics)
                } catch (e: Exception) {
                    android.util.Log.e(
                        TAG,
                        String.format(
                            "Exception thrown inside global statistics callback.%s",
                            Exceptions.getStackTraceString(e)
                        )
                    )
                }
            }
        }

        /**
         * <p>Sets and overrides <code>fontconfig</code> configuration directory.
         *
         * @param path directory that contains fontconfig configuration (fonts.conf)
         * @return zero on success, non-zero on error
         */
        @JvmStatic
        fun setFontconfigConfigurationPath(@NonNull path: String): Int {
            return setNativeEnvironmentVariable("FONTCONFIG_PATH", path)
        }

        /**
         * <p>Registers the fonts inside the given path, so they become available to use in FFmpeg
         * filters.
         *
         * <p>Note that you need to build <code>FFmpegKit</code> with <code>fontconfig</code>
         * enabled or use a prebuilt package with <code>fontconfig</code> inside to be able to use
         * fonts in <code>FFmpeg</code>.
         *
         * @param context           application context to access application data
         * @param fontDirectoryPath directory that contains fonts (.ttf and .otf files)
         * @param fontNameMapping   custom font name mappings, useful to access your fonts with more
         *                          friendly names
         */
        @JvmStatic
        fun setFontDirectory(
            @NonNull context: Context,
            @NonNull fontDirectoryPath: String,
            @Nullable fontNameMapping: @JvmSuppressWildcards Map<String?, String?>?
        ) {
            setFontDirectoryList(
                context,
                Collections.singletonList(fontDirectoryPath),
                fontNameMapping
            )
        }

        /**
         * <p>Registers the fonts inside the given list of font directories, so they become available
         * to use in FFmpeg filters.
         *
         * <p>Note that you need to build <code>FFmpegKit</code> with <code>fontconfig</code>
         * enabled or use a prebuilt package with <code>fontconfig</code> inside to be able to use
         * fonts in <code>FFmpeg</code>.
         *
         * @param context           application context to access application data
         * @param fontDirectoryList list of directories that contain fonts (.ttf and .otf files)
         * @param fontNameMapping   custom font name mappings, useful to access your fonts with more
         *                          friendly names
         */
        @JvmStatic
        fun setFontDirectoryList(
            @NonNull context: Context,
            @NonNull fontDirectoryList: @JvmSuppressWildcards List<String>,
            @Nullable fontNameMapping: @JvmSuppressWildcards Map<String?, String?>?
        ) {
            val cacheDir = context.cacheDir
            var validFontNameMappingCount = 0

            val tempConfigurationDirectory = File(cacheDir, "fontconfig")
            if (!tempConfigurationDirectory.exists()) {
                val tempFontConfDirectoryCreated = tempConfigurationDirectory.mkdirs()
                android.util.Log.d(
                    TAG,
                    String.format(
                        "Created temporary font conf directory: %s.",
                        tempFontConfDirectoryCreated
                    )
                )
            }

            val fontConfiguration = File(tempConfigurationDirectory, "fonts.conf")
            if (fontConfiguration.exists()) {
                val fontConfigurationDeleted = fontConfiguration.delete()
                android.util.Log.d(
                    TAG,
                    String.format(
                        "Deleted old temporary font configuration: %s.",
                        fontConfigurationDeleted
                    )
                )
            }

            /* PROCESS MAPPINGS FIRST */
            val fontNameMappingBlock = buildString {
            if (fontNameMapping != null && fontNameMapping.isNotEmpty()) {
                fontNameMapping.entries
                for (mapping in fontNameMapping.entries) {
                    val fontName = mapping.key
                    val mappedFontName = mapping.value

                    if (!fontName.isNullOrBlank() && !mappedFontName.isNullOrBlank()) {
                        append("    <match target=\"pattern\">\n")
                        append("        <test qual=\"any\" name=\"family\">\n")
                        append(
                            String.format(
                                "            <string>%s</string>\n",
                                fontName
                            )
                        )
                        append("        </test>\n")
                        append("        <edit name=\"family\" mode=\"assign\" binding=\"same\">\n")
                        append(
                            String.format(
                                "            <string>%s</string>\n",
                                mappedFontName
                            )
                        )
                        append("        </edit>\n")
                        append("    </match>\n")

                        validFontNameMappingCount++
                    }
                }
            }
                }

            val fontConfigBuilder = buildString {
                append("<?xml version=\"1.0\"?>\n")
                append("<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n")
                append("<fontconfig>\n")
                append("    <dir prefix=\"cwd\">.</dir>\n")
                for (fontDirectoryPath in fontDirectoryList) {
                    append("    <dir>")
                    append(fontDirectoryPath)
                    append("</dir>\n")
                }
                append(fontNameMappingBlock)
                append("</fontconfig>\n")
            }

            val reference = AtomicReference<FileOutputStream>()
            try {
                val outputStream = FileOutputStream(fontConfiguration)
                reference.set(outputStream)

                outputStream.write(fontConfigBuilder.toString().toByteArray())
                outputStream.flush()

                android.util.Log.d(
                    TAG,
                    String.format(
                        "Saved new temporary font configuration with %d font name mappings.",
                        validFontNameMappingCount
                    )
                )

                setFontconfigConfigurationPath(tempConfigurationDirectory.absolutePath)

                for (fontDirectoryPath in fontDirectoryList) {
                    android.util.Log.d(
                        TAG,
                        String.format(
                            "Font directory %s registered successfully.",
                            fontDirectoryPath
                        )
                    )
                }

            } catch (e: IOException) {
                android.util.Log.e(
                    TAG,
                    String.format(
                        "Failed to set font directory: %s.%s",
                        fontDirectoryList.toTypedArray().contentToString(),
                        Exceptions.getStackTraceString(e)
                    )
                )
            } finally {
                if (reference.get() != null) {
                    try {
                        reference.get().close()
                    } catch (_: IOException) {
                        // DO NOT PRINT THIS ERROR
                    }
                }
            }
        }

        /**
         * <p>Creates a new named pipe to use in <code>FFmpeg</code> operations.
         *
         * <p>Please note that creator is responsible of closing created pipes via the
         * {@link #closeFFmpegPipe} method.
         *
         * @param context application context
         * @return the full path of the named pipe
         */
        @JvmStatic
        @Nullable
        fun registerNewFFmpegPipe(@NonNull context: Context): String? {

            // PIPES ARE CREATED UNDER THE PIPES DIRECTORY
            val cacheDir = context.cacheDir
            val pipesDir = File(cacheDir, "pipes")

            if (!pipesDir.exists()) {
                val pipesDirCreated = pipesDir.mkdirs()
                if (!pipesDirCreated) {
                    android.util.Log.e(
                        TAG,
                        String.format(
                            "Failed to create pipes directory: %s.",
                            pipesDir.absolutePath
                        )
                    )
                    return null
                }
            }

            val newFFmpegPipePath = MessageFormat.format(
                "{0}{1}{2}{3}",
                pipesDir,
                File.separator,
                FFMPEG_KIT_NAMED_PIPE_PREFIX,
                uniqueIdGenerator.getAndIncrement()
            )

            // FIRST CLOSE OLD PIPES WITH THE SAME NAME
            closeFFmpegPipe(newFFmpegPipePath)

            val rc = registerNewNativeFFmpegPipe(newFFmpegPipePath)
            return if (rc == 0) {
                newFFmpegPipePath
            } else {
                android.util.Log.e(
                    TAG,
                    String.format(
                        "Failed to register new FFmpeg pipe %s. Operation failed with rc=%d.",
                        newFFmpegPipePath,
                        rc
                    )
                )
                null
            }
        }

        /**
         * <p>Closes a previously created <code>FFmpeg</code> pipe.
         *
         * @param ffmpegPipePath full path of the FFmpeg pipe
         */
        @JvmStatic
        fun closeFFmpegPipe(@NonNull ffmpegPipePath: String) {
            val file = File(ffmpegPipePath)
            if (file.exists()) {
                file.delete()
            }
        }

        @JvmStatic
        internal fun registerFFmpegKitInputBuffer(data: ByteArray): Long {
            return registerNativeFFmpegKitInputBuffer(data)
        }

        @JvmStatic
        internal fun registerFFmpegKitInputDirectBuffer(byteBuffer: ByteBuffer, size: Int): Long {
            return registerNativeFFmpegKitInputDirectBuffer(byteBuffer, size)
        }

        @JvmStatic
        internal fun registerFFmpegKitOutputBuffer(initialCapacity: Long, maxCapacity: Long): Long {
            return registerNativeFFmpegKitOutputBuffer(initialCapacity, maxCapacity)
        }

        @JvmStatic
        internal fun getFFmpegKitBufferSize(id: Long): Long {
            return getNativeFFmpegKitBufferSize(id)
        }

        @JvmStatic
        internal fun getFFmpegKitOutputBuffer(id: Long): ByteArray? {
            return getNativeFFmpegKitOutputBuffer(id)
        }

        @JvmStatic
        internal fun getFFmpegKitOutputBufferDirect(id: Long): ByteBuffer? {
            return getNativeFFmpegKitOutputBufferDirect(id)
        }

        @JvmStatic
        internal fun unregisterFFmpegKitBuffer(id: Long) {
            unregisterNativeFFmpegKitBuffer(id)
        }

        @JvmStatic
        internal fun registerFFmpegKitStream(capacity: Long, type: Int): Long {
            return registerNativeFFmpegKitStream(capacity, type)
        }

        @JvmStatic
        internal fun writeFFmpegKitStream(
            id: Long,
            data: ByteArray,
            offset: Int,
            length: Int,
            timeoutMs: Int
        ): Int {
            return nativeFFmpegKitStreamWrite(id, data, offset, length, timeoutMs)
        }

        @JvmStatic
        internal fun readFFmpegKitStream(id: Long, maxBytes: Int, timeoutMs: Int): ByteArray {
            return nativeFFmpegKitStreamRead(id, maxBytes, timeoutMs)
        }

        @JvmStatic
        internal fun closeFFmpegKitStreamInput(id: Long) {
            nativeFFmpegKitStreamCloseInput(id)
        }

        @JvmStatic
        internal fun unregisterFFmpegKitStream(id: Long) {
            unregisterNativeFFmpegKitStream(id)
        }

        /**
         * Returns the list of camera ids supported. These devices can be used in <code>FFmpeg</code>
         * commands.
         *
         * <p>Note that this method requires API Level &ge; 24. On older API levels it returns an empty
         * list.
         *
         * @param context application context
         * @return list of camera ids supported or an empty list if no supported cameras are found
         */
        @JvmStatic
        @NonNull
        fun getSupportedCameraIds(@NonNull context: Context): List<String> {
            val detectedCameraIdList = ArrayList<String>()

            detectedCameraIdList.addAll(CameraSupport.extractSupportedCameraIds(context))

            return detectedCameraIdList
        }

        /**
         * <p>Returns the version of FFmpeg bundled within <code>FFmpegKit</code> library.
         *
         * @return the version of FFmpeg
         */
        @JvmStatic
        @NonNull
        fun getFFmpegVersion(): String {
            return getNativeFFmpegVersion()
        }

        /**
         * <p>Returns FFmpegKit library version.
         *
         * @return FFmpegKit version
         */
        @JvmStatic
        @NonNull
        fun getVersion(): String {
            return getNativeVersion()
        }

        /**
         * <p>Returns whether FFmpegKit release is a Long Term Release or not.
         *
         * @return true/yes or false/no
         * @deprecated as of version 6.1.2, use the {@link AbiDetect#getNativeMinSdk()} method to
         * determine the features supported by this version
         */
        @JvmStatic
        @Deprecated("as of version 6.1.2, use the AbiDetect#getNativeMinSdk() method to determine the features supported by this version")
        fun isLTSBuild(): Boolean {
            return AbiDetect.isNativeLTSBuild()
        }

        /**
         * <p>Returns FFmpegKit library build date.
         *
         * @return FFmpegKit library build date
         */
        @JvmStatic
        @NonNull
        fun getBuildDate(): String {
            return getNativeBuildDate()
        }

        /**
         * <p>Prints the given string to Logcat using the given priority. If string provided is bigger
         * than the Logcat buffer, the string is printed in multiple lines.
         *
         * @param logPriority one of {@link android.util.Log#VERBOSE},
         *                    {@link android.util.Log#DEBUG},
         *                    {@link android.util.Log#INFO},
         *                    {@link android.util.Log#WARN},
         *                    {@link android.util.Log#ERROR},
         *                    {@link android.util.Log#ASSERT}
         * @param string      string to be printed
         */
        @JvmStatic
        fun printToLogcat(logPriority: Int, @NonNull string: String) {
            val LOGGER_ENTRY_MAX_LEN = 4 * 1000

            var remainingString = string
            do {
                if (remainingString.length <= LOGGER_ENTRY_MAX_LEN) {
                    android.util.Log.println(logPriority, TAG, remainingString)
                    remainingString = ""
                } else {
                    val index = remainingString.substring(0, LOGGER_ENTRY_MAX_LEN).lastIndexOf('\n')
                    if (index < 0) {
                        android.util.Log.println(
                            logPriority,
                            TAG,
                            remainingString.substring(0, LOGGER_ENTRY_MAX_LEN)
                        )
                        remainingString = remainingString.substring(LOGGER_ENTRY_MAX_LEN)
                    } else {
                        android.util.Log.println(
                            logPriority,
                            TAG,
                            remainingString.substring(0, index)
                        )
                        remainingString = remainingString.substring(index)
                    }
                }
            } while (remainingString.length > 0)
        }

        /**
         * <p>Sets an environment variable.
         *
         * @param variableName  environment variable name
         * @param variableValue environment variable value
         * @return zero on success, non-zero on error
         */
        @JvmStatic
        fun setEnvironmentVariable(
            @NonNull variableName: String,
            @NonNull variableValue: String
        ): Int {
            return setNativeEnvironmentVariable(variableName, variableValue)
        }

        /**
         * <p>Registers a new ignored signal. Ignored signals are not handled by <code>FFmpegKit</code>
         * library.
         *
         * @param signal signal to be ignored
         */
        @JvmStatic
        fun ignoreSignal(@NonNull signal: Signal) {
            ignoreNativeSignal(signal.value)
        }

        /**
         * <p>Synchronously executes the FFmpeg session provided.
         *
         * @param ffmpegSession FFmpeg session which includes command options/arguments
         */
        @JvmStatic
        fun ffmpegExecute(@NonNull ffmpegSession: FFmpegSession) {
            ffmpegSession.startRunning()

            try {
                val returnCode =
                    nativeFFmpegExecute(ffmpegSession.getSessionId(), ffmpegSession.getArguments())
                ffmpegSession.complete(ReturnCode(returnCode))
            } catch (e: Exception) {
                ffmpegSession.fail(e)
                android.util.Log.w(
                    TAG,
                    String.format(
                        "FFmpeg execute failed: %s.%s",
                        argumentsToString(ffmpegSession.getArguments()),
                        Exceptions.getStackTraceString(e)
                    )
                )
            }
        }

        /**
         * <p>Synchronously executes the FFprobe session provided.
         *
         * @param ffprobeSession FFprobe session which includes command options/arguments
         */
        @JvmStatic
        fun ffprobeExecute(@NonNull ffprobeSession: FFprobeSession) {
            ffprobeSession.startRunning()

            try {
                val returnCode = nativeFFprobeExecute(
                    ffprobeSession.getSessionId(),
                    ffprobeSession.getArguments()
                )
                ffprobeSession.complete(ReturnCode(returnCode))
            } catch (e: Exception) {
                ffprobeSession.fail(e)
                android.util.Log.w(
                    TAG,
                    String.format(
                        "FFprobe execute failed: %s.%s",
                        argumentsToString(ffprobeSession.getArguments()),
                        Exceptions.getStackTraceString(e)
                    )
                )
            }
        }

        /**
         * <p>Synchronously executes the media information session provided.
         *
         * @param mediaInformationSession media information session which includes command options/arguments
         * @param waitTimeout             max time to wait until media information is transmitted
         */
        @JvmStatic
        fun getMediaInformationExecute(
            @NonNull mediaInformationSession: MediaInformationSession,
            waitTimeout: Int
        ) {
            mediaInformationSession.startRunning()

            try {
                val returnCodeValue = nativeFFprobeExecute(
                    mediaInformationSession.getSessionId(),
                    mediaInformationSession.getArguments()
                )
                val returnCode = ReturnCode(returnCodeValue)
                mediaInformationSession.complete(returnCode)
                if (returnCode.isValueSuccess()) {
                    val allLogs = mediaInformationSession.getAllLogs(waitTimeout)
                    val ffprobeJsonOutput = StringBuilder()
                    var i = 0
                    val allLogsSize = allLogs.size
                    while (i < allLogsSize) {
                        val log = allLogs[i]
                        if (log.level == Level.AV_LOG_STDERR) {
                            ffprobeJsonOutput.append(log.message)
                        }
                        i++
                    }
                    val mediaInformation =
                        MediaInformationJsonParser.fromWithError(ffprobeJsonOutput.toString())
                    mediaInformationSession.setMediaInformation(mediaInformation)
                }
            } catch (e: Exception) {
                mediaInformationSession.fail(e)
                android.util.Log.w(
                    TAG,
                    String.format(
                        "Get media information execute failed: %s.%s",
                        argumentsToString(mediaInformationSession.getArguments()),
                        Exceptions.getStackTraceString(e)
                    )
                )
            }
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an {@link FFmpegSessionCompleteCallback} if you want to be notified about the
         * result.
         *
         * @param ffmpegSession FFmpeg session which includes command options/arguments
         */
        @JvmStatic
        fun asyncFFmpegExecute(@NonNull ffmpegSession: FFmpegSession) {
            val asyncFFmpegExecuteTask = AsyncFFmpegExecuteTask(ffmpegSession)
            synchronized(asyncExecutorServiceLock) {
                val future = asyncExecutorService.submit(asyncFFmpegExecuteTask)
                ffmpegSession.setFuture(future)
            }
        }

        /**
         * <p>Starts an asynchronous FFmpeg execution for the given session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an {@link FFmpegSessionCompleteCallback} if you want to be notified about the
         * result.
         *
         * @param ffmpegSession   FFmpeg session which includes command options/arguments
         * @param executorService executor service that will be used to run this asynchronous operation
         */
        @JvmStatic
        fun asyncFFmpegExecute(
            @NonNull ffmpegSession: FFmpegSession,
            @NonNull executorService: ExecutorService
        ) {
            val asyncFFmpegExecuteTask = AsyncFFmpegExecuteTask(ffmpegSession)
            val future = executorService.submit(asyncFFmpegExecuteTask)
            ffmpegSession.setFuture(future)
        }

        /**
         * <p>Starts an asynchronous FFprobe execution for the given session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an {@link FFprobeSessionCompleteCallback} if you want to be notified about the
         * result.
         *
         * @param ffprobeSession FFprobe session which includes command options/arguments
         */
        @JvmStatic
        fun asyncFFprobeExecute(@NonNull ffprobeSession: FFprobeSession) {
            val asyncFFmpegExecuteTask = AsyncFFprobeExecuteTask(ffprobeSession)
            synchronized(asyncExecutorServiceLock) {
                val future = asyncExecutorService.submit(asyncFFmpegExecuteTask)
                ffprobeSession.setFuture(future)
            }
        }

        /**
         * <p>Starts an asynchronous FFprobe execution for the given session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use an {@link FFprobeSessionCompleteCallback} if you want to be notified about the
         * result.
         *
         * @param ffprobeSession  FFprobe session which includes command options/arguments
         * @param executorService executor service that will be used to run this asynchronous operation
         */
        @JvmStatic
        fun asyncFFprobeExecute(
            @NonNull ffprobeSession: FFprobeSession,
            @NonNull executorService: ExecutorService
        ) {
            val asyncFFmpegExecuteTask = AsyncFFprobeExecuteTask(ffprobeSession)
            val future = executorService.submit(asyncFFmpegExecuteTask)
            ffprobeSession.setFuture(future)
        }

        /**
         * <p>Starts an asynchronous FFprobe execution for the given media information session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a {@link MediaInformationSessionCompleteCallback} if you want to be notified
         * about the result.
         *
         * @param mediaInformationSession media information session which includes command
         *                                options/arguments
         * @param waitTimeout             max time to wait until media information is transmitted
         */
        @JvmStatic
        fun asyncGetMediaInformationExecute(
            @NonNull mediaInformationSession: MediaInformationSession,
            waitTimeout: Int
        ) {
            val asyncGetMediaInformationTask =
                AsyncGetMediaInformationTask(mediaInformationSession, waitTimeout)
            synchronized(asyncExecutorServiceLock) {
                val future = asyncExecutorService.submit(asyncGetMediaInformationTask)
                mediaInformationSession.setFuture(future)
            }
        }

        /**
         * <p>Starts an asynchronous FFprobe execution for the given media information session.
         *
         * <p>Note that this method returns immediately and does not wait the execution to complete.
         * You must use a {@link MediaInformationSessionCompleteCallback} if you want to be notified
         * about the result.
         *
         * @param mediaInformationSession media information session which includes command
         *                                options/arguments
         * @param executorService         executor service that will be used to run this asynchronous
         *                                operation
         * @param waitTimeout             max time to wait until media information is transmitted
         */
        @JvmStatic
        fun asyncGetMediaInformationExecute(
            @NonNull mediaInformationSession: MediaInformationSession,
            @NonNull executorService: ExecutorService,
            waitTimeout: Int
        ) {
            val asyncGetMediaInformationTask =
                AsyncGetMediaInformationTask(mediaInformationSession, waitTimeout)
            val future = executorService.submit(asyncGetMediaInformationTask)
            mediaInformationSession.setFuture(future)
        }

        /**
         * Returns the maximum number of async sessions that will be executed in parallel.
         *
         * @return maximum number of async sessions that will be executed in parallel
         */
        @JvmStatic
        fun getAsyncConcurrencyLimit(): Int {
            return asyncConcurrencyLimit
        }

        /**
         * Sets the maximum number of async sessions that will be executed in parallel. If more
         * sessions are submitted those will be queued.
         *
         * @param asyncConcurrencyLimit new async concurrency limit
         */
        @JvmStatic
        fun setAsyncConcurrencyLimit(asyncConcurrencyLimit: Int) {

            if (asyncConcurrencyLimit > 0) {
                synchronized(asyncExecutorServiceLock) {

                    /* SET THE NEW LIMIT */
                    this.asyncConcurrencyLimit = asyncConcurrencyLimit
                    val oldAsyncExecutorService = this.asyncExecutorService

                    /* CREATE THE NEW ASYNC THREAD POOL */
                    this.asyncExecutorService = Executors.newFixedThreadPool(asyncConcurrencyLimit)

                    /* STOP THE OLD ASYNC THREAD POOL */
                    oldAsyncExecutorService.shutdown()
                }
            }
        }

        /**
         * <p>Sets a global callback to redirect FFmpeg/FFprobe logs.
         *
         * @param logCallback log callback or null to disable a previously defined callback
         */
        @JvmStatic
        fun enableLogCallback(@Nullable logCallback: LogCallback?) {
            globalLogCallback = logCallback
        }

        /**
         * <p>Sets a global callback to redirect FFmpeg statistics.
         *
         * @param statisticsCallback statistics callback or null to disable a previously
         *                           defined callback
         */
        @JvmStatic
        fun enableStatisticsCallback(@Nullable statisticsCallback: StatisticsCallback?) {
            globalStatisticsCallback = statisticsCallback
        }

        /**
         * <p>Sets a global FFmpegSessionCompleteCallback to receive execution results for FFmpeg
         * sessions.
         *
         * @param ffmpegSessionCompleteCallback complete callback or null to disable a
         *                                      previously defined callback
         */
        @JvmStatic
        fun enableFFmpegSessionCompleteCallback(@Nullable ffmpegSessionCompleteCallback: FFmpegSessionCompleteCallback?) {
            globalFFmpegSessionCompleteCallback = ffmpegSessionCompleteCallback
        }

        /**
         * <p>Returns the global FFmpegSessionCompleteCallback set.
         *
         * @return global FFmpegSessionCompleteCallback or null if it is not set
         */
        @JvmStatic
        @Nullable
        fun getFFmpegSessionCompleteCallback(): FFmpegSessionCompleteCallback? {
            return globalFFmpegSessionCompleteCallback
        }

        /**
         * <p>Sets a global FFprobeSessionCompleteCallback to receive execution results for FFprobe
         * sessions.
         *
         * @param ffprobeSessionCompleteCallback complete callback or null to disable a
         *                                       previously defined callback
         */
        @JvmStatic
        fun enableFFprobeSessionCompleteCallback(@Nullable ffprobeSessionCompleteCallback: FFprobeSessionCompleteCallback?) {
            globalFFprobeSessionCompleteCallback = ffprobeSessionCompleteCallback
        }

        /**
         * <p>Returns the global FFprobeSessionCompleteCallback set.
         *
         * @return global FFprobeSessionCompleteCallback or null if it is not set
         */
        @JvmStatic
        @Nullable
        fun getFFprobeSessionCompleteCallback(): FFprobeSessionCompleteCallback? {
            return globalFFprobeSessionCompleteCallback
        }

        /**
         * <p>Sets a global MediaInformationSessionCompleteCallback to receive execution results for
         * MediaInformation sessions.
         *
         * @param mediaInformationSessionCompleteCallback complete callback or null to disable
         *                                                a previously defined callback
         */
        @JvmStatic
        fun enableMediaInformationSessionCompleteCallback(@Nullable mediaInformationSessionCompleteCallback: MediaInformationSessionCompleteCallback?) {
            globalMediaInformationSessionCompleteCallback = mediaInformationSessionCompleteCallback
        }

        /**
         * <p>Returns the global MediaInformationSessionCompleteCallback set.
         *
         * @return global MediaInformationSessionCompleteCallback or null if it is not set
         */
        @JvmStatic
        @Nullable
        fun getMediaInformationSessionCompleteCallback(): MediaInformationSessionCompleteCallback? {
            return globalMediaInformationSessionCompleteCallback
        }

        /**
         * Returns the current log level.
         *
         * @return current log level
         */
        @JvmStatic
        @NonNull
        fun getLogLevel(): Level {
            return activeLogLevel
        }

        /**
         * Sets the log level.
         *
         * @param level new log level
         */
        @JvmStatic
        fun setLogLevel(@NonNull level: Level) {
            activeLogLevel = level
            setNativeLogLevel(level.value)
        }

        @JvmStatic
        @JvmName("extractExtensionFromSafDisplayName")
        internal fun extractExtensionFromSafDisplayName(safDisplayName: String): String {
            var rawExtension = safDisplayName
            if (safDisplayName.lastIndexOf(".") >= 0) {
                rawExtension = safDisplayName.substring(safDisplayName.lastIndexOf("."))
            }
            return try {
                // workaround for https://issuetracker.google.com/issues/162440528: ANDROID_CREATE_DOCUMENT generating file names like "transcode.mp3 (2)"
                StringTokenizer(rawExtension, " .").nextToken()
            } catch (e: Exception) {
                android.util.Log.w(
                    TAG,
                    String.format(
                        "Failed to extract extension from saf display name: %s.%s",
                        safDisplayName,
                        Exceptions.getStackTraceString(e)
                    )
                )
                "raw"
            }
        }

        /**
         * <p>Converts the given Structured Access Framework Uri (<code>"content:…"</code>) into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level 19+. On older API levels it returns an empty url.
         *
         * <p>The generated url inherits the global reuse setting defined via
         * {@link #setSafUrlsReusable}, captured at the time this method is called. Use the
         * {@link #getSafParameter(Context, Uri, String, boolean)} overload to define the reuse
         * behaviour for this url explicitly.
         *
         * @param context  application context
         * @param uri      SAF uri
         * @param openMode file mode to use as defined in {@link ContentProvider#openFile ContentProvider.openFile}
         * @return input/output url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameter(
            @NonNull context: Context,
            @NonNull uri: Uri,
            @NonNull openMode: String
        ): String {
            return getSafParameter(context, uri, openMode, safUrlsReusable.get())
        }

        /**
         * <p>Converts the given Structured Access Framework Uri into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level 19+. On older API levels it returns an empty url.
         *
         * <p>The <code>reusable</code> value is stored per url when the url is created. It defines
         * whether this specific url will be automatically unregistered when the file associated
         * with it is closed. Because it is captured at creation time, changing the global reuse
         * setting via {@link #setSafUrlsReusable} afterwards does not affect urls that were already
         * created. Reusable urls must be unregistered manually via {@link #unregisterSafProtocolUrl}.
         *
         * @param context  application context
         * @param uri      SAF uri
         * @param openMode file mode to use as defined in {@link ContentProvider#openFile ContentProvider.openFile}
         * @param reusable set to true to make this url reusable, false to unregister it automatically on close
         * @return input/output url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameter(
            @NonNull context: Context,
            @NonNull uri: Uri,
            @NonNull openMode: String,
            reusable: Boolean
        ): String {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                android.util.Log.i(
                    TAG,
                    String.format(
                        "getSafParameter is not supported on API Level %d",
                        Build.VERSION.SDK_INT
                    )
                )
                return ""
            }

            var displayName = "unknown"
            try {
                context.contentResolver.query(uri, null, null, null, null).use { cursor ->
                    if (cursor != null && cursor.moveToFirst()) {
                        displayName =
                            cursor.getString(cursor.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME))
                    }
                }
            } catch (t: Throwable) {
                android.util.Log.e(
                    TAG,
                    String.format(
                        "Failed to get %s column for %s.%s",
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                        uri.toString(),
                        Exceptions.getStackTraceString(t)
                    )
                )
                throw t
            }

            val safId = uniqueIdGenerator.getAndIncrement()
            safIdMap.put(safId, SAFProtocolUrl(safId, uri, openMode, context.contentResolver, reusable))

            return "ffkitsaf:" + safId + "." + extractExtensionFromSafDisplayName(displayName)
        }

        /**
         * <p>Converts the given Structured Access Framework Uri (<code>"content:…"</code>) into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level &ge; 19. On older API levels it returns an empty url.
         *
         * @param context application context
         * @param uri     SAF uri
         * @return input url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameterForRead(@NonNull context: Context, @NonNull uri: Uri): String {
            return getSafParameter(context, uri, "r")
        }

        /**
         * <p>Converts the given Structured Access Framework Uri (<code>"content:…"</code>) into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level &ge; 19. On older API levels it returns an empty url.
         *
         * @param context  application context
         * @param uri      SAF uri
         * @param reusable set to true to make this url reusable, false to unregister it automatically on close
         * @return input url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameterForRead(@NonNull context: Context, @NonNull uri: Uri, reusable: Boolean): String {
            return getSafParameter(context, uri, "r", reusable)
        }

        /**
         * <p>Converts the given Structured Access Framework Uri (<code>"content:…"</code>) into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level &ge; 19. On older API levels it returns an empty url.
         *
         * @param context application context
         * @param uri     SAF uri
         * @return output url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameterForWrite(@NonNull context: Context, @NonNull uri: Uri): String {
            return getSafParameter(context, uri, "w")
        }

        /**
         * <p>Converts the given Structured Access Framework Uri (<code>"content:…"</code>) into an
         * SAF protocol url that can be used in FFmpeg and FFprobe commands.
         *
         * <p>Requires API Level &ge; 19. On older API levels it returns an empty url.
         *
         * @param context  application context
         * @param uri      SAF uri
         * @param reusable set to true to make this url reusable, false to unregister it automatically on close
         * @return output url that can be passed to FFmpegKit or FFprobeKit
         */
        @JvmStatic
        @NonNull
        fun getSafParameterForWrite(@NonNull context: Context, @NonNull uri: Uri, reusable: Boolean): String {
            return getSafParameter(context, uri, "w", reusable)
        }

        /**
         * Called from native library to open an SAF protocol url.
         *
         * @param safId SAF id part of an SAF protocol url
         * @return file descriptor created for this SAF id or 0 if an error occurs
         */
        @JvmStatic
        private fun safOpen(safId: Int): Int {
            try {
                val safUrl = safIdMap.get(safId)
                if (safUrl != null) {
                    val parcelFileDescriptor =
                        safUrl.contentResolver.openFileDescriptor(safUrl.uri, safUrl.openMode)
                    if (parcelFileDescriptor == null) {
                        android.util.Log.e(
                            TAG,
                            String.format("Failed to open SAF id %d. Content resolver returned null.", safId)
                        )
                        return 0
                    }
                    safUrl.parcelFileDescriptor = parcelFileDescriptor
                    val fd = parcelFileDescriptor.fd
                    safFileDescriptorMap.put(fd, safUrl)
                    android.util.Log.d(
                        TAG,
                        String.format("Generated fd %d for SAF id %d.", fd, safId)
                    )
                    return fd
                } else {
                    android.util.Log.e(TAG, String.format("SAF id %d not found.", safId))
                }
            } catch (t: Throwable) {
                android.util.Log.e(
                    TAG,
                    String.format(
                        "Failed to open SAF id: %d.%s",
                        safId,
                        Exceptions.getStackTraceString(t)
                    )
                )
            }

            return 0
        }

        /**
         * Called from native library to close a file descriptor created for a SAF protocol url.
         *
         * @param fileDescriptor file descriptor that belongs to a SAF protocol url
         * @return 1 if the given file descriptor is closed successfully, 0 if an error occurs
         */
        @JvmStatic
        private fun safClose(fileDescriptor: Int): Int {
            try {
                val safProtocolUrl = safFileDescriptorMap.get(fileDescriptor)
                if (safProtocolUrl != null) {
                    val safId = safProtocolUrl.safId
                    val parcelFileDescriptor = safProtocolUrl.parcelFileDescriptor
                    if (parcelFileDescriptor != null) {
                        safFileDescriptorMap.delete(fileDescriptor)
                        try {
                            parcelFileDescriptor.close()
                        } finally {
                            if (!safProtocolUrl.reusable) {
                                safIdMap.delete(safId)
                            }
                        }
                        android.util.Log.d(
                            TAG,
                            String.format("Closed fd %d for SAF id %d.", fileDescriptor, safId)
                        )
                        return 1
                    } else {
                        android.util.Log.e(
                            TAG,
                            String.format(
                                "ParcelFileDescriptor for SAF fd %d not found.",
                                fileDescriptor
                            )
                        )
                    }
                } else {
                    android.util.Log.e(TAG, String.format("SAF fd %d not found.", fileDescriptor))
                }
            } catch (t: Throwable) {
                android.util.Log.e(
                    TAG,
                    String.format(
                        "Failed to close SAF fd: %d.%s",
                        fileDescriptor,
                        Exceptions.getStackTraceString(t)
                    )
                )
            }

            return 0
        }

        /**
         * Unregisters saf protocol urls and cleans up the resources associated with them.
         *
         * @param safUrl SAF protocol url e.g. ffkitsaf:1.mp4
         */
        @JvmStatic
        fun unregisterSafProtocolUrl(@Nullable safUrl: String?) {
            if (safUrl != null) {
                try {
                    val uri = URI(safUrl)
                    val path = uri.schemeSpecificPart
                    val index = path.indexOf(".")
                    if (index > -1) {
                        val safIdString = path.substring(0, index)
                        val safId = Integer.parseInt(safIdString)
                        safIdMap.delete(safId)
                        android.util.Log.d(
                            TAG,
                            String.format("Unregistered safUrl %s successfully.", safUrl)
                        )
                    } else {
                        android.util.Log.w(
                            TAG,
                            String.format(
                                "Cannot unregister safUrl %s. Failed to drop extension!",
                                safUrl
                            )
                        )
                    }
                } catch (e: URISyntaxException) {
                    android.util.Log.w(
                        TAG,
                        String.format(
                            "Cannot unregister safUrl %s. Failed to extract saf id!",
                            safUrl
                        )
                    )
                } catch (e: NumberFormatException) {
                    android.util.Log.w(
                        TAG,
                        String.format(
                            "Cannot unregister safUrl %s. Failed to extract saf id!",
                            safUrl
                        )
                    )
                }
            }
        }

        /**
         * Returns the session history size.
         *
         * @return session history size
         */
        @JvmStatic
        fun getSessionHistorySize(): Int {
            return sessionHistorySize
        }

        /**
         * Sets the session history size.
         *
         * @param sessionHistorySize session history size, should be smaller than 1000
         */
        @JvmStatic
        fun setSessionHistorySize(sessionHistorySize: Int) {
            if (sessionHistorySize >= 1000) {

                /*
                 * THERE IS A HARD LIMIT ON THE NATIVE SIDE. HISTORY SIZE MUST BE SMALLER THAN 1000
                 */
                throw IllegalArgumentException("Session history size must not exceed the hard limit!")
            } else if (sessionHistorySize > 0) {
                val deletedSessionIds = synchronized(sessionHistoryLock) {
                    this.sessionHistorySize = sessionHistorySize
                    deleteExpiredSessionsLocked()
                }
                notifySessionsDeleted(deletedSessionIds)
            }
        }

        /**
         * Deletes expired sessions.
         */
        private fun deleteExpiredSessionsLocked(): List<Long> {
            val deletedSessionIds = ArrayList<Long>()
            while (sessionHistoryList.size > sessionHistorySize) {
                try {
                    val expiredSession: Session = sessionHistoryList.removeAt(0)
                    sessionHistoryMap.remove(expiredSession.getSessionId())
                    deletedSessionIds.add(expiredSession.getSessionId())
                } catch (_: IndexOutOfBoundsException) {
                }
            }

            return deletedSessionIds
        }

        /**
         * Adds a session to the session history.
         *
         * @param session new session
         */
        @JvmStatic
        internal fun addSession(session: Session) {
            val deletedSessionIds = synchronized(sessionHistoryLock) {

                /*
                 * ASYNC SESSIONS CALL THIS METHOD TWICE
                 * THIS CHECK PREVENTS ADDING THE SAME SESSION AGAIN
                 */
                val sessionAlreadyAdded = sessionHistoryMap.containsKey(session.getSessionId())
                if (!sessionAlreadyAdded) {
                    sessionHistoryMap[session.getSessionId()] = session
                    sessionHistoryList.add(session)
                    deleteExpiredSessionsLocked()
                } else {
                    emptyList()
                }
            }
            notifySessionsDeleted(deletedSessionIds)
        }

        /**
         * Returns the session specified with <code>sessionId</code> from the session history.
         *
         * @param sessionId session identifier
         * @return session specified with sessionId or null if it is not found in the history
         */
        @JvmStatic
        @Nullable
        fun getSession(sessionId: Long): Session? {
            synchronized(sessionHistoryLock) {
                return sessionHistoryMap[sessionId]
            }
        }

        /**
         * Deletes the session specified with <code>sessionId</code> from the session history.
         *
         * @param sessionId session identifier
         */
        @JvmStatic
        fun deleteSession(sessionId: Long) {
            val deletedSessionId = synchronized(sessionHistoryLock) {
                val removedSession = sessionHistoryMap.remove(sessionId)
                if (removedSession != null) {
                    sessionHistoryList.remove(removedSession)
                }
                removedSession?.getSessionId()
            }

            if (deletedSessionId != null) {
                notifySessionDeleted(deletedSessionId)
            }
        }

        /**
         * Returns the last session created from the session history.
         *
         * @return the last session created or null if session history is empty
         */
        @JvmStatic
        @Nullable
        fun getLastSession(): Session? {
            synchronized(sessionHistoryLock) {
                if (sessionHistoryList.size > 0) {
                    return sessionHistoryList[sessionHistoryList.size - 1]
                }
            }

            return null
        }

        /**
         * Returns the last session completed from the session history.
         *
         * @return the last session completed. If there are no completed sessions in the history this
         * method will return null
         */
        @JvmStatic
        @Nullable
        fun getLastCompletedSession(): Session? {
            synchronized(sessionHistoryLock) {
                for (i in sessionHistoryList.size - 1 downTo 0) {
                    val session = sessionHistoryList[i]
                    if (session.getState() == SessionState.COMPLETED) {
                        return session
                    }
                }
            }

            return null
        }

        /**
         * <p>Returns all sessions in the session history.
         *
         * @return all sessions in the session history
         */
        @JvmStatic
        @NonNull
        fun getSessions(): List<Session> {
            synchronized(sessionHistoryLock) {
                return LinkedList(sessionHistoryList)
            }
        }

        /**
         * <p>Clears all, including ongoing, sessions in the session history.
         * <p>Note that callbacks cannot be triggered for deleted sessions.
         */
        @JvmStatic
        fun clearSessions() {
            val deletedSessionIds = synchronized(sessionHistoryLock) {
                val sessionIds = sessionHistoryList.map { it.getSessionId() }
                sessionHistoryList.clear()
                sessionHistoryMap.clear()
                sessionIds
            }
            notifySessionsDeleted(deletedSessionIds)
        }

        /**
         * Adds a listener that is notified when sessions are deleted from session history.
         */
        @JvmStatic
        fun addSessionDeleteListener(@NonNull listener: SessionDeleteListener) {
            removeSessionDeleteListener(listener)
            sessionDeleteListeners.add(WeakReference(listener))
        }

        /**
         * Removes a session delete listener.
         */
        @JvmStatic
        fun removeSessionDeleteListener(@NonNull listener: SessionDeleteListener) {
            for (listenerReference in sessionDeleteListeners) {
                val existingListener = listenerReference.get()
                if (existingListener == null || existingListener === listener) {
                    sessionDeleteListeners.remove(listenerReference)
                }
            }
        }

        private fun notifySessionsDeleted(sessionIds: List<Long>) {
            for (sessionId in sessionIds) {
                notifySessionDeleted(sessionId)
            }
        }

        private fun notifySessionDeleted(sessionId: Long) {
            for (listenerReference in sessionDeleteListeners) {
                val listener = listenerReference.get()
                if (listener == null) {
                    sessionDeleteListeners.remove(listenerReference)
                    continue
                }

                try {
                    listener.sessionDeleted(sessionId)
                } catch (e: Exception) {
                    android.util.Log.e(
                        TAG,
                        String.format(
                            "Exception thrown inside session delete listener.%s",
                            Exceptions.getStackTraceString(e)
                        )
                    )
                }
            }
        }

        /**
         * <p>Returns all FFmpeg sessions in the session history.
         *
         * @return all FFmpeg sessions in the session history
         */
        @JvmStatic
        @NonNull
        fun getFFmpegSessions(): List<FFmpegSession> {
            val list = LinkedList<FFmpegSession>()

            synchronized(sessionHistoryLock) {
                for (session in sessionHistoryList) {
                    if (session.isFFmpeg()) {
                        list.add(session as FFmpegSession)
                    }
                }
            }

            return list
        }

        /**
         * <p>Returns all FFprobe sessions in the session history.
         *
         * @return all FFprobe sessions in the session history
         */
        @JvmStatic
        @NonNull
        fun getFFprobeSessions(): List<FFprobeSession> {
            val list = LinkedList<FFprobeSession>()

            synchronized(sessionHistoryLock) {
                for (session in sessionHistoryList) {
                    if (session.isFFprobe()) {
                        list.add(session as FFprobeSession)
                    }
                }
            }

            return list
        }

        /**
         * <p>Returns all MediaInformation sessions in the session history.
         *
         * @return all MediaInformation sessions in the session history
         */
        @JvmStatic
        @NonNull
        fun getMediaInformationSessions(): List<MediaInformationSession> {
            val list = LinkedList<MediaInformationSession>()

            synchronized(sessionHistoryLock) {
                for (session in sessionHistoryList) {
                    if (session.isMediaInformation()) {
                        list.add(session as MediaInformationSession)
                    }
                }
            }

            return list
        }

        /**
         * <p>Returns sessions that have the given state.
         *
         * @param state session state
         * @return sessions that have the given state from the session history
         */
        @JvmStatic
        @NonNull
        fun getSessionsByState(@NonNull state: SessionState): List<Session> {
            val list = LinkedList<Session>()

            synchronized(sessionHistoryLock) {
                for (session in sessionHistoryList) {
                    if (session.getState() == state) {
                        list.add(session)
                    }
                }
            }

            return list
        }

        /**
         * Returns the active log redirection strategy.
         *
         * @return log redirection strategy
         */
        @JvmStatic
        @NonNull
        fun getLogRedirectionStrategy(): LogRedirectionStrategy {
            return globalLogRedirectionStrategy
        }

        /**
         * <p>Sets the log redirection strategy
         *
         * @param logRedirectionStrategy log redirection strategy
         */
        @JvmStatic
        fun setLogRedirectionStrategy(@NonNull logRedirectionStrategy: LogRedirectionStrategy) {
            globalLogRedirectionStrategy = logRedirectionStrategy
        }

        /**
         * Returns if SAF protocol urls are reusable or not.
         *
         * @return true if SAF protocol urls are reusable, false otherwise
         */
        @JvmStatic
        fun getSafUrlsReusable(): Boolean {
            return safUrlsReusable.get()
        }

        /**
         * Defines the default reuse behaviour applied to SAF protocol urls that are created
         * without an explicit <code>reusable</code> value.
         *
         * <p>Note that SAF protocol urls are not reusable by default and are automatically
         * unregistered when the file associated with them is closed.
         *
         * <p>This setting is captured per url at the time the url is created. Changing it does not
         * affect urls that were already created. To define the reuse behaviour of an individual url
         * regardless of this setting, use the <code>reusable</code> overloads of
         * {@link #getSafParameter}, {@link #getSafParameterForRead} and
         * {@link #getSafParameterForWrite}.
         *
         * <p>When a url is reusable, automatic unregistration is disabled for that url. Therefore,
         * it will be the developer's responsibility to unregister it via the
         * {@link #unregisterSafProtocolUrl} method.
         *
         * @param safUrlsReusable set to true to make newly created SAF protocol urls reusable by default
         */
        @JvmStatic
        fun setSafUrlsReusable(safUrlsReusable: Boolean) {
            this.safUrlsReusable.compareAndSet(!safUrlsReusable, safUrlsReusable)
        }

        /**
         * Converts session state to string.
         *
         * @param state session state
         * @return string value
         */
        @JvmStatic
        @NonNull
        fun sessionStateToString(@NonNull state: SessionState): String {
            return state.toString()
        }

        /**
         * <p>Parses the given command into arguments. Uses space character to split the arguments.
         * Supports single and double quote characters.
         *
         * @param command string command
         * @return array of arguments
         */
        @JvmStatic
        @NonNull
        fun parseArguments(@NonNull command: String): Array<String> {
            val argumentList = ArrayList<String>()
            var currentArgument = StringBuilder()

            var singleQuoteStarted = false
            var doubleQuoteStarted = false

            for (i in command.indices) {
                val previousChar: Char?
                previousChar = if (i > 0) {
                    command[i - 1]
                } else {
                    null
                }
                val currentChar = command[i]

                if (currentChar == ' ') {
                    if (singleQuoteStarted || doubleQuoteStarted) {
                        currentArgument.append(currentChar)
                    } else if (currentArgument.isNotEmpty()) {
                        argumentList.add(currentArgument.toString())
                        currentArgument = StringBuilder()
                    }
                } else if (currentChar == '\'' && (previousChar == null || previousChar != '\\')) {
                    if (singleQuoteStarted) {
                        singleQuoteStarted = false
                    } else if (doubleQuoteStarted) {
                        currentArgument.append(currentChar)
                    } else {
                        singleQuoteStarted = true
                    }
                } else if (currentChar == '\"' && (previousChar == null || previousChar != '\\')) {
                    if (doubleQuoteStarted) {
                        doubleQuoteStarted = false
                    } else if (singleQuoteStarted) {
                        currentArgument.append(currentChar)
                    } else {
                        doubleQuoteStarted = true
                    }
                } else {
                    currentArgument.append(currentChar)
                }
            }

            if (currentArgument.isNotEmpty()) {
                argumentList.add(currentArgument.toString())
            }

            return argumentList.toTypedArray()
        }

        /**
         * <p>Concatenates arguments into a string adding a space character between two arguments.
         *
         * @param arguments arguments
         * @return concatenated string containing all arguments
         */
        @JvmStatic
        @NonNull
        fun argumentsToString(@Nullable arguments: Array<String>?): String {
            arguments ?: return "null"

            return buildString {
                for (i in arguments.indices) {
                    if (i > 0) {
                        append(" ")
                    }
                    append(arguments[i])
                }
            }
        }

        /**
         * <p>Enables redirection natively.
         */
        @JvmStatic
        private external fun enableNativeRedirection()

        /**
         * <p>Disables redirection natively.
         */
        @JvmStatic
        private external fun disableNativeRedirection()

        /**
         * Returns native log level.
         *
         * @return log level
         */
        @JvmStatic
        external fun getNativeLogLevel(): Int

        /**
         * Sets native log level
         *
         * @param level log level
         */
        @JvmStatic
        private external fun setNativeLogLevel(level: Int)

        /**
         * <p>Returns FFmpeg version bundled within the library natively.
         *
         * @return FFmpeg version
         */
        @JvmStatic
        private external fun getNativeFFmpegVersion(): String

        /**
         * <p>Returns FFmpegKit library version natively.
         *
         * @return FFmpegKit version
         */
        @JvmStatic
        private external fun getNativeVersion(): String

        /**
         * <p>Returns the native FFmpegKit package name.
         *
         * @return native FFmpegKit package name
         */
        @JvmStatic
        external fun getNativePackageName(): String

        /**
         * <p>Synchronously executes FFmpeg natively.
         *
         * @param sessionId id of the session
         * @param arguments FFmpeg command options/arguments as string array
         * @return {@link ReturnCode#SUCCESS} on successful execution and {@link ReturnCode#CANCEL} on
         * user cancel. Other non-zero values are returned on error. Use {@link ReturnCode} class to
         * handle the value
         */
        @JvmStatic
        private external fun nativeFFmpegExecute(sessionId: Long, arguments: Array<String>): Int

        /**
         * <p>Synchronously executes FFprobe natively.
         *
         * @param sessionId id of the session
         * @param arguments FFprobe command options/arguments as string array
         * @return {@link ReturnCode#SUCCESS} on successful execution and {@link ReturnCode#CANCEL} on
         * user cancel. Other non-zero values are returned on error. Use {@link ReturnCode} class to
         * handle the value
         */
        @JvmStatic
        external fun nativeFFprobeExecute(sessionId: Long, arguments: Array<String>): Int

        /**
         * <p>Cancels an ongoing FFmpeg operation natively. This method does not wait for termination
         * to complete and returns immediately.
         *
         * @param sessionId id of the session
         */
        @JvmStatic
        external fun nativeFFmpegCancel(sessionId: Long)

        /**
         * <p>Returns the number of native messages that are not transmitted to the Java callbacks for
         * this session natively.
         *
         * @param sessionId id of the session
         * @return number of native messages that are not transmitted to the Java callbacks for
         * this session natively
         */
        @JvmStatic
        external fun messagesInTransmit(sessionId: Long): Int

        /**
         * <p>Creates a new named pipe to use in <code>FFmpeg</code> operations natively.
         *
         * <p>Please note that creator is responsible of closing created pipes.
         *
         * @param ffmpegPipePath full path of ffmpeg pipe
         * @return zero on successful creation, non-zero on error
         */
        @JvmStatic
        private external fun registerNewNativeFFmpegPipe(ffmpegPipePath: String): Int

        @JvmStatic
        private external fun registerNativeFFmpegKitInputBuffer(data: ByteArray): Long

        @JvmStatic
        private external fun registerNativeFFmpegKitInputDirectBuffer(
            byteBuffer: ByteBuffer,
            size: Int
        ): Long

        @JvmStatic
        private external fun registerNativeFFmpegKitOutputBuffer(
            initialCapacity: Long,
            maxCapacity: Long
        ): Long

        @JvmStatic
        private external fun getNativeFFmpegKitBufferSize(id: Long): Long

        @JvmStatic
        private external fun getNativeFFmpegKitOutputBuffer(id: Long): ByteArray?

        @JvmStatic
        private external fun getNativeFFmpegKitOutputBufferDirect(id: Long): ByteBuffer?

        @JvmStatic
        private external fun unregisterNativeFFmpegKitBuffer(id: Long)

        @JvmStatic
        private external fun registerNativeFFmpegKitStream(capacity: Long, type: Int): Long

        @JvmStatic
        private external fun nativeFFmpegKitStreamWrite(
            id: Long,
            data: ByteArray,
            offset: Int,
            length: Int,
            timeoutMs: Int
        ): Int

        @JvmStatic
        private external fun nativeFFmpegKitStreamRead(
            id: Long,
            maxBytes: Int,
            timeoutMs: Int
        ): ByteArray

        @JvmStatic
        private external fun nativeFFmpegKitStreamCloseInput(id: Long)

        @JvmStatic
        private external fun unregisterNativeFFmpegKitStream(id: Long)

        /**
         * <p>Returns FFmpegKit library build date natively.
         *
         * @return FFmpegKit library build date
         */
        @JvmStatic
        private external fun getNativeBuildDate(): String

        /**
         * <p>Sets an environment variable natively.
         *
         * @param variableName  environment variable name
         * @param variableValue environment variable value
         * @return zero on success, non-zero on error
         */
        @JvmStatic
        private external fun setNativeEnvironmentVariable(
            variableName: String,
            variableValue: String
        ): Int

        /**
         * <p>Registers a new ignored signal natively. Ignored signals are not handled by
         * <code>FFmpegKit</code> library.
         *
         * @param signum signal number
         */
        @JvmStatic
        private external fun ignoreNativeSignal(signum: Int)
    }
}
