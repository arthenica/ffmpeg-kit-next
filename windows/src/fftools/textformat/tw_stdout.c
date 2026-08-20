/*
 * Original FFmpeg source:
 * Copyright (c) The FFmpeg developers
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2026 Taner Sener
 *
 * This modified file is part of FFmpegKitNext.
 * It is derived from FFmpeg's fftools/textformat/tw_stdout.c at tag n8.1.2.
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
 * ffmpeg-kit changes by Taner Sener
 *
 * 07.2026
 * --------------------------------------------------------
 * - FFmpeg 8.1.2 stdout writer changes migrated
 * - FFmpegKitNext stdout redirection through log callback path preserved
 */

#include <stdarg.h>
#include <string.h>

#include "avtextwriters.h"
#include "fftools/cmdutils.h"
#include "libavutil/opt.h"

/* STDOUT Writer */

# define WRITER_NAME "stdoutwriter"

typedef struct StdOutWriterContext {
    const AVClass *class;
} StdOutWriterContext;

static const char *stdoutwriter_get_name(void *ctx)
{
    return WRITER_NAME;
}

static const AVClass stdoutwriter_class = {
    .class_name = WRITER_NAME,
    .item_name = stdoutwriter_get_name,
};

static inline void stdout_w8(AVTextWriterContext *wctx, int b)
{
    av_log(NULL, AV_LOG_STDERR, "%c", b);
}

static inline void stdout_put_str(AVTextWriterContext *wctx, const char *str)
{
    av_log(NULL, AV_LOG_STDERR, "%s", str);
}

static inline void stdout_vprintf(AVTextWriterContext *wctx, const char *fmt, va_list vl)
{
    av_vlog(NULL, AV_LOG_STDERR, fmt, vl);
}


static const AVTextWriter avtextwriter_stdout = {
    .name                 = WRITER_NAME,
    .priv_size            = sizeof(StdOutWriterContext),
    .priv_class           = &stdoutwriter_class,
    .writer_put_str       = stdout_put_str,
    .writer_vprintf       = stdout_vprintf,
    .writer_w8            = stdout_w8
};

int avtextwriter_create_stdout(AVTextWriterContext **pwctx)
{
    int ret;

    ret = avtextwriter_context_open(pwctx, &avtextwriter_stdout);

    return ret;
}
