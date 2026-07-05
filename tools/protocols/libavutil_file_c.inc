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

static saf_open_function _saf_open_function = NULL;
static saf_close_function _saf_close_function = NULL;
static ffkit_protocol_open_function _ffkitmem_open_function = NULL;
static ffkit_protocol_read_function _ffkitmem_read_function = NULL;
static ffkit_protocol_write_function _ffkitmem_write_function = NULL;
static ffkit_protocol_seek_function _ffkitmem_seek_function = NULL;
static ffkit_protocol_close_function _ffkitmem_close_function = NULL;
static ffkit_protocol_open_function _ffkitstream_open_function = NULL;
static ffkit_protocol_read_function _ffkitstream_read_function = NULL;
static ffkit_protocol_write_function _ffkitstream_write_function = NULL;
static ffkit_protocol_seek_function _ffkitstream_seek_function = NULL;
static ffkit_protocol_close_function _ffkitstream_close_function = NULL;

saf_open_function av_get_saf_open() {
    return _saf_open_function;
}

saf_close_function av_get_saf_close() {
    return _saf_close_function;
}

void av_set_saf_open(saf_open_function open_function) {
    _saf_open_function = open_function;
}

void av_set_saf_close(saf_close_function close_function) {
    _saf_close_function = close_function;
}

ffkit_protocol_open_function av_get_ffkitmem_open() {
    return _ffkitmem_open_function;
}

ffkit_protocol_read_function av_get_ffkitmem_read() {
    return _ffkitmem_read_function;
}

ffkit_protocol_write_function av_get_ffkitmem_write() {
    return _ffkitmem_write_function;
}

ffkit_protocol_seek_function av_get_ffkitmem_seek() {
    return _ffkitmem_seek_function;
}

ffkit_protocol_close_function av_get_ffkitmem_close() {
    return _ffkitmem_close_function;
}

void av_set_ffkitmem_functions(ffkit_protocol_open_function open_function,
                               ffkit_protocol_read_function read_function,
                               ffkit_protocol_write_function write_function,
                               ffkit_protocol_seek_function seek_function,
                               ffkit_protocol_close_function close_function) {
    _ffkitmem_open_function = open_function;
    _ffkitmem_read_function = read_function;
    _ffkitmem_write_function = write_function;
    _ffkitmem_seek_function = seek_function;
    _ffkitmem_close_function = close_function;
}

ffkit_protocol_open_function av_get_ffkitstream_open() {
    return _ffkitstream_open_function;
}

ffkit_protocol_read_function av_get_ffkitstream_read() {
    return _ffkitstream_read_function;
}

ffkit_protocol_write_function av_get_ffkitstream_write() {
    return _ffkitstream_write_function;
}

ffkit_protocol_seek_function av_get_ffkitstream_seek() {
    return _ffkitstream_seek_function;
}

ffkit_protocol_close_function av_get_ffkitstream_close() {
    return _ffkitstream_close_function;
}

void av_set_ffkitstream_functions(ffkit_protocol_open_function open_function,
                                  ffkit_protocol_read_function read_function,
                                  ffkit_protocol_write_function write_function,
                                  ffkit_protocol_seek_function seek_function,
                                  ffkit_protocol_close_function close_function) {
    _ffkitstream_open_function = open_function;
    _ffkitstream_read_function = read_function;
    _ffkitstream_write_function = write_function;
    _ffkitstream_seek_function = seek_function;
    _ffkitstream_close_function = close_function;
}
