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

#ifndef FFMPEG_KIT_MEDIA_INFORMATION_H
#define FFMPEG_KIT_MEDIA_INFORMATION_H

#include "Chapter.h"
#include "StreamInformation.h"
#include "ffmpegkit_facade.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace ffmpegkit {

class MediaInformation;

namespace detail {
inline std::shared_ptr<ffmpegkit::MediaInformation>
adoptMediaInformation(FFKMediaInformation *handle);
inline FFKMediaInformation *
mediaInformationHandle(const ffmpegkit::MediaInformation &mediaInformation);
}

/**
 * <p>Media information of a media file.
 */
class MediaInformation {
  public:
    static constexpr const char *KeyFormatProperties = "format";
    static constexpr const char *KeyFilename = "filename";
    static constexpr const char *KeyFormat = "format_name";
    static constexpr const char *KeyFormatLong = "format_long_name";
    static constexpr const char *KeyStartTime = "start_time";
    static constexpr const char *KeyDuration = "duration";
    static constexpr const char *KeySize = "size";
    static constexpr const char *KeyBitRate = "bit_rate";
    static constexpr const char *KeyTags = "tags";

    MediaInformation(
        std::shared_ptr<ffmpegkit::json::Value> mediaInformationValue,
        std::shared_ptr<
            std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>
            streams,
        std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::Chapter>>>
            chapters)
        : _handle(nullptr, ffk_media_information_free) {
        const std::string value =
            ffmpegkit::detail::fromJson(mediaInformationValue);

        std::vector<std::string> streamJson;
        if (streams != nullptr) {
            for (const auto &stream : *streams) {
                streamJson.push_back(stream == nullptr
                                         ? std::string("null")
                                         : ffmpegkit::detail::fromJson(
                                               stream->getAllProperties()));
            }
        }
        std::vector<std::string> chapterJson;
        if (chapters != nullptr) {
            for (const auto &chapter : *chapters) {
                chapterJson.push_back(chapter == nullptr
                                          ? std::string("null")
                                          : ffmpegkit::detail::fromJson(
                                                chapter->getAllProperties()));
            }
        }

        std::vector<const char *> streamPointers;
        for (const std::string &json : streamJson) {
            streamPointers.push_back(json.c_str());
        }
        std::vector<const char *> chapterPointers;
        for (const std::string &json : chapterJson) {
            chapterPointers.push_back(json.c_str());
        }

        FFKMediaInformation *handle = ffk_media_information_create(
            value.c_str(),
            streamPointers.empty() ? nullptr : streamPointers.data(),
            streamPointers.size(),
            chapterPointers.empty() ? nullptr : chapterPointers.data(),
            chapterPointers.size());
        ffmpegkit::detail::checkError();
        _handle.reset(handle, ffk_media_information_free);
    }

    std::shared_ptr<std::string> getFilename() {
        return getStringFormatProperty(KeyFilename);
    }

    std::shared_ptr<std::string> getFormat() {
        return getStringFormatProperty(KeyFormat);
    }

    std::shared_ptr<std::string> getLongFormat() {
        return getStringFormatProperty(KeyFormatLong);
    }

    std::shared_ptr<std::string> getDuration() {
        return getStringFormatProperty(KeyDuration);
    }

    std::shared_ptr<std::string> getStartTime() {
        return getStringFormatProperty(KeyStartTime);
    }

    std::shared_ptr<std::string> getSize() {
        return getStringFormatProperty(KeySize);
    }

    std::shared_ptr<std::string> getBitrate() {
        return getStringFormatProperty(KeyBitRate);
    }

    std::shared_ptr<ffmpegkit::json::Value> getTags() {
        return getFormatProperty(KeyTags);
    }

    std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>
    getStreams() {
        return ffmpegkit::detail::takeStreamInformationList(
            ffk_media_information_get_streams(_handle.get()));
    }

    std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::Chapter>>>
    getChapters() {
        return ffmpegkit::detail::takeChapterList(
            ffk_media_information_get_chapters(_handle.get()));
    }

    std::shared_ptr<std::string> getStringProperty(const char *key) {
        return ffmpegkit::detail::takeOptionalString(
            ffk_media_information_get_string_property(_handle.get(), key));
    }

    std::shared_ptr<int64_t> getNumberProperty(const char *key) {
        int64_t value = 0;
        if (ffk_media_information_get_number_property(_handle.get(), key,
                                                      &value) == 0) {
            return nullptr;
        }
        return std::make_shared<int64_t>(value);
    }

    std::shared_ptr<ffmpegkit::json::Value> getProperty(const char *key) {
        return ffmpegkit::detail::takeJson(
            ffk_media_information_get_property_json(_handle.get(), key));
    }

    std::shared_ptr<std::string> getStringFormatProperty(const char *key) {
        return ffmpegkit::detail::takeOptionalString(
            ffk_media_information_get_string_format_property(_handle.get(),
                                                             key));
    }

    std::shared_ptr<int64_t> getNumberFormatProperty(const char *key) {
        int64_t value = 0;
        if (ffk_media_information_get_number_format_property(
                _handle.get(), key, &value) == 0) {
            return nullptr;
        }
        return std::make_shared<int64_t>(value);
    }

    std::shared_ptr<ffmpegkit::json::Value> getFormatProperty(const char *key) {
        return ffmpegkit::detail::takeJson(
            ffk_media_information_get_format_property_json(_handle.get(), key));
    }

    std::shared_ptr<ffmpegkit::json::Value> getFormatProperties() {
        return ffmpegkit::detail::takeJson(
            ffk_media_information_get_format_properties_json(_handle.get()));
    }

    std::shared_ptr<ffmpegkit::json::Value> getAllProperties() {
        return ffmpegkit::detail::takeJson(
            ffk_media_information_get_all_properties_json(_handle.get()));
    }

  private:
    friend std::shared_ptr<ffmpegkit::MediaInformation>
    ffmpegkit::detail::adoptMediaInformation(FFKMediaInformation *);
    friend FFKMediaInformation *
    ffmpegkit::detail::mediaInformationHandle(const ffmpegkit::MediaInformation &);

    struct Adopt {};

    MediaInformation(FFKMediaInformation *handle, Adopt)
        : _handle(handle, ffk_media_information_free) {}

    std::shared_ptr<FFKMediaInformation> _handle;
};

namespace detail {

/** Wraps a library media information handle, taking ownership of it. */
inline std::shared_ptr<ffmpegkit::MediaInformation>
adoptMediaInformation(FFKMediaInformation *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return std::shared_ptr<ffmpegkit::MediaInformation>(
        new ffmpegkit::MediaInformation(handle,
                                        ffmpegkit::MediaInformation::Adopt()));
}

/** Borrows the library handle a media information object wraps. */
inline FFKMediaInformation *
mediaInformationHandle(const ffmpegkit::MediaInformation &mediaInformation) {
    return mediaInformation._handle.get();
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_MEDIA_INFORMATION_H
