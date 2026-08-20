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

#ifndef FFMPEG_KIT_STATISTICS_H
#define FFMPEG_KIT_STATISTICS_H

#include "ffmpegkit_facade.h"
#include <cstdint>
#include <memory>
#include <stdlib.h>

namespace ffmpegkit {

/**
 * <p>Statistics entry for an <code>FFmpeg</code> execution.
 *
 * <p>Statistics are values: they are read out of the library once and then
 * owned by the caller.
 */
class Statistics {
  public:
    Statistics(const long sessionId, const int videoFrameNumber,
               const float videoFps, const float videoQuality,
               const int64_t size, const double time, const double bitrate,
               const double speed)
        : _sessionId{sessionId}, _videoFrameNumber{videoFrameNumber},
          _videoFps{videoFps}, _videoQuality{videoQuality}, _size{size},
          _time{time}, _bitrate{bitrate}, _speed{speed} {}

    long getSessionId() { return _sessionId; }

    int getVideoFrameNumber() { return _videoFrameNumber; }

    float getVideoFps() { return _videoFps; }

    float getVideoQuality() { return _videoQuality; }

    int64_t getSize() { return _size; }

    double getTime() { return _time; }

    double getBitrate() { return _bitrate; }

    double getSpeed() { return _speed; }

  private:
    long _sessionId;
    int _videoFrameNumber;
    float _videoFps;
    float _videoQuality;
    int64_t _size;
    double _time;
    double _bitrate;
    double _speed;
};

namespace detail {

/** Copies a library statistics handle into a Statistics value. */
inline std::shared_ptr<ffmpegkit::Statistics>
takeStatistics(FFKStatistics *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::Statistics>(
        ffk_statistics_get_session_id(handle),
        ffk_statistics_get_video_frame_number(handle),
        ffk_statistics_get_video_fps(handle),
        ffk_statistics_get_video_quality(handle),
        ffk_statistics_get_size(handle), ffk_statistics_get_time(handle),
        ffk_statistics_get_bitrate(handle), ffk_statistics_get_speed(handle));
}

/** Creates a library statistics handle from a Statistics value. */
inline FFKStatistics *
makeStatisticsHandle(const std::shared_ptr<ffmpegkit::Statistics> &statistics) {
    if (statistics == nullptr) {
        return nullptr;
    }
    return ffk_statistics_create(
        statistics->getSessionId(), statistics->getVideoFrameNumber(),
        statistics->getVideoFps(), statistics->getVideoQuality(),
        statistics->getSize(), statistics->getTime(), statistics->getBitrate(),
        statistics->getSpeed());
}

/** Rebuilds a std::list of statistics from a library list and releases it. */
inline std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Statistics>>>
takeStatisticsList(FFKStatisticsList *list) {
    auto result =
        std::make_shared<std::list<std::shared_ptr<ffmpegkit::Statistics>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_statistics_list_size(list);
    for (size_t index = 0; index < size; index++) {
        FFKStatistics *entry = ffk_statistics_list_get(list, index);
        result->push_back(takeStatistics(entry));
        ffk_statistics_free(entry);
    }
    ffk_statistics_list_free(list);
    return result;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_STATISTICS_H
