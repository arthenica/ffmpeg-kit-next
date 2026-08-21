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

#include "ffmpegkit_facade.h"
#include "json/Value.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace ffmpegkit {

class Chapter;

namespace detail {
inline std::shared_ptr<ffmpegkit::Chapter> adoptChapter(FFKChapter *handle);
}

/**
 * <p>Chapter information of a media file.
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

    Chapter(std::shared_ptr<ffmpegkit::json::Value> chapterValue)
        : _handle(nullptr, ffk_chapter_free) {
        const std::string json = ffmpegkit::detail::fromJson(chapterValue);
        FFKChapter *handle = ffk_chapter_create(json.c_str());
        ffmpegkit::detail::checkError();
        _handle.reset(handle, ffk_chapter_free);
    }

    std::shared_ptr<int64_t> getId() { return getNumberProperty(KeyId); }

    std::shared_ptr<std::string> getTimeBase() {
        return getStringProperty(KeyTimeBase);
    }

    std::shared_ptr<int64_t> getStart() { return getNumberProperty(KeyStart); }

    std::shared_ptr<std::string> getStartTime() {
        return getStringProperty(KeyStartTime);
    }

    std::shared_ptr<int64_t> getEnd() { return getNumberProperty(KeyEnd); }

    std::shared_ptr<std::string> getEndTime() {
        return getStringProperty(KeyEndTime);
    }

    std::shared_ptr<ffmpegkit::json::Value> getTags() {
        return getProperty(KeyTags);
    }

    std::shared_ptr<std::string> getStringProperty(const char *key) {
        return ffmpegkit::detail::takeOptionalString(
            ffk_chapter_get_string_property(_handle.get(), key));
    }

    std::shared_ptr<int64_t> getNumberProperty(const char *key) {
        int64_t value = 0;
        if (ffk_chapter_get_number_property(_handle.get(), key, &value) == 0) {
            return nullptr;
        }
        return std::make_shared<int64_t>(value);
    }

    std::shared_ptr<ffmpegkit::json::Value> getProperty(const char *key) {
        return ffmpegkit::detail::takeJson(
            ffk_chapter_get_property_json(_handle.get(), key));
    }

    std::shared_ptr<ffmpegkit::json::Value> getAllProperties() {
        return ffmpegkit::detail::takeJson(
            ffk_chapter_get_all_properties_json(_handle.get()));
    }

  private:
    friend std::shared_ptr<ffmpegkit::Chapter>
    ffmpegkit::detail::adoptChapter(FFKChapter *);

    struct Adopt {};

    Chapter(FFKChapter *handle, Adopt) : _handle(handle, ffk_chapter_free) {}

    std::shared_ptr<FFKChapter> _handle;
};

namespace detail {

/** Wraps a library chapter handle, taking ownership of it. */
inline std::shared_ptr<ffmpegkit::Chapter> adoptChapter(FFKChapter *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return std::shared_ptr<ffmpegkit::Chapter>(
        new ffmpegkit::Chapter(handle, ffmpegkit::Chapter::Adopt()));
}

/** Rebuilds a std::vector of chapters from a library list and releases it. */
inline std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::Chapter>>>
takeChapterList(FFKChapterList *list) {
    auto result =
        std::make_shared<std::vector<std::shared_ptr<ffmpegkit::Chapter>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_chapter_list_size(list);
    for (size_t index = 0; index < size; index++) {
        result->push_back(adoptChapter(ffk_chapter_list_get(list, index)));
    }
    ffk_chapter_list_free(list);
    return result;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_CHAPTER_H
