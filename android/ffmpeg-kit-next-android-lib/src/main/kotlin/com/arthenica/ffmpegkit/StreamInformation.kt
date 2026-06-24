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

/**
 * Stream information class.
 */
open class StreamInformation(private val jsonObject: JSONObject?) {

    /**
     * Returns stream index.
     *
     * @return stream index, starting from zero
     */
    open fun getIndex(): Long? = getNumberProperty(KEY_INDEX)

    /**
     * Returns stream type.
     *
     * @return stream type; audio or video
     */
    open fun getType(): String? = getStringProperty(KEY_TYPE)

    /**
     * Returns stream codec.
     *
     * @return stream codec
     */
    open fun getCodec(): String? = getStringProperty(KEY_CODEC)

    /**
     * Returns stream codec in long format.
     *
     * @return stream codec with additional profile and mode information
     */
    open fun getCodecLong(): String? = getStringProperty(KEY_CODEC_LONG)

    /**
     * Returns stream format.
     *
     * @return stream format
     */
    open fun getFormat(): String? = getStringProperty(KEY_FORMAT)

    /**
     * Returns width.
     *
     * @return width in pixels
     */
    open fun getWidth(): Long? = getNumberProperty(KEY_WIDTH)

    /**
     * Returns height.
     *
     * @return height in pixels
     */
    open fun getHeight(): Long? = getNumberProperty(KEY_HEIGHT)

    /**
     * Returns bitrate.
     *
     * @return bitrate in kb/s
     */
    open fun getBitrate(): String? = getStringProperty(KEY_BIT_RATE)

    /**
     * Returns sample rate.
     *
     * @return sample rate in hz
     */
    open fun getSampleRate(): String? = getStringProperty(KEY_SAMPLE_RATE)

    /**
     * Returns sample format.
     *
     * @return sample format
     */
    open fun getSampleFormat(): String? = getStringProperty(KEY_SAMPLE_FORMAT)

    /**
     * Returns channel layout.
     *
     * @return channel layout
     */
    open fun getChannelLayout(): String? = getStringProperty(KEY_CHANNEL_LAYOUT)

    /**
     * Returns sample aspect ratio.
     *
     * @return sample aspect ratio
     */
    open fun getSampleAspectRatio(): String? = getStringProperty(KEY_SAMPLE_ASPECT_RATIO)

    /**
     * Returns display aspect ratio.
     *
     * @return display aspect ratio
     */
    open fun getDisplayAspectRatio(): String? = getStringProperty(KEY_DISPLAY_ASPECT_RATIO)

    /**
     * Returns display aspect ratio.
     *
     * @return average frame rate in fps
     */
    open fun getAverageFrameRate(): String? = getStringProperty(KEY_AVERAGE_FRAME_RATE)

    /**
     * Returns real frame rate.
     *
     * @return real frame rate in tbr
     */
    open fun getRealFrameRate(): String? = getStringProperty(KEY_REAL_FRAME_RATE)

    /**
     * Returns time base.
     *
     * @return time base in tbn
     */
    open fun getTimeBase(): String? = getStringProperty(KEY_TIME_BASE)

    /**
     * Returns codec time base.
     *
     * @return codec time base in tbc
     */
    open fun getCodecTimeBase(): String? = getStringProperty(KEY_CODEC_TIME_BASE)

    /**
     * Returns all tags.
     * The returned JSONObject is a mutable view of parsed stream information.
     *
     * @return tags object
     */
    open fun getTags(): JSONObject? = getProperty(KEY_TAGS)

    /**
     * Returns the stream property associated with the key.
     *
     * @param key property key
     * @return stream property as string or null if the key is not found
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
     * Returns the stream property associated with the key.
     *
     * @param key property key
     * @return stream property as Long or null if the key is not found
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
     * Returns the stream property associated with the key.
     * The returned JSONObject is a mutable view of parsed stream information.
     *
     * @param key property key
     * @return stream property as a JSONObject or null if the key is not found
     */
    open fun getProperty(key: String): JSONObject? {
        val allProperties = getAllProperties() ?: return null

        return allProperties.optJSONObject(key)
    }

    /**
     * Returns all stream properties defined.
     * The returned JSONObject is a mutable view of parsed stream information.
     *
     * @return all stream properties as a JSONObject or null if no properties are defined
     */
    open fun getAllProperties(): JSONObject? = jsonObject

    companion object {

        /* COMMON KEYS */
        const val KEY_INDEX = "index"
        const val KEY_TYPE = "codec_type"
        const val KEY_CODEC = "codec_name"
        const val KEY_CODEC_LONG = "codec_long_name"
        const val KEY_FORMAT = "pix_fmt"
        const val KEY_WIDTH = "width"
        const val KEY_HEIGHT = "height"
        const val KEY_BIT_RATE = "bit_rate"
        const val KEY_SAMPLE_RATE = "sample_rate"
        const val KEY_SAMPLE_FORMAT = "sample_fmt"
        const val KEY_CHANNEL_LAYOUT = "channel_layout"
        const val KEY_SAMPLE_ASPECT_RATIO = "sample_aspect_ratio"
        const val KEY_DISPLAY_ASPECT_RATIO = "display_aspect_ratio"
        const val KEY_AVERAGE_FRAME_RATE = "avg_frame_rate"
        const val KEY_REAL_FRAME_RATE = "r_frame_rate"
        const val KEY_TIME_BASE = "time_base"
        const val KEY_CODEC_TIME_BASE = "codec_time_base"
        const val KEY_TAGS = "tags"
    }

}
