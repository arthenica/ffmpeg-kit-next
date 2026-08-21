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

#ifndef FFMPEG_KIT_STREAM_OUTPUT_H
#define FFMPEG_KIT_STREAM_OUTPUT_H

#include "FFmpegKitConfig.h"
#include "ffmpegkit_facade.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * <p>A streaming output that <code>FFmpeg</code> writes into through the
 * <code>ffkitstream</code> protocol while the caller keeps reading from it.
 *
 * <p>Pass getUrl() where a command expects an output file and drain it with
 * read().
 */
class FFmpegKitStreamOutput {
  public:
    /**
     * Registers a new output stream with the default capacity.
     *
     * @param extension file extension hint, for example "mp4"
     * @return created output stream
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>
    create(const std::string &extension) {
        return create(extension, DefaultStreamCapacity);
    }

    /**
     * Registers a new output stream.
     *
     * @param extension file extension hint, for example "mp4"
     * @param capacity ring buffer capacity in bytes
     * @return created output stream
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>
    create(const std::string &extension, const long capacity) {
        if (capacity <= 0) {
            throw ffmpegkit::Exception("capacity must be positive");
        }
        const long id = ffmpegkit::FFmpegKitConfig::registerFFmpegKitStream(
            capacity, StreamTypeOutput);
        if (id == 0) {
            throw ffmpegkit::Exception(
                "Failed to register FFmpegKit output stream.");
        }
        return std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>(
            new ffmpegkit::FFmpegKitStreamOutput(id, extension));
    }

    ~FFmpegKitStreamOutput() { close(); }

    /**
     * Returns the url that addresses this stream.
     *
     * @return ffkitstream url
     */
    std::string getUrl() {
        ensureOpen();
        return _url;
    }

    /**
     * Reads from this stream, waiting indefinitely for data.
     *
     * @param maxBytes maximum number of bytes to read
     * @return bytes read, or nullptr at end of stream
     */
    std::shared_ptr<std::vector<uint8_t>> read(const int maxBytes) {
        return read(maxBytes, -1);
    }

    /**
     * Reads from this stream.
     *
     * @param maxBytes maximum number of bytes to read
     * @param timeoutMs read timeout in milliseconds, negative to wait forever
     * @return bytes read, or nullptr at end of stream
     */
    std::shared_ptr<std::vector<uint8_t>> read(const int maxBytes,
                                               const int timeoutMs) {
        ensureOpen();
        if (maxBytes < 0) {
            throw ffmpegkit::Exception("maxBytes must not be negative");
        }
        return ffmpegkit::FFmpegKitConfig::readFFmpegKitStream(_id, maxBytes,
                                                               timeoutMs);
    }

    /**
     * Unregisters this stream. Safe to call more than once.
     */
    void close() {
        if (!_closed) {
            ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(_id);
            _closed = true;
        }
    }

  private:
    static const long DefaultStreamCapacity = 1024 * 1024;
    static const int StreamTypeOutput = 2;

    FFmpegKitStreamOutput(const long id, const std::string &extension)
        : _id{id},
          _url{ffmpegkit::detail::buildProtocolUrl("ffkitstream", id,
                                                   extension)},
          _closed{false} {}

    void ensureOpen() {
        if (_closed) {
            throw ffmpegkit::Exception("FFmpegKit output stream is closed.");
        }
    }

    long _id;
    std::string _url;
    bool _closed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_STREAM_OUTPUT_H
