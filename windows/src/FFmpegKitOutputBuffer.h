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

#include "FFmpegKitConfig.h"
#include "ffmpegkit_facade.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * <p>An in-memory output buffer that <code>FFmpeg</code> can write into
 * through the <code>ffkitmem</code> protocol.
 *
 * <p>Pass getUrl() where a command expects an output file, then read the
 * result back with toByteArray(). The buffer is unregistered when this object
 * is destroyed.
 */
class FFmpegKitOutputBuffer {
  public:
    /**
     * Registers a new output buffer with the default initial capacity.
     *
     * @param extension file extension hint, for example "mp4"
     * @return created output buffer
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>
    create(const std::string &extension) {
        return create(extension, DefaultInitialCapacity, 0);
    }

    /**
     * Registers a new output buffer.
     *
     * @param extension file extension hint, for example "mp4"
     * @param initialCapacity initial capacity in bytes
     * @param maxCapacity maximum capacity in bytes, zero for the default
     * @return created output buffer
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>
    create(const std::string &extension, const long initialCapacity,
           const long maxCapacity) {
        if (initialCapacity < 0 || maxCapacity < 0) {
            throw ffmpegkit::Exception("capacities must not be negative");
        }
        const long id =
            ffmpegkit::FFmpegKitConfig::registerFFmpegKitOutputBuffer(
                initialCapacity, maxCapacity);
        if (id == 0) {
            throw ffmpegkit::Exception(
                "Failed to register FFmpegKit output buffer.");
        }
        return std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>(
            new ffmpegkit::FFmpegKitOutputBuffer(id, extension));
    }

    ~FFmpegKitOutputBuffer() { close(); }

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
     * Returns the number of bytes written into this buffer so far.
     *
     * @return size in bytes
     */
    long getSize() {
        ensureOpen();
        return ffmpegkit::FFmpegKitConfig::getFFmpegKitBufferSize(_id);
    }

    /**
     * Returns a copy of the contents of this buffer.
     *
     * @return buffer contents
     */
    std::shared_ptr<std::vector<uint8_t>> toByteArray() {
        ensureOpen();
        auto output =
            ffmpegkit::FFmpegKitConfig::getFFmpegKitOutputBuffer(_id);
        if (output == nullptr) {
            throw ffmpegkit::Exception(
                "Failed to read FFmpegKit output buffer.");
        }
        return output;
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
    static const long DefaultInitialCapacity = 4096;

    FFmpegKitOutputBuffer(const long id, const std::string &extension)
        : _id{id},
          _url{ffmpegkit::detail::buildProtocolUrl("ffkitmem", id, extension)},
          _closed{false} {}

    void ensureOpen() {
        if (_closed) {
            throw ffmpegkit::Exception("FFmpegKit output buffer is closed.");
        }
    }

    long _id;
    std::string _url;
    bool _closed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_OUTPUT_BUFFER_H
