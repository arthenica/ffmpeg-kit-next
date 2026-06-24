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

import android.util.Log
import androidx.annotation.NonNull
import androidx.annotation.Nullable
import com.arthenica.smartexception.java.Exceptions
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.util.ArrayList

/**
 * A parser that constructs {@link MediaInformation} from FFprobe's json output.
 */
open class MediaInformationJsonParser {

    companion object {

        const val KEY_STREAMS = "streams"
        const val KEY_CHAPTERS = "chapters"

        /**
         * Extracts <code>MediaInformation</code> from the given FFprobe json output. Note that this
         * method does not throw {@link JSONException} as {@link #fromWithError(String)} does and
         * handles errors internally.
         *
         * @param ffprobeJsonOutput FFprobe json output
         * @return created {@link MediaInformation} instance of null if a parsing error occurs
         */
        @Nullable
        @JvmStatic
        fun from(@NonNull ffprobeJsonOutput: String): MediaInformation? {
            return try {
                fromWithError(ffprobeJsonOutput)
            } catch (e: JSONException) {
                Log.e(FFmpegKitConfig.TAG, String.format("MediaInformation parsing failed.%s", Exceptions.getStackTraceString(e)))
                null
            }
        }

        /**
         * Extracts MediaInformation from the given FFprobe json output.
         *
         * @param ffprobeJsonOutput ffprobe json output
         * @return created {@link MediaInformation} instance
         * @throws JSONException if a parsing error occurs
         */
        @NonNull
        @JvmStatic
        @Throws(JSONException::class)
        fun fromWithError(@NonNull ffprobeJsonOutput: String): MediaInformation {
            val jsonObject = JSONObject(ffprobeJsonOutput)
            val streamArray = jsonObject.optJSONArray(KEY_STREAMS)
            val chapterArray = jsonObject.optJSONArray(KEY_CHAPTERS)

            val streamList = ArrayList<StreamInformation>()
            var i = 0
            while (streamArray != null && i < streamArray.length()) {
                val streamObject = streamArray.optJSONObject(i)
                if (streamObject != null) {
                    streamList.add(StreamInformation(streamObject))
                }
                i++
            }

            val chapterList = ArrayList<Chapter>()
            var j = 0
            while (chapterArray != null && j < chapterArray.length()) {
                val chapterObject = chapterArray.optJSONObject(j)
                if (chapterObject != null) {
                    chapterList.add(Chapter(chapterObject))
                }
                j++
            }

            return MediaInformation(jsonObject, streamList, chapterList)
        }
    }
}
