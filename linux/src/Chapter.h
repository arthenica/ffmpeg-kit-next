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

#ifndef FFMPEG_KIT_CHAPTER_H
#define FFMPEG_KIT_CHAPTER_H

// OVERRIDING THE MACRO TO PREVENT APPLICATION TERMINATION. RAPIDJSON PRECONDITIONS
// ARE ASSERTIONS THAT abort() BY DEFAULT; THROWING INSTEAD KEEPS THEM CATCHABLE.
// RAPIDJSON_ASSERT_THROWS KEEPS THE noexcept CALL SITES ON assert(), WHERE THROWING
// WOULD CALL std::terminate. BOTH MUST BE DEFINED BEFORE rapidjson/document.h.
#include <stdexcept>
#define RAPIDJSON_ASSERT(x)                                                    \
    do {                                                                       \
        if (!(x))                                                              \
            throw std::logic_error("rapidjson: " #x);                          \
    } while (0)
#define RAPIDJSON_ASSERT_THROWS
#include "rapidjson/document.h"
#include <iostream>
#include <memory>
#include <string>

namespace ffmpegkit {

/**
 * Chapter class.
 */
class Chapter {
  public:
    static constexpr const char *KeyId = "id";
    static constexpr const char *KeyTimeBase = "time_base";
    static constexpr const char *KeyStart = "start";
    static constexpr const char *KeyStartTime = "start_time";
    static constexpr const char *KeyEnd = "end";
    static constexpr const char *KeyEndTime = "end_time";
    static constexpr const char *KeyTags = "tags";

    Chapter(std::shared_ptr<rapidjson::Value> chapterValue);

    std::shared_ptr<int64_t> getId();

    std::shared_ptr<std::string> getTimeBase();

    std::shared_ptr<int64_t> getStart();

    std::shared_ptr<std::string> getStartTime();

    std::shared_ptr<int64_t> getEnd();

    std::shared_ptr<std::string> getEndTime();

    std::shared_ptr<rapidjson::Value> getTags();

    /**
     * Returns the chapter property associated with the key.
     *
     * @return chapter property as string or nullptr if the key is not found
     */
    std::shared_ptr<std::string> getStringProperty(const char *key);

    /**
     * Returns the chapter property associated with the key.
     *
     * @return chapter property as number or nullptr if the key is not found
     */
    std::shared_ptr<int64_t> getNumberProperty(const char *key);

    /**
     * Returns the chapter property associated with the key.
     *
     * @return chapter property in a Value or nullptr if the key is not found
     */
    std::shared_ptr<rapidjson::Value> getProperty(const char *key);

    /**
     * Returns all chapter properties defined.
     *
     * @return all chapter properties in a Value or nullptr if no properties are
     * defined
     */
    std::shared_ptr<rapidjson::Value> getAllProperties();

  private:
    std::shared_ptr<rapidjson::Value> _chapterValue;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_CHAPTER_H
