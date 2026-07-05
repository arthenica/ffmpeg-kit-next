/*
 * Copyright (c) 2021 Taner Sener
 *
 * This file is part of FFmpegKit.
 *
 * FFmpegKit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKit.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AVUTIL_FILE_FFMPEG_KIT_PROTOCOLS_H
#define AVUTIL_FILE_FFMPEG_KIT_PROTOCOLS_H

typedef int (*saf_open_function)(int);

typedef int (*saf_close_function)(int);

typedef int (*ffkit_protocol_open_function)(int64_t, int, void **);

typedef int (*ffkit_protocol_read_function)(void *, unsigned char *, int);

typedef int (*ffkit_protocol_write_function)(void *, const unsigned char *, int);

typedef int64_t (*ffkit_protocol_seek_function)(void *, int64_t, int);

typedef int (*ffkit_protocol_close_function)(void *);

saf_open_function av_get_saf_open(void);

saf_close_function av_get_saf_close(void);

void av_set_saf_open(saf_open_function);

void av_set_saf_close(saf_close_function);

ffkit_protocol_open_function av_get_ffkitmem_open(void);

ffkit_protocol_read_function av_get_ffkitmem_read(void);

ffkit_protocol_write_function av_get_ffkitmem_write(void);

ffkit_protocol_seek_function av_get_ffkitmem_seek(void);

ffkit_protocol_close_function av_get_ffkitmem_close(void);

void av_set_ffkitmem_functions(ffkit_protocol_open_function,
                               ffkit_protocol_read_function,
                               ffkit_protocol_write_function,
                               ffkit_protocol_seek_function,
                               ffkit_protocol_close_function);

ffkit_protocol_open_function av_get_ffkitstream_open(void);

ffkit_protocol_read_function av_get_ffkitstream_read(void);

ffkit_protocol_write_function av_get_ffkitstream_write(void);

ffkit_protocol_seek_function av_get_ffkitstream_seek(void);

ffkit_protocol_close_function av_get_ffkitstream_close(void);

void av_set_ffkitstream_functions(ffkit_protocol_open_function,
                                  ffkit_protocol_read_function,
                                  ffkit_protocol_write_function,
                                  ffkit_protocol_seek_function,
                                  ffkit_protocol_close_function);

#endif /* AVUTIL_FILE_FFMPEG_KIT_PROTOCOLS_H */
