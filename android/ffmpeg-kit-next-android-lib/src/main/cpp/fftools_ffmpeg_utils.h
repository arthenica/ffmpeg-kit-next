/*
 * Original FFmpeg source:
 * Derived from FFmpeg source file fftools/ffmpeg_utils.h.
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2026 Taner Sener
 *
 * This modified file is part of FFmpegKitNext.
 * It is derived from FFmpeg's fftools/ffmpeg_utils.h at tag n7.1.5.
 *
 * The original FFmpeg source is licensed under the GNU Lesser General
 * Public License version 2.1 or later. FFmpegKitNext distributes this
 * modified file under the GNU Lesser General Public License version 3 or
 * later, as permitted by that original "or later" license.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modification history:
 *
 * 06.2026
 * --------------------------------------------------------
 * - Added to FFmpegKitNext from FFmpeg n7.1.5.
 * - fftools_ wrapper integration and Apple build compatibility updates
 *   applied for the FFmpegKit API.
 */

#ifndef FFTOOLS_FFMPEG_UTILS_H
#define FFTOOLS_FFMPEG_UTILS_H

#include <stdint.h>

#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/rational.h"

#include "libavcodec/packet.h"

typedef struct Timestamp {
    int64_t    ts;
    AVRational tb;
} Timestamp;

/**
 * Merge two return codes - return one of the error codes if at least one of
 * them was negative, 0 otherwise.
 */
static inline int err_merge(int err0, int err1)
{
    // prefer "real" errors over EOF
    if ((err0 >= 0 || err0 == AVERROR_EOF) && err1 < 0)
        return err1;
    return (err0 < 0) ? err0 : FFMIN(err1, 0);
}

static inline void pkt_move(void *dst, void *src)
{
    av_packet_move_ref(dst, src);
}

static inline void frame_move(void *dst, void *src)
{
    av_frame_move_ref(dst, src);
}

#endif // FFTOOLS_FFMPEG_UTILS_H
