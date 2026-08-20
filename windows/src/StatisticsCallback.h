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

#ifndef FFMPEG_KIT_STATISTICS_CALLBACK_H
#define FFMPEG_KIT_STATISTICS_CALLBACK_H

#include "Statistics.h"
#include "ffmpegkit_facade.h"
#include <functional>
#include <iostream>
#include <memory>

namespace ffmpegkit {

/**
 * <p>Callback invoked when a statistics entry is received.
 */
typedef std::function<void(
    const std::shared_ptr<ffmpegkit::Statistics> statistics)>
    StatisticsCallback;

namespace detail {

/** @see logTrampoline */
inline void statisticsTrampoline(FFKStatistics *statistics, void *userData) {
    CallbackHolder<StatisticsCallback> *holder =
        static_cast<CallbackHolder<StatisticsCallback> *>(userData);
    if (holder == nullptr || !holder->function) {
        return;
    }
    try {
        holder->function(takeStatistics(statistics));
    } catch (...) {
    }
}

/** Returns the trampoline to register, or nullptr for an empty callback. */
inline ffk_statistics_cb
statisticsCallbackFunction(const StatisticsCallback &callback) {
    return callback ? &statisticsTrampoline : nullptr;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_STATISTICS_CALLBACK_H
