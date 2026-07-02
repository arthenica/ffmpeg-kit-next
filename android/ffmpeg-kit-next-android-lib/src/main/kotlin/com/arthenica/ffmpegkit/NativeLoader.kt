/*
 * Copyright (c) 2021, 2026 Taner Sener
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

import android.os.Build
import com.arthenica.smartexception.java.Exceptions
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Responsible of loading native libraries.
 */
open class NativeLoader {

    companion object {

        @JvmStatic
        internal fun isTestModeDisabled(): Boolean {
            return System.getProperty("enable.ffmpegkit.test.mode") == null
        }

        private fun loadLibrary(libraryName: String) {
            if (isTestModeDisabled()) {
                try {
                    System.loadLibrary(libraryName)
                } catch (e: UnsatisfiedLinkError) {
                    throw Error(
                        String.format(
                            "FFmpegKit failed to start on %s.",
                            getDeviceDebugInformation()
                        ), e
                    )
                }
            }
        }

        private fun loadNativeAbi(): String {
            return if (isTestModeDisabled()) {
                AbiDetect.getNativeAbi()
            } else {
                Abi.ABI_X86_64.getName()
            }
        }

        @JvmStatic
        internal fun loadAbi(): String {
            return if (isTestModeDisabled()) {
                AbiDetect.getAbi()
            } else {
                Abi.ABI_X86_64.getName()
            }
        }

        @JvmStatic
        internal fun loadMinSdk(): String {
            return if (isTestModeDisabled()) {
                AbiDetect.getNativeMinSdk()
            } else {
                String.format(Locale.getDefault(), "%d", Build.VERSION.SDK_INT)
            }
        }

        @JvmStatic
        internal fun loadVersion(): String {
            val version = "8.1.0"

            return if (isTestModeDisabled()) {
                FFmpegKitConfig.getVersion()
            } else {
                version
            }
        }

        @JvmStatic
        internal fun loadLogLevel(): Int {
            return if (isTestModeDisabled()) {
                FFmpegKitConfig.getNativeLogLevel()
            } else {
                Level.AV_LOG_DEBUG.value
            }
        }

        @JvmStatic
        internal fun loadBuildDate(): String {
            return if (isTestModeDisabled()) {
                FFmpegKitConfig.getBuildDate()
            } else {
                SimpleDateFormat("yyyyMMdd", Locale.getDefault()).format(Date())
            }
        }

        @JvmStatic
        internal fun enableRedirection() {
            if (isTestModeDisabled()) {
                FFmpegKitConfig.enableRedirection()
            }
        }

        @JvmStatic
        internal fun loadFFmpegKitAbiDetect() {
            loadLibrary("ffmpegkit_abidetect")
        }

        @JvmStatic
        internal fun loadFFmpeg(): Boolean {
            val nativeFFmpegTriedAndFailed = false
            return nativeFFmpegTriedAndFailed
        }

        @JvmStatic
        internal fun loadFFmpegKit(nativeFFmpegTriedAndFailed: Boolean) {
            var nativeFFmpegKitLoaded = false

            if (!nativeFFmpegTriedAndFailed && AbiDetect.ARM_V7A == loadNativeAbi()) {
                try {

                    /*
                     * THE TRY TO LOAD ARM-V7A-NEON FIRST. IF NOT LOAD DEFAULT ARM-V7A
                     */

                    loadLibrary("ffmpegkit_armv7a_neon")
                    nativeFFmpegKitLoaded = true
                    AbiDetect.setArmV7aNeonLoaded()
                } catch (e: Error) {
                    android.util.Log.i(
                        FFmpegKitConfig.TAG,
                        String.format(
                            "NEON supported armeabi-v7a ffmpegkit library not found. Loading default armeabi-v7a library.%s",
                            Exceptions.getStackTraceString(e)
                        )
                    )
                }
            }

            if (!nativeFFmpegKitLoaded) {
                loadLibrary("ffmpegkit")
            }
        }

        @JvmStatic
        @Suppress("DEPRECATION")
        internal fun getDeviceDebugInformation(): String = buildString {
            append("brand: ")
            append(Build.BRAND)
            append(", model: ")
            append(Build.MODEL)
            append(", device: ")
            append(Build.DEVICE)
            append(", api level: ")
            append(Build.VERSION.SDK_INT)
            append(", abis: ")
            append(FFmpegKitConfig.argumentsToString(Build.SUPPORTED_ABIS))
            append(", 32bit abis: ")
            append(FFmpegKitConfig.argumentsToString(Build.SUPPORTED_32_BIT_ABIS))
            append(", 64bit abis: ")
            append(FFmpegKitConfig.argumentsToString(Build.SUPPORTED_64_BIT_ABIS))
        }
    }
}
