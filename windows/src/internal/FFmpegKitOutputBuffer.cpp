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

#include "FFmpegKitOutputBuffer.h"
#include "FFmpegKitConfig.h"
#include "FFmpegKitProtocolUrl.h"
#include <stdexcept>

static const long DefaultInitialCapacity = 4096;

std::shared_ptr<ffmpegkit::internal::FFmpegKitOutputBuffer>
ffmpegkit::internal::FFmpegKitOutputBuffer::create(const std::string &extension) {
    return ffmpegkit::internal::FFmpegKitOutputBuffer::create(
        extension, DefaultInitialCapacity, 0);
}

std::shared_ptr<ffmpegkit::internal::FFmpegKitOutputBuffer>
ffmpegkit::internal::FFmpegKitOutputBuffer::create(const std::string &extension,
                                         const long initialCapacity,
                                         const long maxCapacity) {
    if (initialCapacity < 0 || maxCapacity < 0) {
        throw std::invalid_argument("capacities must not be negative");
    }

    long id = ffmpegkit::internal::FFmpegKitConfig::registerFFmpegKitOutputBuffer(
        initialCapacity, maxCapacity);
    if (id == 0) {
        throw std::runtime_error("Failed to register FFmpegKit output buffer.");
    }

    return std::shared_ptr<ffmpegkit::internal::FFmpegKitOutputBuffer>(
        new ffmpegkit::internal::FFmpegKitOutputBuffer(id, extension));
}

ffmpegkit::internal::FFmpegKitOutputBuffer::~FFmpegKitOutputBuffer() { close(); }

ffmpegkit::internal::FFmpegKitOutputBuffer::FFmpegKitOutputBuffer(
    const long id, const std::string &extension)
    : _id{id}, _url{ffmpegkit::internal::buildFFmpegKitUrl("ffkitmem", id, extension)},
      _closed{false} {}

std::string ffmpegkit::internal::FFmpegKitOutputBuffer::getUrl() {
    ensureOpen();
    return _url;
}

long ffmpegkit::internal::FFmpegKitOutputBuffer::getSize() {
    ensureOpen();
    return ffmpegkit::internal::FFmpegKitConfig::getFFmpegKitBufferSize(_id);
}

std::shared_ptr<std::vector<uint8_t>>
ffmpegkit::internal::FFmpegKitOutputBuffer::toByteArray() {
    ensureOpen();
    auto output = ffmpegkit::internal::FFmpegKitConfig::getFFmpegKitOutputBuffer(_id);
    if (output == nullptr) {
        throw std::runtime_error("Failed to read FFmpegKit output buffer.");
    }

    return output;
}

void ffmpegkit::internal::FFmpegKitOutputBuffer::close() {
    if (!_closed) {
        ffmpegkit::internal::FFmpegKitConfig::unregisterFFmpegKitBuffer(_id);
        _closed = true;
    }
}

void ffmpegkit::internal::FFmpegKitOutputBuffer::ensureOpen() {
    if (_closed) {
        throw std::runtime_error("FFmpegKit output buffer is closed.");
    }
}
