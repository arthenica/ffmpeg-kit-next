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

#include "Statistics.h"

ffmpegkit::internal::Statistics::Statistics(const long sessionId,
                                  const int videoFrameNumber,
                                  const float videoFps,
                                  const float videoQuality, const int64_t size,
                                  const double time, const double bitrate,
                                  const double speed)
    : _sessionId{sessionId}, _videoFrameNumber{videoFrameNumber},
      _videoFps{videoFps}, _videoQuality{videoQuality}, _size{size},
      _time{time}, _bitrate{bitrate}, _speed{speed} {}

long ffmpegkit::internal::Statistics::getSessionId() { return _sessionId; }

int ffmpegkit::internal::Statistics::getVideoFrameNumber() { return _videoFrameNumber; }

float ffmpegkit::internal::Statistics::getVideoFps() { return _videoFps; }

float ffmpegkit::internal::Statistics::getVideoQuality() { return _videoQuality; }

int64_t ffmpegkit::internal::Statistics::getSize() { return _size; }

double ffmpegkit::internal::Statistics::getTime() { return _time; }

double ffmpegkit::internal::Statistics::getBitrate() { return _bitrate; }

double ffmpegkit::internal::Statistics::getSpeed() { return _speed; }
