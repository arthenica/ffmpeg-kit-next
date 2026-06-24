/*
 * Copyright (c) 2021-2022, 2026 Taner Sener
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

open class Chapter(private val jsonObject: JSONObject?) {

    open fun getId(): Long? = getNumberProperty(KEY_ID)

    open fun getTimeBase(): String? = getStringProperty(KEY_TIME_BASE)

    open fun getStart(): Long? = getNumberProperty(KEY_START)

    open fun getStartTime(): String? = getStringProperty(KEY_START_TIME)

    open fun getEnd(): Long? = getNumberProperty(KEY_END)

    open fun getEndTime(): String? = getStringProperty(KEY_END_TIME)

    /**
     * Returns all tags.
     * The returned JSONObject is a mutable view of parsed chapter information.
     *
     * @return tags object
     */
    open fun getTags(): JSONObject? = getProperty(KEY_TAGS)

    /**
     * Returns the chapter property associated with the key.
     *
     * @param key property key
     * @return chapter property as string or null if the key is not found
     */
    open fun getStringProperty(key: String): String? {
        val allProperties = getAllProperties() ?: return null

        return if (allProperties.has(key)) {
            allProperties.optString(key)
        } else {
            null
        }
    }

    /**
     * Returns the chapter property associated with the key.
     *
     * @param key property key
     * @return chapter property as Long or null if the key is not found
     */
    open fun getNumberProperty(key: String): Long? {
        val allProperties = getAllProperties() ?: return null

        return if (allProperties.has(key)) {
            allProperties.optLong(key)
        } else {
            null
        }
    }

    /**
     * Returns the chapter property associated with the key.
     * The returned JSONObject is a mutable view of parsed chapter information.
     *
     * @param key property key
     * @return chapter property as a JSONObject or null if the key is not found
     */
    open fun getProperty(key: String): JSONObject? {
        val allProperties = getAllProperties() ?: return null

        return allProperties.optJSONObject(key)
    }

    /**
     * Returns all chapter properties defined.
     * The returned JSONObject is a mutable view of parsed chapter information.
     *
     * @return all chapter properties as a JSONObject or null if no properties are defined
     */
    open fun getAllProperties(): JSONObject? = jsonObject

    companion object {

        /* KEYS */
        const val KEY_ID = "id"
        const val KEY_TIME_BASE = "time_base"
        const val KEY_START = "start"
        const val KEY_START_TIME = "start_time"
        const val KEY_END = "end"
        const val KEY_END_TIME = "end_time"
        const val KEY_TAGS = "tags"
    }

}
