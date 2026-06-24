/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
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
import java.util.ArrayList
import java.util.Collections

/**
 * Media information class.
 */
open class MediaInformation(
    private val jsonObject: JSONObject?,
    streams: @JvmSuppressWildcards List<StreamInformation>,
    chapters: @JvmSuppressWildcards List<Chapter>
) {

    private val streams: List<StreamInformation> =
        Collections.unmodifiableList(ArrayList(streams))

    private val chapters: List<Chapter> =
        Collections.unmodifiableList(ArrayList(chapters))

    /**
     * Returns file name.
     *
     * @return media file name
     */
    open fun getFilename(): String? = getStringFormatProperty(KEY_FILENAME)

    /**
     * Returns format.
     *
     * @return media format
     */
    open fun getFormat(): String? = getStringFormatProperty(KEY_FORMAT)

    /**
     * Returns long format.
     *
     * @return media long format
     */
    open fun getLongFormat(): String? = getStringFormatProperty(KEY_FORMAT_LONG)

    /**
     * Returns duration.
     *
     * @return media duration in "seconds.microseconds" format
     */
    open fun getDuration(): String? = getStringFormatProperty(KEY_DURATION)

    /**
     * Returns start time.
     *
     * @return media start time in milliseconds
     */
    open fun getStartTime(): String? = getStringFormatProperty(KEY_START_TIME)

    /**
     * Returns size.
     *
     * @return media size in bytes
     */
    open fun getSize(): String? = getStringFormatProperty(KEY_SIZE)

    /**
     * Returns bitrate.
     *
     * @return media bitrate in kb/s
     */
    open fun getBitrate(): String? = getStringFormatProperty(KEY_BIT_RATE)

    /**
     * Returns all tags.
     * The returned JSONObject is a mutable view of parsed media information.
     *
     * @return tags as a JSONObject
     */
    open fun getTags(): JSONObject? = getFormatProperty(KEY_TAGS)

    /**
     * Returns all streams.
     *
     * @return list of streams
     */
    open fun getStreams(): List<StreamInformation> = streams

    /**
     * Returns all chapters.
     *
     * @return list of chapters
     */
    open fun getChapters(): List<Chapter> = chapters

    /**
     * Returns the property associated with the key.
     *
     * @param key property key
     * @return property as string or null if the key is not found
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
     * Returns the property associated with the key.
     *
     * @param key property key
     * @return property as Long or null if the key is not found
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
     * Returns the property associated with the key.
     * The returned JSONObject is a mutable view of parsed media information.
     *
     * @param key property key
     * @return property as a JSONObject or null if the key is not found
     */
    open fun getProperty(key: String): JSONObject? {
        val allProperties = getAllProperties() ?: return null

        return allProperties.optJSONObject(key)
    }

    /**
     * Returns the format property associated with the key.
     *
     * @param key property key
     * @return format property as string or null if the key is not found
     */
    open fun getStringFormatProperty(key: String): String? {
        val formatProperties = getFormatProperties() ?: return null

        return if (formatProperties.has(key)) {
            formatProperties.optString(key)
        } else {
            null
        }
    }

    /**
     * Returns the format property associated with the key.
     *
     * @param key property key
     * @return format property as Long or null if the key is not found
     */
    open fun getNumberFormatProperty(key: String): Long? {
        val formatProperties = getFormatProperties() ?: return null

        return if (formatProperties.has(key)) {
            formatProperties.optLong(key)
        } else {
            null
        }
    }

    /**
     * Returns the format property associated with the key.
     * The returned JSONObject is a mutable view of parsed media information.
     *
     * @param key property key
     * @return format property as a JSONObject or null if the key is not found
     */
    open fun getFormatProperty(key: String): JSONObject? {
        val formatProperties = getFormatProperties() ?: return null

        return formatProperties.optJSONObject(key)
    }

    /**
     * Returns all format properties defined.
     * The returned JSONObject is a mutable view of parsed media information.
     *
     * @return all format properties as a JSONObject or null if no format properties are defined
     */
    open fun getFormatProperties(): JSONObject? = jsonObject?.optJSONObject(KEY_FORMAT_PROPERTIES)

    /**
     * Returns all properties defined.
     * The returned JSONObject is a mutable view of parsed media information.
     *
     * @return all properties as a JSONObject or null if no properties are defined
     */
    open fun getAllProperties(): JSONObject? = jsonObject

    companion object {

        /* COMMON KEYS */
        const val KEY_FORMAT_PROPERTIES = "format"
        const val KEY_FILENAME = "filename"
        const val KEY_FORMAT = "format_name"
        const val KEY_FORMAT_LONG = "format_long_name"
        const val KEY_START_TIME = "start_time"
        const val KEY_DURATION = "duration"
        const val KEY_SIZE = "size"
        const val KEY_BIT_RATE = "bit_rate"
        const val KEY_TAGS = "tags"
    }

}
