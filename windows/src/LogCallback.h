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

#ifndef FFMPEG_KIT_LOG_CALLBACK_H
#define FFMPEG_KIT_LOG_CALLBACK_H

#include "Log.h"
#include "ffmpegkit_facade.h"
#include <functional>
#include <iostream>
#include <memory>

namespace ffmpegkit {

/**
 * <p>Callback invoked when a log entry is received.
 */
typedef std::function<void(const std::shared_ptr<ffmpegkit::Log> log)>
    LogCallback;

namespace detail {

/**
 * <p>C entry point the library calls back into. It converts the log handle
 * into a Log value and invokes the std::function kept on this side.
 *
 * <p>An exception must never unwind into the library, which was built with a
 * different compiler, so everything the callback raises is swallowed here.
 */
inline void logTrampoline(FFKLog *log, void *userData) {
    CallbackHolder<LogCallback> *holder =
        static_cast<CallbackHolder<LogCallback> *>(userData);
    if (holder == nullptr || !holder->function) {
        return;
    }
    try {
        holder->function(takeLog(log));
    } catch (...) {
    }
}

/** Returns the trampoline to register, or nullptr for an empty callback. */
inline ffk_log_cb logCallbackFunction(const LogCallback &callback) {
    return callback ? &logTrampoline : nullptr;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_LOG_CALLBACK_H
