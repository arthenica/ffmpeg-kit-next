/*
 * Copyright (c) 2026 Taner Sener
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

#ifndef FFMPEG_KIT_PROTOCOL_URL_H
#define FFMPEG_KIT_PROTOCOL_URL_H

#include <algorithm>
#include <cctype>
#include <string>

namespace ffmpegkit {

static inline std::string normalizeFFmpegKitExtension(
    const std::string &extension) {
    std::string normalized = extension;
    normalized.erase(normalized.begin(),
                     std::find_if(normalized.begin(), normalized.end(),
                                  [](unsigned char c) {
                                      return !std::isspace(c);
                                  }));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(),
                                  [](unsigned char c) {
                                      return !std::isspace(c);
                                  }).base(),
                     normalized.end());
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    while (!normalized.empty() && normalized[0] == '.') {
        normalized.erase(0, 1);
    }

    std::string safeExtension;
    for (char c : normalized) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            safeExtension.push_back(c);
        }
    }

    return safeExtension.empty() ? "bin" : safeExtension;
}

static inline std::string buildFFmpegKitUrl(const std::string &protocol,
                                            const long id,
                                            const std::string &extension) {
    return protocol + ":" + std::to_string(id) + "." +
           normalizeFFmpegKitExtension(extension);
}

} // namespace ffmpegkit

#endif // FFMPEG_KIT_PROTOCOL_URL_H
