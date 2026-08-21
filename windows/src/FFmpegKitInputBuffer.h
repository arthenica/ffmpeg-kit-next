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

#ifndef FFMPEG_KIT_INPUT_BUFFER_H
#define FFMPEG_KIT_INPUT_BUFFER_H

#include "FFmpegKitConfig.h"
#include "ffmpegkit_facade.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * <p>An in-memory input buffer that <code>FFmpeg</code> can read from through
 * the <code>ffkitmem</code> protocol.
 *
 * <p>Pass getUrl() where a command expects an input file. The buffer is
 * unregistered when this object is destroyed.
 */
class FFmpegKitInputBuffer {
  public:
    /**
     * Registers a new input buffer holding a copy of the given data.
     *
     * @param data buffer contents
     * @param extension file extension hint, for example "mp4"
     * @return created input buffer
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>
    fromByteArray(const std::vector<uint8_t> &data,
                  const std::string &extension) {
        return fromBytes(data.data(), data.size(), extension);
    }

    /**
     * Registers a new input buffer holding a copy of the given data.
     *
     * @param data buffer contents
     * @param size number of bytes
     * @param extension file extension hint, for example "mp4"
     * @return created input buffer
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>
    fromBytes(const uint8_t *data, const size_t size,
              const std::string &extension) {
        if (data == nullptr && size > 0) {
            throw ffmpegkit::Exception(
                "data must not be null when size is positive");
        }
        const long id =
            ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(data, size);
        if (id == 0) {
            throw ffmpegkit::Exception(
                "Failed to register FFmpegKit input buffer.");
        }
        return std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>(
            new ffmpegkit::FFmpegKitInputBuffer(id, extension));
    }

    ~FFmpegKitInputBuffer() { close(); }

    /**
     * Returns the url that addresses this buffer.
     *
     * @return ffkitmem url
     */
    std::string getUrl() {
        ensureOpen();
        return _url;
    }

    /**
     * Returns the size of this buffer.
     *
     * @return size in bytes
     */
    long getSize() {
        ensureOpen();
        return ffmpegkit::FFmpegKitConfig::getFFmpegKitBufferSize(_id);
    }

    /**
     * Unregisters this buffer. Safe to call more than once.
     */
    void close() {
        if (!_closed) {
            ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitBuffer(_id);
            _closed = true;
        }
    }

  private:
    FFmpegKitInputBuffer(const long id, const std::string &extension)
        : _id{id},
          _url{ffmpegkit::detail::buildProtocolUrl("ffkitmem", id, extension)},
          _closed{false} {}

    void ensureOpen() {
        if (_closed) {
            throw ffmpegkit::Exception("FFmpegKit input buffer is closed.");
        }
    }

    long _id;
    std::string _url;
    bool _closed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_INPUT_BUFFER_H
