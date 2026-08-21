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

#include "Chapter.h"

ffmpegkit::internal::Chapter::Chapter(std::shared_ptr<ffmpegkit::json::Value> chapterValue)
    : _chapterValue{chapterValue} {}

std::shared_ptr<int64_t> ffmpegkit::internal::Chapter::getId() {
    return getNumberProperty(KeyId);
}

std::shared_ptr<std::string> ffmpegkit::internal::Chapter::getTimeBase() {
    return getStringProperty(KeyTimeBase);
}

std::shared_ptr<int64_t> ffmpegkit::internal::Chapter::getStart() {
    return getNumberProperty(KeyStart);
}

std::shared_ptr<std::string> ffmpegkit::internal::Chapter::getStartTime() {
    return getStringProperty(KeyStartTime);
}

std::shared_ptr<int64_t> ffmpegkit::internal::Chapter::getEnd() {
    return getNumberProperty(KeyEnd);
}

std::shared_ptr<std::string> ffmpegkit::internal::Chapter::getEndTime() {
    return getStringProperty(KeyEndTime);
}

std::shared_ptr<ffmpegkit::json::Value> ffmpegkit::internal::Chapter::getTags() {
    return getProperty(KeyTags);
}

std::shared_ptr<std::string>
ffmpegkit::internal::Chapter::getStringProperty(const char *key) {
    if (_chapterValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _chapterValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return property->getString();
}

std::shared_ptr<int64_t>
ffmpegkit::internal::Chapter::getNumberProperty(const char *key) {
    if (_chapterValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _chapterValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return property->getInt();
}

std::shared_ptr<ffmpegkit::json::Value>
ffmpegkit::internal::Chapter::getProperty(const char *key) {
    if (_chapterValue == nullptr) {
        return nullptr;
    }
    const ffmpegkit::json::Value *property = _chapterValue->find(key);
    if (property == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::json::Value>(*property);
}

std::shared_ptr<ffmpegkit::json::Value> ffmpegkit::internal::Chapter::getAllProperties() {
    if (_chapterValue == nullptr) {
        return nullptr;
    }
    return std::make_shared<ffmpegkit::json::Value>(*_chapterValue);
}
