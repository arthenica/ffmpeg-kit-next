#include "libavutil/file.h"

typedef struct FFKitProtocolContext {
    const AVClass *class;
    void *handle;
} FFKitProtocolContext;

static int ffkit_parse_protocol_id(const char *filename, const char *protocol,
                                   int64_t *id)
{
    const char *id_string = NULL;
    const char *p;
    int64_t parsed_id = 0;

    if (!av_strstart(filename, protocol, &id_string)) {
        return AVERROR(EINVAL);
    }

    if (!av_isdigit(*id_string)) {
        return AVERROR(EINVAL);
    }

    for (p = id_string; av_isdigit(*p); p++) {
        int digit = *p - '0';
        if (parsed_id > (INT64_MAX - digit) / 10) {
            return AVERROR(EINVAL);
        }
        parsed_id = parsed_id * 10 + digit;
    }

    if (*p && *p != '.') {
        return AVERROR(EINVAL);
    }

    *id = parsed_id;
    return 0;
}

static int ffkitmem_open(URLContext *h, const char *filename, int flags)
{
    FFKitProtocolContext *c = h->priv_data;
    int64_t id;
    int ret = ffkit_parse_protocol_id(filename, "ffkitmem:", &id);
    ffkit_protocol_open_function open_function = av_get_ffkitmem_open();

    if (ret < 0) {
        av_log(h, AV_LOG_ERROR, "Invalid ffkitmem URL '%s'.\n", filename);
        return ret;
    }

    if (open_function == NULL) {
        av_log(h, AV_LOG_ERROR, "Cannot open ffkitmem URL '%s': no open callback registered.\n", filename);
        return AVERROR(ENOSYS);
    }

    ret = open_function(id, flags, &c->handle);
    if (ret < 0) {
        c->handle = NULL;
        return ret;
    }

    h->is_streamed = 0;
    return 0;
}

static int ffkitmem_read(URLContext *h, unsigned char *buf, int size)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_read_function read_function = av_get_ffkitmem_read();

    return read_function == NULL ? AVERROR(ENOSYS)
                                 : read_function(c->handle, buf, size);
}

static int ffkitmem_write(URLContext *h, const unsigned char *buf, int size)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_write_function write_function = av_get_ffkitmem_write();

    return write_function == NULL ? AVERROR(ENOSYS)
                                  : write_function(c->handle, buf, size);
}

static int64_t ffkitmem_seek(URLContext *h, int64_t pos, int whence)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_seek_function seek_function = av_get_ffkitmem_seek();

    return seek_function == NULL ? AVERROR(ENOSYS)
                                 : seek_function(c->handle, pos, whence);
}

static int ffkitmem_close(URLContext *h)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_close_function close_function = av_get_ffkitmem_close();

    /*
     * Return convention: 0 on success, negative AVERROR on failure (FFmpeg's
     * url_close contract). The ffkitmem close callback already follows this
     * convention, so its value is passed through unchanged; a missing callback
     * is a successful no-op. (Unlike saf_close, whose boolean callback must be
     * translated -- do not unify the two.)
     */
    return close_function == NULL ? 0 : close_function(c->handle);
}

static int ffkitstream_open(URLContext *h, const char *filename, int flags)
{
    FFKitProtocolContext *c = h->priv_data;
    int64_t id;
    int ret = ffkit_parse_protocol_id(filename, "ffkitstream:", &id);
    ffkit_protocol_open_function open_function = av_get_ffkitstream_open();

    if (ret < 0) {
        av_log(h, AV_LOG_ERROR, "Invalid ffkitstream URL '%s'.\n", filename);
        return ret;
    }

    if (open_function == NULL) {
        av_log(h, AV_LOG_ERROR, "Cannot open ffkitstream URL '%s': no open callback registered.\n", filename);
        return AVERROR(ENOSYS);
    }

    ret = open_function(id, flags, &c->handle);
    if (ret < 0) {
        c->handle = NULL;
        return ret;
    }

    h->is_streamed = 1;
    return 0;
}

static int ffkitstream_read(URLContext *h, unsigned char *buf, int size)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_read_function read_function = av_get_ffkitstream_read();

    return read_function == NULL ? AVERROR(ENOSYS)
                                 : read_function(c->handle, buf, size);
}

static int ffkitstream_write(URLContext *h, const unsigned char *buf, int size)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_write_function write_function = av_get_ffkitstream_write();

    return write_function == NULL ? AVERROR(ENOSYS)
                                  : write_function(c->handle, buf, size);
}

static int64_t ffkitstream_seek(URLContext *h, int64_t pos, int whence)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_seek_function seek_function = av_get_ffkitstream_seek();

    return seek_function == NULL ? AVERROR(ESPIPE)
                                 : seek_function(c->handle, pos, whence);
}

static int ffkitstream_close(URLContext *h)
{
    FFKitProtocolContext *c = h->priv_data;
    ffkit_protocol_close_function close_function = av_get_ffkitstream_close();

    /*
     * Return convention: 0 on success, negative AVERROR on failure (FFmpeg's
     * url_close contract). The ffkitstream close callback already follows this
     * convention, so its value is passed through unchanged; a missing callback
     * is a successful no-op. (Unlike saf_close, whose boolean callback must be
     * translated -- do not unify the two.)
     */
    return close_function == NULL ? 0 : close_function(c->handle);
}

static const AVClass ffkitmem_class = {
    .class_name = "ffkitmem",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const AVClass ffkitstream_class = {
    .class_name = "ffkitstream",
    .item_name  = av_default_item_name,
    .version    = LIBAVUTIL_VERSION_INT,
};

const URLProtocol ff_ffkitmem_protocol = {
    .name              = "ffkitmem",
    .url_open          = ffkitmem_open,
    .url_read          = ffkitmem_read,
    .url_write         = ffkitmem_write,
    .url_seek          = ffkitmem_seek,
    .url_close         = ffkitmem_close,
    .priv_data_size    = sizeof(FFKitProtocolContext),
    .priv_data_class   = &ffkitmem_class,
    .default_whitelist = "ffkitmem,crypto,data"
};

const URLProtocol ff_ffkitstream_protocol = {
    .name              = "ffkitstream",
    .url_open          = ffkitstream_open,
    .url_read          = ffkitstream_read,
    .url_write         = ffkitstream_write,
    .url_seek          = ffkitstream_seek,
    .url_close         = ffkitstream_close,
    .priv_data_size    = sizeof(FFKitProtocolContext),
    .priv_data_class   = &ffkitstream_class,
    .default_whitelist = "ffkitstream,crypto,data"
};
