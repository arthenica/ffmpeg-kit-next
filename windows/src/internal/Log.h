/*
 * Copyright (c) 2022, 2026 Taner Sener
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

#ifndef FFMPEG_KIT_INTERNAL_FFMPEG_KIT_LOG_H
#define FFMPEG_KIT_INTERNAL_FFMPEG_KIT_LOG_H

#include "Level.h"
#include <string>

namespace ffmpegkit {
namespace internal {

/**
 * <p>Log entry for an <code>FFmpegKit</code> session.
 */
class Log {
  public:
    Log(const long sessionId, const ffmpegkit::internal::Level level,
        const char *message);
    long getSessionId() const;
    ffmpegkit::internal::Level getLevel() const;
    std::string getMessage() const;

  private:
    long _sessionId;
    ffmpegkit::internal::Level _level;
    std::string _message;
};

} // namespace internal
} // namespace ffmpegkit

#endif // FFMPEG_KIT_INTERNAL_FFMPEG_KIT_LOG_H
