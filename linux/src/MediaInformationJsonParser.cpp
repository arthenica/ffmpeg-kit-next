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

#include "MediaInformationJsonParser.h"
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
#include "rapidjson/error/en.h"
#include "rapidjson/reader.h"
#include <memory>

static const char *MediaInformationJsonParserKeyStreams = "streams";
static const char *MediaInformationJsonParserKeyChapters = "chapters";

namespace {

/**
 * Converts a parsed rapidjson value into its ffmpegkit::json::Value equivalent.
 *
 * This is the only place rapidjson types are read. Every accessor is guarded by
 * the matching type check, so rapidjson preconditions are never violated and the
 * result owns all of its data.
 *
 * @param source parsed value
 * @return an equivalent value, null when source has no representable type
 */
ffmpegkit::json::Value toJsonValue(const rapidjson::Value &source) {
    switch (source.GetType()) {
    case rapidjson::kFalseType:
        return ffmpegkit::json::Value(false);
    case rapidjson::kTrueType:
        return ffmpegkit::json::Value(true);
    case rapidjson::kStringType:
        return ffmpegkit::json::Value(
            std::string(source.GetString(), source.GetStringLength()));
    case rapidjson::kNumberType:
        if (source.IsInt64()) {
            return ffmpegkit::json::Value(source.GetInt64());
        } else if (source.IsUint64() &&
                   source.GetUint64() <=
                       static_cast<uint64_t>(INT64_MAX)) {
            return ffmpegkit::json::Value(
                static_cast<int64_t>(source.GetUint64()));
        } else {
            return ffmpegkit::json::Value(source.GetDouble());
        }
    case rapidjson::kObjectType: {
        auto object = ffmpegkit::json::Value::makeObject();
        for (auto member = source.MemberBegin(); member != source.MemberEnd();
             ++member) {
            object.set(std::string(member->name.GetString(),
                                   member->name.GetStringLength()),
                       toJsonValue(member->value));
        }
        return object;
    }
    case rapidjson::kArrayType: {
        auto array = ffmpegkit::json::Value::makeArray();
        for (auto element = source.Begin(); element != source.End();
             ++element) {
            array.append(toJsonValue(*element));
        }
        return array;
    }
    case rapidjson::kNullType:
    default:
        return ffmpegkit::json::Value();
    }
}

} // namespace

std::shared_ptr<ffmpegkit::MediaInformation>
ffmpegkit::MediaInformationJsonParser::from(
    const std::string &ffprobeJsonOutput) {
    try {
        return fromWithError(ffprobeJsonOutput);
    } catch (const std::exception &exception) {
        std::cout << "MediaInformation parsing failed: " << exception.what()
                  << std::endl;
        return nullptr;
    }
}

std::shared_ptr<ffmpegkit::MediaInformation>
ffmpegkit::MediaInformationJsonParser::fromWithError(
    const std::string &ffprobeJsonOutput) {
    std::shared_ptr<rapidjson::Document> document =
        std::make_shared<rapidjson::Document>();

    document->Parse(ffprobeJsonOutput.c_str());

    if (document->HasParseError()) {
        throw std::runtime_error(GetParseError_En(document->GetParseError()));
    } else {
        std::shared_ptr<
            std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>
            streams = std::make_shared<
                std::vector<std::shared_ptr<ffmpegkit::StreamInformation>>>();
        std::shared_ptr<std::vector<std::shared_ptr<ffmpegkit::Chapter>>>
            chapters = std::make_shared<
                std::vector<std::shared_ptr<ffmpegkit::Chapter>>>();

        if (document->HasMember(MediaInformationJsonParserKeyStreams)) {
            const rapidjson::Value &streamArray =
                (*document.get())[MediaInformationJsonParserKeyStreams];
            if (streamArray.IsArray()) {
                for (rapidjson::SizeType i = 0; i < streamArray.Size(); i++) {
                    streams->push_back(
                        std::make_shared<ffmpegkit::StreamInformation>(
                            std::make_shared<ffmpegkit::json::Value>(
                                toJsonValue(streamArray[i]))));
                }
            }
        }

        if (document->HasMember(MediaInformationJsonParserKeyChapters)) {
            const rapidjson::Value &chapterArray =
                (*document.get())[MediaInformationJsonParserKeyChapters];
            if (chapterArray.IsArray()) {
                for (rapidjson::SizeType i = 0; i < chapterArray.Size(); i++) {
                    chapters->push_back(std::make_shared<ffmpegkit::Chapter>(
                        std::make_shared<ffmpegkit::json::Value>(
                            toJsonValue(chapterArray[i]))));
                }
            }
        }

        return std::make_shared<ffmpegkit::MediaInformation>(
            std::make_shared<ffmpegkit::json::Value>(toJsonValue(*document)),
            streams, chapters);
    }
}
