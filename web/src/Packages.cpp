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

#include "Packages.h"
#include "config.h"
#include <algorithm>
#include <memory>

std::string ffmpegkit::Packages::getPackageName() {
#ifndef FFMPEG_KIT_PACKAGE_NAME
#define FFMPEG_KIT_PACKAGE_NAME
#endif
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FFMPEG_KIT_PACKAGE_NAME_STR TOSTRING(FFMPEG_KIT_PACKAGE_NAME)

    return FFMPEG_KIT_PACKAGE_NAME_STR;
}

std::shared_ptr<std::set<std::string>>
ffmpegkit::Packages::getExternalLibraries() {
    const std::set<const char *> supportedExternalLibraries{
        "dav1d", "fontconfig", "freetype", "fribidi", "gmp", "gnutls",
        "harfbuzz", "kvazaar", "mp3lame", "libaom", "libass", "libjxl",
        "liblc3", "libsvtav1", "iconv", "libilbc", "libtheora", "libvidstab",
        "libvorbis", "libvpx", "libwebp", "libxml2", "opencore-amr",
        "openh264", "opus", "rubberband", "sdl2", "shine", "snappy", "soxr",
        "speex", "tesseract", "twolame", "vvenc", "x264", "x265", "xvid"
    };
    std::string buildConfiguration(FFMPEG_CONFIGURATION);
    char libraryName1[50];
    char libraryName2[50];
    std::shared_ptr<std::set<std::string>> enabledLibrarySet =
        std::make_shared<std::set<std::string>>();

    std::for_each(
        supportedExternalLibraries.cbegin(), supportedExternalLibraries.cend(),
        [&](const char *supportedExternalLibrary) {
            sprintf(libraryName1, "enable-%s", supportedExternalLibrary);
            sprintf(libraryName2, "enable-lib%s", supportedExternalLibrary);

            if (buildConfiguration.find(libraryName1) != std::string::npos ||
                buildConfiguration.find(libraryName2) != std::string::npos) {
                enabledLibrarySet->insert(supportedExternalLibrary);
            }
        });

    return enabledLibrarySet;
}
