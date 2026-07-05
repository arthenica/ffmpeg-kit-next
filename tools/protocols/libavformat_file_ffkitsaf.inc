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

#include "libavutil/file.h"

static int64_t saf_seek(URLContext *h, int64_t pos, int whence)
{
    FileContext *c = h->priv_data;
    int64_t ret;

    if (whence == AVSEEK_SIZE) {
        struct stat st;
        ret = fstat(c->fd, &st);
        return ret < 0 ? AVERROR(errno) : (S_ISFIFO(st.st_mode) ? 0 : st.st_size);
    }

    ret = lseek(c->fd, pos, whence);

    return ret < 0 ? AVERROR(errno) : ret;
}

static int saf_open(URLContext *h, const char *filename, int flags)
{
    FileContext *c = h->priv_data;
    struct stat st;
    const char *id_start;
    const char *p;
    int saf_id = 0;
    saf_open_function custom_saf_open;

    if (!av_strstart(filename, "ffkitsaf:", &id_start)) {
        av_log(h, AV_LOG_ERROR, "Invalid ffkitsaf URL '%s': missing 'ffkitsaf:' prefix.\n", filename);
        return AVERROR(EINVAL);
    }

    if (!av_isdigit(*id_start)) {
        av_log(h, AV_LOG_ERROR, "Invalid ffkitsaf URL '%s': SAF id must start with a digit.\n", filename);
        return AVERROR(EINVAL);
    }

    for (p = id_start; av_isdigit(*p); p++) {
        int digit = *p - '0';

        if (saf_id > (INT_MAX - digit) / 10) {
            av_log(h, AV_LOG_ERROR, "Invalid ffkitsaf URL '%s': SAF id overflows int.\n", filename);
            return AVERROR(EINVAL);
        }

        saf_id = saf_id * 10 + digit;
    }

    if (*p && *p != '.') {
        av_log(h, AV_LOG_ERROR, "Invalid ffkitsaf URL '%s': unexpected characters after SAF id.\n", filename);
        return AVERROR(EINVAL);
    }

    custom_saf_open = av_get_saf_open();
    if (custom_saf_open == NULL) {
        av_log(h, AV_LOG_ERROR, "Cannot open ffkitsaf URL '%s': no SAF open callback registered.\n", filename);
        return AVERROR(ENOSYS);
    }

    c->fd = custom_saf_open(saf_id);
    if (c->fd <= 0) {
        av_log(h, AV_LOG_ERROR, "Failed to open SAF id %d: callback returned %d.\n", saf_id, c->fd);
        return AVERROR(EIO);
    }

    h->is_streamed = !fstat(c->fd, &st) && S_ISFIFO(st.st_mode);

    /* Buffer writes more than the default 32k to improve throughput especially
     * with networked file systems */
    if (!h->is_streamed && flags & AVIO_FLAG_WRITE)
        h->min_packet_size = h->max_packet_size = 262144;

    if (c->seekable >= 0)
        h->is_streamed = !c->seekable;

    return 0;
}

static int saf_check(URLContext *h, int mask)
{
    (void)h;
    (void)mask;

    return AVERROR(ENOSYS);
}

static int saf_delete(URLContext *h)
{
    (void)h;

    return AVERROR(ENOSYS);
}

static int saf_move(URLContext *h_src, URLContext *h_dst)
{
    (void)h_src;
    (void)h_dst;

    return AVERROR(ENOSYS);
}

static int saf_close(URLContext *h)
{
    FileContext *c = h->priv_data;

    /*
     * Return convention: 0 on success, negative AVERROR on failure (FFmpeg's
     * url_close contract).
     *
     * The SAF close callback uses a *boolean* convention instead: it returns
     * nonzero on a successful close and 0 on failure. It is therefore
     * translated here and MUST NOT be returned directly. A missing callback is
     * treated as a successful no-op. (The ffkitmem/ffkitstream close callbacks
     * use the opposite, FFmpeg-native convention and are passed through as-is;
     * do not unify the two.)
     */
    saf_close_function custom_saf_close = av_get_saf_close();
    if (custom_saf_close != NULL) {
        int rc = custom_saf_close(c->fd);
        return rc ? 0 : AVERROR(EIO);
    } else {
        return 0;
    }
}

static const AVClass saf_class = {
    .class_name = "ffkitsaf",
    .item_name  = av_default_item_name,
    .option     = file_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const URLProtocol ff_ffkitsaf_protocol = {
    .name                = "ffkitsaf",
    .url_open            = saf_open,
    .url_read            = file_read,
    .url_write           = file_write,
    .url_seek            = saf_seek,
    .url_close           = saf_close,
    .url_get_file_handle = file_get_handle,
    .url_check           = saf_check,
    .url_delete          = saf_delete,
    .url_move            = saf_move,
    .priv_data_size      = sizeof(FileContext),
    .priv_data_class     = &saf_class,
    .default_whitelist   = "ffkitsaf,crypto,data"
};
