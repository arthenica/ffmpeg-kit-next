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

open class ReturnCode(open val value: Int) {

    open fun isValueSuccess(): Boolean = value == SUCCESS
    open fun isValueError(): Boolean = value != SUCCESS && value != CANCEL
    open fun isValueCancel(): Boolean = value == CANCEL

    override fun toString(): String = value.toString()

    companion object {
        const val SUCCESS = 0
        const val CANCEL = 255

        @JvmStatic
        fun isSuccess(returnCode: ReturnCode?): Boolean =
            returnCode != null && returnCode.value == SUCCESS

        @JvmStatic
        fun isCancel(returnCode: ReturnCode?): Boolean =
            returnCode != null && returnCode.value == CANCEL
    }
}
