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

#include "MediaInformation.h"


namespace {

const ffmpegkit::json::Value *findMember(const ffmpegkit::json::Value *value,
                                        const char *key) {
    if (value == nullptr) {
        return nullptr;
    }
    return value->find(key);
}

const ffmpegkit::json::Value *
findFormatObject(const ffmpegkit::json::Value *value) {
    const ffmpegkit::json::Value *format =
        findMember(value, ffmpegkit::internal::MediaInformation::KeyFormatProperties);
    if (format == nullptr || !format->isObject()) {
        return nullptr;
    }
    return format;
}

} // namespace

ffmpegkit::internal::MediaInformation::MediaInformation(
    std::shared_ptr<ffmpegkit::json::Value> mediaInformationValue,
    std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::internal::StreamInformation>>>
        streams,
    std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::internal::Chapter>>> chapters)
    : _mediaInformationValue{mediaInformationValue}, _streams{streams},
      _chapters{chapters} {}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getFilename() {
    return getStringFormatProperty(KeyFilename);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getFormat() {
    return getStringFormatProperty(KeyFormat);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getLongFormat() {
    return getStringFormatProperty(KeyFormatLong);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getStartTime() {
    return getStringFormatProperty(KeyStartTime);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getDuration() {
    return getStringFormatProperty(KeyDuration);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getSize() {
    return getStringFormatProperty(KeySize);
}

std::shared_ptr<std::string> ffmpegkit::internal::MediaInformation::getBitrate() {
    return getStringFormatProperty(KeyBitRate);
}

std::shared_ptr<ffmpegkit::json::Value> ffmpegkit::internal::MediaInformation::getTags() {
    const ffmpegkit::json::Value *tags =
        findMember(findFormatObject(_mediaInformationValue.get()), KeyTags);
    if (tags != nullptr) {
        return std::make_shared<ffmpegkit::json::Value>(*tags);
    } else {
        return nullptr;
    }
}

std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::internal::StreamInformation>>>
ffmpegkit::internal::MediaInformation::getStreams() {
    return _streams;
}

std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::internal::Chapter>>>
ffmpegkit::internal::MediaInformation::getChapters() {
    return _chapters;
}

std::shared_ptr<std::string>
ffmpegkit::internal::MediaInformation::getStringProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(_mediaInformationValue.get(), key);
    if (property != nullptr) {
        return property->getString();
    } else {
        return nullptr;
    }
}

std::shared_ptr<int64_t>
ffmpegkit::internal::MediaInformation::getNumberProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(_mediaInformationValue.get(), key);
    if (property != nullptr) {
        return property->getInt();
    } else {
        return nullptr;
    }
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::internal::MediaInformation::getProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(_mediaInformationValue.get(), key);
    if (property != nullptr) {
        return std::make_shared<ffmpegkit::json::Value>(*property);
    } else {
        return nullptr;
    }
}

std::shared_ptr<std::string>
ffmpegkit::internal::MediaInformation::getStringFormatProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(findFormatObject(_mediaInformationValue.get()), key);
    if (property != nullptr) {
        return property->getString();
    } else {
        return nullptr;
    }
}

std::shared_ptr<int64_t>
ffmpegkit::internal::MediaInformation::getNumberFormatProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(findFormatObject(_mediaInformationValue.get()), key);
    if (property != nullptr) {
        return property->getInt();
    } else {
        return nullptr;
    }
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::internal::MediaInformation::getFormatProperty(const char *key) {
    const ffmpegkit::json::Value *property =
        findMember(findFormatObject(_mediaInformationValue.get()), key);
    if (property != nullptr) {
        return std::make_shared<ffmpegkit::json::Value>(*property);
    } else {
        return nullptr;
    }
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::internal::MediaInformation::getFormatProperties() {
    const ffmpegkit::json::Value *format =
        findFormatObject(_mediaInformationValue.get());
    if (format != nullptr) {
        return std::make_shared<ffmpegkit::json::Value>(*format);
    } else {
        return nullptr;
    }
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::internal::MediaInformation::getAllProperties() {
    if (_mediaInformationValue == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::json::Value>(*_mediaInformationValue);
}
