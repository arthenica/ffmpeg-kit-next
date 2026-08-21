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

#include "FFmpegKitStreamOutput.h"
#include "FFmpegKitConfig.h"
#include "FFmpegKitProtocolUrl.h"
#include <stdexcept>

static const long DefaultStreamCapacity = 1024 * 1024;
static const int StreamTypeOutput = 2;

std::shared_ptr<ffmpegkit::internal::FFmpegKitStreamOutput>
ffmpegkit::internal::FFmpegKitStreamOutput::create(const std::string &extension) {
    return ffmpegkit::internal::FFmpegKitStreamOutput::create(extension,
                                                    DefaultStreamCapacity);
}

std::shared_ptr<ffmpegkit::internal::FFmpegKitStreamOutput>
ffmpegkit::internal::FFmpegKitStreamOutput::create(const std::string &extension,
                                         const long capacity) {
    if (capacity <= 0) {
        throw std::invalid_argument("capacity must be positive");
    }

    long id = ffmpegkit::internal::FFmpegKitConfig::registerFFmpegKitStream(
        capacity, StreamTypeOutput);
    if (id == 0) {
        throw std::runtime_error("Failed to register FFmpegKit output stream.");
    }

    return std::shared_ptr<ffmpegkit::internal::FFmpegKitStreamOutput>(
        new ffmpegkit::internal::FFmpegKitStreamOutput(id, extension));
}

ffmpegkit::internal::FFmpegKitStreamOutput::~FFmpegKitStreamOutput() { close(); }

ffmpegkit::internal::FFmpegKitStreamOutput::FFmpegKitStreamOutput(
    const long id, const std::string &extension)
    : _id{id}, _url{ffmpegkit::internal::buildFFmpegKitUrl("ffkitstream", id, extension)},
      _closed{false} {}

std::string ffmpegkit::internal::FFmpegKitStreamOutput::getUrl() {
    ensureOpen();
    return _url;
}

std::shared_ptr<std::vector<uint8_t>>
ffmpegkit::internal::FFmpegKitStreamOutput::read(const int maxBytes) {
    return read(maxBytes, -1);
}

std::shared_ptr<std::vector<uint8_t>>
ffmpegkit::internal::FFmpegKitStreamOutput::read(const int maxBytes,
                                       const int timeoutMs) {
    ensureOpen();
    if (maxBytes < 0) {
        throw std::invalid_argument("maxBytes must not be negative");
    }

    return ffmpegkit::internal::FFmpegKitConfig::readFFmpegKitStream(_id, maxBytes,
                                                           timeoutMs);
}

void ffmpegkit::internal::FFmpegKitStreamOutput::close() {
    if (!_closed) {
        ffmpegkit::internal::FFmpegKitConfig::unregisterFFmpegKitStream(_id);
        _closed = true;
    }
}

void ffmpegkit::internal::FFmpegKitStreamOutput::ensureOpen() {
    if (_closed) {
        throw std::runtime_error("FFmpegKit output stream is closed.");
    }
}
