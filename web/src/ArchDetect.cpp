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

#include "ArchDetect.h"

extern void *ffmpegKitInitialize();

const void *_archDetectInitializer{ffmpegKitInitialize()};

std::string ffmpegkit::ArchDetect::getArch() {
#ifdef FFMPEG_KIT_ARM64
    return "arm64";
#elif FFMPEG_KIT_WASM32
    return "wasm32";
#elif FFMPEG_KIT_I386
    return "i386";
#elif FFMPEG_KIT_X86_64
    return "x86_64";
#else
    return "";
#endif
}
