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

#include "StreamInformation.h"


ffmpegkit::StreamInformation::StreamInformation(
    std::shared_ptr<ffmpegkit::json::Value> streamInformationValue)
    : _streamInformationValue{streamInformationValue} {}

std::shared_ptr<int64_t> ffmpegkit::StreamInformation::getIndex() {
    return getNumberProperty(KeyIndex);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getType() {
    return getStringProperty(KeyType);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getCodec() {
    return getStringProperty(KeyCodec);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getCodecLong() {
    return getStringProperty(KeyCodecLong);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getFormat() {
    return getStringProperty(KeyFormat);
}

std::shared_ptr<int64_t> ffmpegkit::StreamInformation::getWidth() {
    return getNumberProperty(KeyWidth);
}

std::shared_ptr<int64_t> ffmpegkit::StreamInformation::getHeight() {
    return getNumberProperty(KeyHeight);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getBitrate() {
    return getStringProperty(KeyBitRate);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getSampleRate() {
    return getStringProperty(KeySampleRate);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getSampleFormat() {
    return getStringProperty(KeySampleFormat);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getChannelLayout() {
    return getStringProperty(KeyChannelLayout);
}

std::shared_ptr<std::string>
ffmpegkit::StreamInformation::getSampleAspectRatio() {
    return getStringProperty(KeySampleAspectRatio);
}

std::shared_ptr<std::string>
ffmpegkit::StreamInformation::getDisplayAspectRatio() {
    return getStringProperty(KeyDisplayAspectRatio);
}

std::shared_ptr<std::string>
ffmpegkit::StreamInformation::getAverageFrameRate() {
    return getStringProperty(KeyAverageFrameRate);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getRealFrameRate() {
    return getStringProperty(KeyRealFrameRate);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getTimeBase() {
    return getStringProperty(KeyTimeBase);
}

std::shared_ptr<std::string> ffmpegkit::StreamInformation::getCodecTimeBase() {
    return getStringProperty(KeyCodecTimeBase);
}

std::shared_ptr<ffmpegkit::json::Value> ffmpegkit::StreamInformation::getTags() {
    return getProperty(KeyTags);
}

std::shared_ptr<std::string>
ffmpegkit::StreamInformation::getStringProperty(const char *key) {
    if (_streamInformationValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _streamInformationValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return property->getString();
}

std::shared_ptr<int64_t>
ffmpegkit::StreamInformation::getNumberProperty(const char *key) {
    if (_streamInformationValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _streamInformationValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return property->getInt();
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::StreamInformation::getProperty(const char *key) {
    if (_streamInformationValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _streamInformationValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::json::Value>(*property);
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::StreamInformation::getAllProperties() {
    if (_streamInformationValue == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::json::Value>(*_streamInformationValue);
}
