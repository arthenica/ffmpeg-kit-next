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

namespace ffmpegkit {

/**
 * Deep copies source into a value that owns its own storage.
 *
 * rapidjson::Value assignment moves its source and leaves it null, and a Value
 * holds no reference to the allocator that owns its storage. Copying into a
 * Document and upcasting keeps that allocator alive for as long as the returned
 * pointer lives, so the result stays valid after source is destroyed.
 *
 * Declared by each translation unit that needs it rather than published in a
 * header, to keep it out of the installed API.
 *
 * @param source value to copy; left unmodified
 * @return a self-contained copy of source
 */
std::shared_ptr<rapidjson::Value>
cloneJsonValue(const rapidjson::Value &source) {
    auto document = std::make_shared<rapidjson::Document>();
    document->CopyFrom(source, document->GetAllocator());
    return std::static_pointer_cast<rapidjson::Value>(document);
}

} // namespace ffmpegkit

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
                            ffmpegkit::cloneJsonValue(streamArray[i])));
                }
            }
        }

        if (document->HasMember(MediaInformationJsonParserKeyChapters)) {
            const rapidjson::Value &chapterArray =
                (*document.get())[MediaInformationJsonParserKeyChapters];
            if (chapterArray.IsArray()) {
                for (rapidjson::SizeType i = 0; i < chapterArray.Size(); i++) {
                    chapters->push_back(std::make_shared<ffmpegkit::Chapter>(
                        ffmpegkit::cloneJsonValue(chapterArray[i])));
                }
            }
        }

        return std::make_shared<ffmpegkit::MediaInformation>(
            std::static_pointer_cast<rapidjson::Value>(document), streams,
            chapters);
    }
}
