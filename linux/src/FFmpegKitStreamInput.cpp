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

#include "FFmpegKitStreamInput.h"
#include "FFmpegKitConfig.h"
#include "FFmpegKitProtocolUrl.h"
#include <stdexcept>

static const long DefaultStreamCapacity = 1024 * 1024;
static const int StreamTypeInput = 1;

std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>
ffmpegkit::FFmpegKitStreamInput::create(const std::string &extension) {
    return ffmpegkit::FFmpegKitStreamInput::create(extension,
                                                   DefaultStreamCapacity);
}

std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>
ffmpegkit::FFmpegKitStreamInput::create(const std::string &extension,
                                        const long capacity) {
    if (capacity <= 0) {
        throw std::invalid_argument("capacity must be positive");
    }

    long id = ffmpegkit::FFmpegKitConfig::registerFFmpegKitStream(
        capacity, StreamTypeInput);
    if (id == 0) {
        throw std::runtime_error("Failed to register FFmpegKit input stream.");
    }

    return std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>(
        new ffmpegkit::FFmpegKitStreamInput(id, extension));
}

ffmpegkit::FFmpegKitStreamInput::~FFmpegKitStreamInput() { close(); }

ffmpegkit::FFmpegKitStreamInput::FFmpegKitStreamInput(
    const long id, const std::string &extension)
    : _id{id}, _url{ffmpegkit::buildFFmpegKitUrl("ffkitstream", id, extension)},
      _closed{false}, _inputClosed{false} {}

std::string ffmpegkit::FFmpegKitStreamInput::getUrl() {
    ensureOpen();
    return _url;
}

int ffmpegkit::FFmpegKitStreamInput::write(
    const std::vector<uint8_t> &data) {
    return write(data, -1);
}

int ffmpegkit::FFmpegKitStreamInput::write(const std::vector<uint8_t> &data,
                                           const int timeoutMs) {
    return write(data, 0, data.size(), timeoutMs);
}

int ffmpegkit::FFmpegKitStreamInput::write(const std::vector<uint8_t> &data,
                                           const size_t offset,
                                           const size_t length,
                                           const int timeoutMs) {
    ensureOpen();
    if (_inputClosed) {
        throw std::runtime_error(
            "FFmpegKit input stream is closed for writing.");
    }
    if (offset > data.size() || length > (data.size() - offset)) {
        throw std::invalid_argument("offset and length must fit inside data");
    }

    int written = ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
        _id, data, offset, length, timeoutMs);
    if (written < 0) {
        throw std::runtime_error("Failed to write FFmpegKit input stream: " +
                                 std::to_string(written) + ".");
    }

    return written;
}

int ffmpegkit::FFmpegKitStreamInput::write(const uint8_t *data,
                                           const size_t length,
                                           const int timeoutMs) {
    ensureOpen();
    if (_inputClosed) {
        throw std::runtime_error(
            "FFmpegKit input stream is closed for writing.");
    }
    if (data == NULL && length > 0) {
        throw std::invalid_argument(
            "data must not be null when length is positive");
    }

    int written = ffmpegkit::FFmpegKitConfig::writeFFmpegKitStream(
        _id, data, length, timeoutMs);
    if (written < 0) {
        throw std::runtime_error("Failed to write FFmpegKit input stream: " +
                                 std::to_string(written) + ".");
    }

    return written;
}

void ffmpegkit::FFmpegKitStreamInput::closeInput() {
    if (!_closed && !_inputClosed) {
        ffmpegkit::FFmpegKitConfig::closeFFmpegKitStreamInput(_id);
        _inputClosed = true;
    }
}

void ffmpegkit::FFmpegKitStreamInput::close() {
    if (!_closed) {
        closeInput();
        ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(_id);
        _closed = true;
    }
}

void ffmpegkit::FFmpegKitStreamInput::ensureOpen() {
    if (_closed) {
        throw std::runtime_error("FFmpegKit input stream is closed.");
    }
}
