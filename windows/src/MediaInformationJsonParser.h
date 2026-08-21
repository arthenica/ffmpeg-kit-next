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

#ifndef FFMPEG_KIT_MEDIA_INFORMATION_PARSER_H
#define FFMPEG_KIT_MEDIA_INFORMATION_PARSER_H

#include "MediaInformation.h"
#include "ffmpegkit_facade.h"
#include <memory>

namespace ffmpegkit {

/**
 * <p>Parses <code>FFprobe</code>'s json output into a
 * <code>MediaInformation</code>.
 */
class MediaInformationJsonParser {
  public:
    /**
     * Extracts media information from the given FFprobe json output.
     *
     * @param ffprobeJsonOutput FFprobe json output
     * @return created media information or nullptr if a parsing error occurs
     */
    static std::shared_ptr<ffmpegkit::MediaInformation>
    from(const std::string &ffprobeJsonOutput) {
        return ffmpegkit::detail::adoptMediaInformation(
            ffk_media_information_parser_from(ffprobeJsonOutput.c_str()));
    }

    /**
     * Extracts media information from the given FFprobe json output.
     *
     * @param ffprobeJsonOutput FFprobe json output
     * @return created media information
     * @throws ffmpegkit::Exception if a parsing error occurs
     */
    static std::shared_ptr<ffmpegkit::MediaInformation>
    fromWithError(const std::string &ffprobeJsonOutput) {
        FFKMediaInformation *handle =
            ffk_media_information_parser_from_with_error(
                ffprobeJsonOutput.c_str());
        ffmpegkit::detail::checkError();
        return ffmpegkit::detail::adoptMediaInformation(handle);
    }
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_MEDIA_INFORMATION_PARSER_H
