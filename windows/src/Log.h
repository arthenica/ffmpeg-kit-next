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

#ifndef FFMPEG_KIT_LOG_H
#define FFMPEG_KIT_LOG_H

#include "Level.h"
#include "ffmpegkit_facade.h"
#include <memory>
#include <string>

namespace ffmpegkit {

/**
 * <p>Log entry for an <code>FFmpegKit</code> session.
 *
 * <p>A log is a value: it is read out of the library once and then owned by
 * the caller, so it stays valid after the session it came from is deleted.
 */
class Log {
  public:
    Log(const long sessionId, const ffmpegkit::Level level,
        const char *message)
        : _sessionId{sessionId}, _level{level}, _message{message} {}

    /**
     * Returns the session identifier.
     *
     * @return session identifier
     */
    long getSessionId() const { return _sessionId; }

    /**
     * Returns the log level.
     *
     * @return log level
     */
    ffmpegkit::Level getLevel() const { return _level; }

    /**
     * Returns the log message.
     *
     * @return log message
     */
    std::string getMessage() const { return _message; }

  private:
    long _sessionId;
    ffmpegkit::Level _level;
    std::string _message;
};

namespace detail {

/** Copies a library log handle into a Log value. */
inline std::shared_ptr<ffmpegkit::Log> takeLog(FFKLog *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    const std::string message = takeString(ffk_log_get_message(handle));
    return std::make_shared<ffmpegkit::Log>(
        ffk_log_get_session_id(handle),
        static_cast<ffmpegkit::Level>(ffk_log_get_level(handle)),
        message.c_str());
}

/** Rebuilds a std::list of logs from a library list and releases it. */
inline std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Log>>>
takeLogList(FFKLogList *list) {
    auto result =
        std::make_shared<std::list<std::shared_ptr<ffmpegkit::Log>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_log_list_size(list);
    for (size_t index = 0; index < size; index++) {
        FFKLog *entry = ffk_log_list_get(list, index);
        result->push_back(takeLog(entry));
        ffk_log_free(entry);
    }
    ffk_log_list_free(list);
    return result;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_LOG_H
