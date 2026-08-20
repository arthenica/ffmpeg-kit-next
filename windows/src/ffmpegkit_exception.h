/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
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

#ifndef FFMPEG_KIT_EXCEPTION_H
#define FFMPEG_KIT_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace ffmpegkit {

/**
 * <p>Thrown by the FFmpegKit C++ API when a library call fails.
 *
 * <p>Exceptions never cross the library's ABI. The library reports failures
 * through the flat C API's per thread error slot and this header-only facade
 * raises them again inside the caller's own translation unit, so the exception
 * is created, thrown and caught entirely by the caller's compiler and runtime.
 */
class Exception : public std::runtime_error {
  public:
    explicit Exception(const std::string &message)
        : std::runtime_error(message) {}
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_EXCEPTION_H
