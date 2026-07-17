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

#ifndef FFMPEG_KIT_OUTPUT_BUFFER_H
#define FFMPEG_KIT_OUTPUT_BUFFER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * Seekable in-memory output that can be used in FFmpeg commands with an
 * ffkitmem: URL.
 */
class FFmpegKitOutputBuffer {
  public:
    static std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>
    create(const std::string &extension);

    static std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>
    create(const std::string &extension, const long initialCapacity,
           const long maxCapacity);

    ~FFmpegKitOutputBuffer();

    std::string getUrl();

    long getSize();

    std::shared_ptr<std::vector<uint8_t>> toByteArray();

    void close();

  private:
    FFmpegKitOutputBuffer(const long id, const std::string &extension);

    void ensureOpen();

    long _id;
    std::string _url;
    bool _closed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_OUTPUT_BUFFER_H
