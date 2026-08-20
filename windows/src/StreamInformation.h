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

#ifndef FFMPEG_KIT_STREAM_INFORMATION_H
#define FFMPEG_KIT_STREAM_INFORMATION_H

#include "ffmpegkit_facade.h"
#include "json/Value.h"
#include <cstdint>
#include <memory>
#include <string>

namespace ffmpegkit {

class StreamInformation;

namespace detail {
inline std::shared_ptr<ffmpegkit::StreamInformation>
adoptStreamInformation(FFKStreamInformation *handle);
}

/**
 * <p>Stream information of a media file.
 */
class StreamInformation {
  public:
    static constexpr const char *KeyIndex = "index";
    static constexpr const char *KeyType = "codec_type";
    static constexpr const char *KeyCodec = "codec_name";
    static constexpr const char *KeyCodecLong = "codec_long_name";
    static constexpr const char *KeyFormat = "pix_fmt";
    static constexpr const char *KeyWidth = "width";
    static constexpr const char *KeyHeight = "height";
    static constexpr const char *KeyBitRate = "bit_rate";
    static constexpr const char *KeySampleRate = "sample_rate";
    static constexpr const char *KeySampleFormat = "sample_fmt";
    static constexpr const char *KeyChannelLayout = "channel_layout";
    static constexpr const char *KeySampleAspectRatio = "sample_aspect_ratio";
    static constexpr const char *KeyDisplayAspectRatio = "display_aspect_ratio";
    static constexpr const char *KeyAverageFrameRate = "avg_frame_rate";
    static constexpr const char *KeyRealFrameRate = "r_frame_rate";
    static constexpr const char *KeyTimeBase = "time_base";
    static constexpr const char *KeyCodecTimeBase = "codec_time_base";
    static constexpr const char *KeyTags = "tags";

    StreamInformation(
        std::shared_ptr<ffmpegkit::json::Value> streamInformationValue)
        : _handle(nullptr, ffk_stream_information_free) {
        const std::string json =
            ffmpegkit::detail::fromJson(streamInformationValue);
        FFKStreamInformation *handle =
            ffk_stream_information_create(json.c_str());
        ffmpegkit::detail::checkError();
        _handle.reset(handle, ffk_stream_information_free);
    }

    std::shared_ptr<int64_t> getIndex() { return getNumberProperty(KeyIndex); }

    std::shared_ptr<std::string> getType() { return getStringProperty(KeyType); }

    std::shared_ptr<std::string> getCodec() {
        return getStringProperty(KeyCodec);
    }

    std::shared_ptr<std::string> getCodecLong() {
        return getStringProperty(KeyCodecLong);
    }

    std::shared_ptr<std::string> getFormat() {
        return getStringProperty(KeyFormat);
    }

    std::shared_ptr<int64_t> getWidth() { return getNumberProperty(KeyWidth); }

    std::shared_ptr<int64_t> getHeight() { return getNumberProperty(KeyHeight); }

    std::shared_ptr<std::string> getBitrate() {
        return getStringProperty(KeyBitRate);
    }

    std::shared_ptr<std::string> getSampleRate() {
        return getStringProperty(KeySampleRate);
    }

    std::shared_ptr<std::string> getSampleFormat() {
        return getStringProperty(KeySampleFormat);
    }

    std::shared_ptr<std::string> getChannelLayout() {
        return getStringProperty(KeyChannelLayout);
    }

    std::shared_ptr<std::string> getSampleAspectRatio() {
        return getStringProperty(KeySampleAspectRatio);
    }

    std::shared_ptr<std::string> getDisplayAspectRatio() {
        return getStringProperty(KeyDisplayAspectRatio);
    }

    std::shared_ptr<std::string> getAverageFrameRate() {
        return getStringProperty(KeyAverageFrameRate);
    }

    std::shared_ptr<std::string> getRealFrameRate() {
        return getStringProperty(KeyRealFrameRate);
    }

    std::shared_ptr<std::string> getTimeBase() {
        return getStringProperty(KeyTimeBase);
    }

    std::shared_ptr<std::string> getCodecTimeBase() {
        return getStringProperty(KeyCodecTimeBase);
    }

    std::shared_ptr<ffmpegkit::json::Value> getTags() {
        return getProperty(KeyTags);
    }

    std::shared_ptr<std::string> getStringProperty(const char *key) {
        return ffmpegkit::detail::takeOptionalString(
            ffk_stream_information_get_string_property(_handle.get(), key));
    }

    std::shared_ptr<int64_t> getNumberProperty(const char *key) {
        int64_t value = 0;
        if (ffk_stream_information_get_number_property(_handle.get(), key,
                                                       &value) == 0) {
            return nullptr;
        }
        return std::make_shared<int64_t>(value);
    }

    std::shared_ptr<ffmpegkit::json::Value> getProperty(const char *key) {
        return ffmpegkit::detail::takeJson(
            ffk_stream_information_get_property_json(_handle.get(), key));
    }

    std::shared_ptr<ffmpegkit::json::Value> getAllProperties() {
        return ffmpegkit::detail::takeJson(
            ffk_stream_information_get_all_properties_json(_handle.get()));
    }

  private:
    friend std::shared_ptr<ffmpegkit::StreamInformation>
    ffmpegkit::detail::adoptStreamInformation(FFKStreamInformation *);

    struct Adopt {};

    StreamInformation(FFKStreamInformation *handle, Adopt)
        : _handle(handle, ffk_stream_information_free) {}

    std::shared_ptr<FFKStreamInformation> _handle;
};

namespace detail {

/** Wraps a library stream information handle, taking ownership of it. */
inline std::shared_ptr<ffmpegkit::StreamInformation>
adoptStreamInformation(FFKStreamInformation *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return std::shared_ptr<ffmpegkit::StreamInformation>(
        new ffmpegkit::StreamInformation(
            handle, ffmpegkit::StreamInformation::Adopt()));
}

/** Rebuilds a std::vector of streams from a library list and releases it. */
inline std::shared_ptr<
    std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>
takeStreamInformationList(FFKStreamInformationList *list) {
    auto result = std::make_shared<
        std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_stream_information_list_size(list);
    for (size_t index = 0; index < size; index++) {
        result->push_back(
            adoptStreamInformation(ffk_stream_information_list_get(list, index)));
    }
    ffk_stream_information_list_free(list);
    return result;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_STREAM_INFORMATION_H
