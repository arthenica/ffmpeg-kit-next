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
 * <p>Detects the running ABI name natively using Google <code>cpu-features</code> library.
 */
open class AbiDetect private constructor() {

    companion object {
        internal const val ARM_V7A = "arm-v7a"
        internal const val ARM_V7A_NEON = "arm-v7a-neon"

        @JvmStatic
        private var armV7aNeonLoaded = false

        init {
            armV7aNeonLoaded = false
            NativeLoader.loadFFmpegKitAbiDetect()

            /* ALL LIBRARIES LOADED AT STARTUP */
            FFmpegKit::class.java.name
            FFmpegKitConfig::class.java.name
            FFprobeKit::class.java.name
        }

        @JvmStatic
        internal fun setArmV7aNeonLoaded() {
            armV7aNeonLoaded = true
        }

        /**
         * <p>Returns the ABI name loaded.
         *
         * @return ABI name loaded
         */
        @JvmStatic
        fun getAbi(): String = if (armV7aNeonLoaded) ARM_V7A_NEON else getNativeAbi()

        /**
         * <p>Returns the ABI name of the cpu running.
         *
         * @return ABI name of the cpu running
         * @deprecated Use <code>android.os.Build#SUPPORTED_ABIS</code> to
         * query the ABIs supported by the device or the
         * {@link AbiDetect#getAbi()} method to obtain the ABI loaded
         */
        @JvmStatic
        @Suppress("DEPRECATION")
        @Deprecated(
            "Use android.os.Build#SUPPORTED_ABIS to query the ABIs supported by the device or the AbiDetect#getAbi() method to obtain the ABI loaded",
            ReplaceWith("getAbi()")
        )
        fun getCpuAbi(): String = getNativeCpuAbi()

        /**
         * <p>Returns the ABI name loaded natively.
         *
         * @return ABI name loaded
         */
        @JvmStatic
        external fun getNativeAbi(): String

        /**
         * <p>Returns the ABI name of the cpu running natively.
         *
         * @return ABI name of the cpu running
         * @deprecated Use <code>android.os.Build#SUPPORTED_ABIS</code> to
         * query the ABIs supported by the device or the
         * {@link AbiDetect#getNativeAbi()} method to obtain the ABI loaded
         */
        @JvmStatic
        @Deprecated(
            "Use android.os.Build#SUPPORTED_ABIS to query the ABIs supported by the device or the AbiDetect#getNativeAbi() method to obtain the ABI loaded",
            ReplaceWith("getNativeAbi()")
        )
        external fun getNativeCpuAbi(): String

        /**
         * <p>Returns whether FFmpegKit release is a long term release or not natively.
         *
         * @return yes or no
         */
        @JvmStatic
        external fun isNativeLTSBuild(): Boolean

        /**
         * <p>Returns the build configuration for <code>FFmpeg</code> natively.
         *
         * @return build configuration string
         */
        @JvmStatic
        external fun getNativeBuildConf(): String

        /**
         * <p>Returns the minimum Android API level required to run this library.
         *
         * @return minimum Android API level required to run this library
         */
        @JvmStatic
        external fun getNativeMinSdk(): String
    }
}
