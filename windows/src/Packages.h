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

#ifndef FFMPEG_KIT_PACKAGES_H
#define FFMPEG_KIT_PACKAGES_H

#include "ffmpegkit_facade.h"
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace ffmpegkit {

/**
 * <p>Helper class to extract binary package information.
 */
class Packages {
  public:
    /**
     * Returns the name of the package.
     *
     * @return guessed name of the package
     */
    static std::string getPackageName() {
        return ffmpegkit::detail::takeString(ffk_packages_get_package_name());
    }

    /**
     * Returns enabled external libraries by FFmpeg.
     *
     * @return enabled external libraries
     */
    static std::shared_ptr<std::set<std::string>> getExternalLibraries() {
        auto result = std::make_shared<std::set<std::string>>();
        FFKStringList *list = ffk_packages_get_external_libraries();
        if (list == nullptr) {
            return result;
        }
        const size_t size = ffk_string_list_size(list);
        for (size_t index = 0; index < size; index++) {
            result->insert(ffmpegkit::detail::takeString(
                ffk_string_list_get(list, index)));
        }
        ffk_string_list_free(list);
        return result;
    }
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_PACKAGES_H
