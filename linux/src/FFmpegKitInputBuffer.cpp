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

#include "FFmpegKitInputBuffer.h"
#include "FFmpegKitConfig.h"
#include "FFmpegKitProtocolUrl.h"
#include <stdexcept>

std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>
ffmpegkit::FFmpegKitInputBuffer::fromByteArray(
    const std::vector<uint8_t> &data, const std::string &extension) {
    long id = ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(data);
    if (id == 0) {
        throw std::runtime_error("Failed to register FFmpegKit input buffer.");
    }

    return std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>(
        new ffmpegkit::FFmpegKitInputBuffer(id, extension));
}

std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>
ffmpegkit::FFmpegKitInputBuffer::fromBytes(const uint8_t *data,
                                           const size_t size,
                                           const std::string &extension) {
    if (data == NULL && size > 0) {
        throw std::invalid_argument(
            "data must not be null when size is positive");
    }

    long id =
        ffmpegkit::FFmpegKitConfig::registerFFmpegKitInputBuffer(data, size);
    if (id == 0) {
        throw std::runtime_error("Failed to register FFmpegKit input buffer.");
    }

    return std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>(
        new ffmpegkit::FFmpegKitInputBuffer(id, extension));
}

ffmpegkit::FFmpegKitInputBuffer::~FFmpegKitInputBuffer() { close(); }

ffmpegkit::FFmpegKitInputBuffer::FFmpegKitInputBuffer(
    const long id, const std::string &extension)
    : _id{id}, _url{ffmpegkit::buildFFmpegKitUrl("ffkitmem", id, extension)},
      _closed{false} {}

std::string ffmpegkit::FFmpegKitInputBuffer::getUrl() {
    ensureOpen();
    return _url;
}

long ffmpegkit::FFmpegKitInputBuffer::getSize() {
    ensureOpen();
    return ffmpegkit::FFmpegKitConfig::getFFmpegKitBufferSize(_id);
}

void ffmpegkit::FFmpegKitInputBuffer::close() {
    if (!_closed) {
        ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitBuffer(_id);
        _closed = true;
    }
}

void ffmpegkit::FFmpegKitInputBuffer::ensureOpen() {
    if (_closed) {
        throw std::runtime_error("FFmpegKit input buffer is closed.");
    }
}
