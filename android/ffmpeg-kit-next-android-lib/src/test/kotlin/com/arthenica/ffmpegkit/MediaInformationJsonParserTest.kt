/*
 * Copyright (c) 2026 Taner Sener
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

import org.json.JSONException
import org.junit.Assert
import org.junit.Test

class MediaInformationJsonParserTest {

    @Test
    fun fromReturnsNullForInvalidJson() {
        Assert.assertNull(MediaInformationJsonParser.from("{invalid"))
    }

    @Test
    fun fromWithErrorThrowsForInvalidJson() {
        try {
            MediaInformationJsonParser.fromWithError("{invalid")
            Assert.fail("Expected JSONException")
        } catch (_: JSONException) {
        }
    }

    @Test
    fun fromHandlesMissingStreamsAndChapters() {
        val mediaInformation = MediaInformationJsonParser.from(
            """
            {
              "format": {
                "filename": "audio.wav",
                "format_name": "wav",
                "format_long_name": "WAV / WAVE",
                "duration": "1.000000",
                "size": "1000",
                "bit_rate": "8000"
              }
            }
            """.trimIndent()
        )

        Assert.assertNotNull(mediaInformation)
        Assert.assertEquals("audio.wav", mediaInformation!!.getFilename())
        Assert.assertEquals("wav", mediaInformation.getFormat())
        Assert.assertEquals("WAV / WAVE", mediaInformation.getLongFormat())
        Assert.assertEquals("1.000000", mediaInformation.getDuration())
        Assert.assertEquals("1000", mediaInformation.getSize())
        Assert.assertEquals("8000", mediaInformation.getBitrate())
        Assert.assertTrue(mediaInformation.getStreams().isEmpty())
        Assert.assertTrue(mediaInformation.getChapters().isEmpty())
    }

    @Test
    fun fromSkipsNonObjectStreamAndChapterEntries() {
        val mediaInformation = MediaInformationJsonParser.fromWithError(
            """
            {
              "streams": [
                "not an object",
                {
                  "index": 3,
                  "codec_type": "video",
                  "codec_name": "h264",
                  "width": 1920,
                  "height": 1080,
                  "tags": {
                    "language": "eng"
                  }
                },
                42
              ],
              "chapters": [
                null,
                {
                  "id": 7,
                  "time_base": "1/1000",
                  "start": 0,
                  "start_time": "0.000000",
                  "end": 5000,
                  "end_time": "5.000000"
                }
              ],
              "format": {
                "filename": "video.mp4",
                "format_name": "mov,mp4"
              }
            }
            """.trimIndent()
        )

        Assert.assertEquals("video.mp4", mediaInformation.getFilename())
        Assert.assertEquals(1, mediaInformation.getStreams().size)
        Assert.assertEquals(1, mediaInformation.getChapters().size)

        val stream = mediaInformation.getStreams()[0]
        Assert.assertEquals(3L, stream.getIndex())
        Assert.assertEquals("video", stream.getType())
        Assert.assertEquals("h264", stream.getCodec())
        Assert.assertEquals(1920L, stream.getWidth())
        Assert.assertEquals(1080L, stream.getHeight())
        Assert.assertEquals("eng", stream.getTags()!!.getString("language"))

        val chapter = mediaInformation.getChapters()[0]
        Assert.assertEquals(7L, chapter.getId())
        Assert.assertEquals("1/1000", chapter.getTimeBase())
        Assert.assertEquals(0L, chapter.getStart())
        Assert.assertEquals("0.000000", chapter.getStartTime())
        Assert.assertEquals(5000L, chapter.getEnd())
        Assert.assertEquals("5.000000", chapter.getEndTime())
        Assert.assertNull(chapter.getTags())
    }
}
