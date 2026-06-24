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
 * <p>Enumeration for Android ABIs.
 */
enum class Abi(private val abiName: String) {

    /**
     * Represents armeabi-v7a ABI with NEON support
     */
    ABI_ARMV7A_NEON("armeabi-v7a-neon"),

    /**
     * Represents armeabi-v7a ABI
     */
    ABI_ARMV7A("armeabi-v7a"),

    /**
     * Represents armeabi ABI
     */
    ABI_ARM("armeabi"),

    /**
     * Represents x86 ABI
     */
    ABI_X86("x86"),

    /**
     * Represents x86_64 ABI
     */
    ABI_X86_64("x86_64"),

    /**
     * Represents arm64-v8a ABI
     */
    ABI_ARM64_V8A("arm64-v8a"),

    /**
     * Represents not supported ABIs
     */
    ABI_UNKNOWN("unknown");

    /**
     * Returns the ABI name.
     *
     * @return ABI name as defined in Android NDK documentation
     */
    open fun getName(): String = abiName

    companion object {

        /**
         * <p>Returns the enumeration defined for the given ABI name.
         *
         * @param abiName ABI name
         * @return enumeration defined for the ABI name
         */
        @JvmStatic
        fun from(abiName: String?): Abi = when (abiName) {
            ABI_ARM.getName() -> ABI_ARM
            ABI_ARMV7A.getName() -> ABI_ARMV7A
            ABI_ARMV7A_NEON.getName() -> ABI_ARMV7A_NEON
            ABI_ARM64_V8A.getName() -> ABI_ARM64_V8A
            ABI_X86.getName() -> ABI_X86
            ABI_X86_64.getName() -> ABI_X86_64
            else -> ABI_UNKNOWN
        }
    }
}
