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

#ifndef FFMPEG_KIT_STREAM_INPUT_H
#define FFMPEG_KIT_STREAM_INPUT_H

#include "FFmpegKitConfig.h"
#include "ffmpegkit_facade.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffmpegkit {

/**
 * <p>A streaming input that <code>FFmpeg</code> reads from through the
 * <code>ffkitstream</code> protocol while the caller keeps writing into it.
 *
 * <p>Pass getUrl() where a command expects an input file, write with write(),
 * and signal end of input with closeInput().
 */
class FFmpegKitStreamInput {
  public:
    /**
     * Registers a new input stream with the default capacity.
     *
     * @param extension file extension hint, for example "mp4"
     * @return created input stream
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>
    create(const std::string &extension) {
        return create(extension, DefaultStreamCapacity);
    }

    /**
     * Registers a new input stream.
     *
     * @param extension file extension hint, for example "mp4"
     * @param capacity ring buffer capacity in bytes
     * @return created input stream
     */
    static std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>
    create(const std::string &extension, const long capacity) {
        if (capacity <= 0) {
            throw ffmpegkit::Exception("capacity must be positive");
        }
        const long id = ffmpegkit::FFmpegKitConfig::registerFFmpegKitStream(
            capacity, StreamTypeInput);
        if (id == 0) {
            throw ffmpegkit::Exception(
                "Failed to register FFmpegKit input stream.");
        }
        return std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>(
            new ffmpegkit::FFmpegKitStreamInput(id, extension));
    }

    ~FFmpegKitStreamInput() { close(); }

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
     * Writes the whole buffer, waiting indefinitely for room.
     *
     * @param data data to write
     * @return number of bytes written
     */
    int write(const std::vector<uint8_t> &data) { return write(data, -1); }

    /**
     * Writes the whole buffer.
     *
     * @param data data to write
     * @param timeoutMs write timeout in milliseconds, negative to wait forever
     * @return number of bytes written
     */
    int write(const std::vector<uint8_t> &data, const int timeoutMs) {
        return write(data, 0, data.size(), timeoutMs);
    }

    /**
     * Writes a slice of the buffer.
     *
     * @param data data to write
     * @param offset offset of the first byte to write
     * @param length number of bytes to write
     * @param timeoutMs write timeout in milliseconds, negative to wait forever
     * @return number of bytes written
     */
    int write(const std::vector<uint8_t> &data, const size_t offset,
              const size_t length, const int timeoutMs) {
        ensureWritable();
        if (offset > data.size() || length > (data.size() - offset)) {
            throw ffmpegkit::Exception(
                "offset and length must fit inside data");
        }
        return checkWritten(ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
            _id, data, offset, length, timeoutMs));
    }

    /**
     * Writes from a raw pointer.
     *
     * @param data data to write
     * @param length number of bytes to write
     * @param timeoutMs write timeout in milliseconds, negative to wait forever
     * @return number of bytes written
     */
    int write(const uint8_t *data, const size_t length, const int timeoutMs) {
        ensureWritable();
        if (data == nullptr && length > 0) {
            throw ffmpegkit::Exception(
                "data must not be null when length is positive");
        }
        return checkWritten(ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
            _id, data, length, timeoutMs));
    }

    /**
     * Signals end of input, so that readers see end of file once the buffered
     * data is consumed. Safe to call more than once.
     */
    void closeInput() {
        if (!_closed && !_inputClosed) {
            ffmpegkit::FFmpegKitConfig::closeFFmpegKitStreamInput(_id);
            _inputClosed = true;
        }
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
    static const int StreamTypeInput = 1;

    FFmpegKitStreamInput(const long id, const std::string &extension)
        : _id{id},
          _url{ffmpegkit::detail::buildProtocolUrl("ffkitstream", id,
                                                   extension)},
          _closed{false}, _inputClosed{false} {}

    void ensureOpen() {
        if (_closed) {
            throw ffmpegkit::Exception("FFmpegKit input stream is closed.");
        }
    }

    void ensureWritable() {
        ensureOpen();
        if (_inputClosed) {
            throw ffmpegkit::Exception(
                "FFmpegKit input stream is closed for writing.");
        }
    }

    static int checkWritten(const int written) {
        if (written < 0) {
            throw ffmpegkit::Exception(
                "Failed to write FFmpegKit input stream: " +
                std::to_string(written) + ".");
        }
        return written;
    }

    long _id;
    std::string _url;
    bool _closed;
    bool _inputClosed;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_STREAM_INPUT_H
