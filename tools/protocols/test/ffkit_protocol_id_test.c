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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define AVERROR(e) (-(e))
#define AVIO_FLAG_WRITE 2
#define AV_LOG_ERROR 16
#define AVSEEK_SIZE 0x10000
#define LIBAVUTIL_VERSION_INT 0
#define av_isdigit(c) ((unsigned)((unsigned char)(c) - '0') < 10)

typedef struct AVOption {
    const char *name;
} AVOption;

typedef struct AVClass {
    const char *class_name;
    const char *(*item_name)(void *ctx);
    const AVOption *option;
    int version;
} AVClass;

typedef struct URLContext {
    void *priv_data;
    int is_streamed;
    int min_packet_size;
    int max_packet_size;
} URLContext;

typedef struct URLProtocol {
    const char *name;
    int (*url_open)(URLContext *h, const char *filename, int flags);
    int (*url_read)(URLContext *h, unsigned char *buf, int size);
    int (*url_write)(URLContext *h, const unsigned char *buf, int size);
    int64_t (*url_seek)(URLContext *h, int64_t pos, int whence);
    int (*url_close)(URLContext *h);
    int (*url_get_file_handle)(URLContext *h);
    int (*url_check)(URLContext *h, int mask);
    int (*url_delete)(URLContext *h);
    int (*url_move)(URLContext *h_src, URLContext *h_dst);
    int priv_data_size;
    const AVClass *priv_data_class;
    const char *default_whitelist;
} URLProtocol;

#ifndef TEST_FFKIT_SAF
#define TEST_FFKIT_SAF 0
#endif

static const char *av_default_item_name(void *ctx)
{
    (void)ctx;
    return "ffkit-protocol-test";
}

static int av_strstart(const char *str, const char *pfx, const char **ptr)
{
    size_t len = strlen(pfx);

    if (strncmp(str, pfx, len) == 0) {
        *ptr = str + len;
        return 1;
    }

    *ptr = NULL;
    return 0;
}

static void av_log(void *avcl, int level, const char *fmt, ...)
{
    (void)avcl;
    (void)level;
    (void)fmt;
}

#if TEST_FFKIT_SAF
typedef struct FileContext {
    const AVClass *class;
    int fd;
    int trunc;
    int blocksize;
    int follow;
    int seekable;
} FileContext;

static const AVOption file_options[] = {
    { NULL }
};

static int file_read(URLContext *h, unsigned char *buf, int size)
{
    (void)h;
    (void)buf;
    (void)size;
    return AVERROR(ENOSYS);
}

static int file_write(URLContext *h, const unsigned char *buf, int size)
{
    (void)h;
    (void)buf;
    (void)size;
    return AVERROR(ENOSYS);
}

static int file_get_handle(URLContext *h)
{
    FileContext *c = h->priv_data;
    return c->fd;
}

#include "../libavformat_file_ffkitsaf.inc"
#endif

#include "../libavformat_file_ffkitmem_stream.c"

#if TEST_FFKIT_SAF
static saf_open_function test_saf_open_function;
static saf_close_function test_saf_close_function;
static int saf_source_fd = -1;
static int saf_open_call_count;
static int last_saf_open_id;
static int last_saf_close_fd;

saf_open_function av_get_saf_open(void)
{
    return test_saf_open_function;
}

saf_close_function av_get_saf_close(void)
{
    return test_saf_close_function;
}

void av_set_saf_open(saf_open_function open_function)
{
    test_saf_open_function = open_function;
}

void av_set_saf_close(saf_close_function close_function)
{
    test_saf_close_function = close_function;
}
#endif

static ffkit_protocol_open_function test_mem_open_function;
static ffkit_protocol_read_function test_mem_read_function;
static ffkit_protocol_write_function test_mem_write_function;
static ffkit_protocol_seek_function test_mem_seek_function;
static ffkit_protocol_close_function test_mem_close_function;

static ffkit_protocol_open_function test_stream_open_function;
static ffkit_protocol_read_function test_stream_read_function;
static ffkit_protocol_write_function test_stream_write_function;
static ffkit_protocol_seek_function test_stream_seek_function;
static ffkit_protocol_close_function test_stream_close_function;

static int open_call_count;
static int64_t last_open_id;
static int last_open_flags;
static char dummy_handle;

ffkit_protocol_open_function av_get_ffkitmem_open(void)
{
    return test_mem_open_function;
}

ffkit_protocol_read_function av_get_ffkitmem_read(void)
{
    return test_mem_read_function;
}

ffkit_protocol_write_function av_get_ffkitmem_write(void)
{
    return test_mem_write_function;
}

ffkit_protocol_seek_function av_get_ffkitmem_seek(void)
{
    return test_mem_seek_function;
}

ffkit_protocol_close_function av_get_ffkitmem_close(void)
{
    return test_mem_close_function;
}

void av_set_ffkitmem_functions(ffkit_protocol_open_function open_function,
                               ffkit_protocol_read_function read_function,
                               ffkit_protocol_write_function write_function,
                               ffkit_protocol_seek_function seek_function,
                               ffkit_protocol_close_function close_function)
{
    test_mem_open_function = open_function;
    test_mem_read_function = read_function;
    test_mem_write_function = write_function;
    test_mem_seek_function = seek_function;
    test_mem_close_function = close_function;
}

ffkit_protocol_open_function av_get_ffkitstream_open(void)
{
    return test_stream_open_function;
}

ffkit_protocol_read_function av_get_ffkitstream_read(void)
{
    return test_stream_read_function;
}

ffkit_protocol_write_function av_get_ffkitstream_write(void)
{
    return test_stream_write_function;
}

ffkit_protocol_seek_function av_get_ffkitstream_seek(void)
{
    return test_stream_seek_function;
}

ffkit_protocol_close_function av_get_ffkitstream_close(void)
{
    return test_stream_close_function;
}

void av_set_ffkitstream_functions(ffkit_protocol_open_function open_function,
                                  ffkit_protocol_read_function read_function,
                                  ffkit_protocol_write_function write_function,
                                  ffkit_protocol_seek_function seek_function,
                                  ffkit_protocol_close_function close_function)
{
    test_stream_open_function = open_function;
    test_stream_read_function = read_function;
    test_stream_write_function = write_function;
    test_stream_seek_function = seek_function;
    test_stream_close_function = close_function;
}

static int test_open(int64_t id, int flags, void **handle)
{
    open_call_count++;
    last_open_id = id;
    last_open_flags = flags;
    *handle = &dummy_handle;
    return 0;
}

#if TEST_FFKIT_SAF
static int test_saf_open(int id)
{
    int fd;

    saf_open_call_count++;
    last_saf_open_id = id;

    fd = dup(saf_source_fd);
    if (fd == 0) {
        int replacement_fd = fcntl(fd, F_DUPFD, 3);
        close(fd);
        fd = replacement_fd;
    }

    return fd;
}

static int test_saf_open_invalid_fd(int id)
{
    saf_open_call_count++;
    last_saf_open_id = id;
    return 0;
}

static int test_saf_close(int fd)
{
    last_saf_close_fd = fd;
    close(fd);
    return 1;
}

static void reset_saf_open_state(void)
{
    saf_open_call_count = 0;
    last_saf_open_id = -1;
    last_saf_close_fd = -1;
}
#endif

static void reset_open_state(void)
{
    open_call_count = 0;
    last_open_id = -1;
    last_open_flags = -1;
}

static int expect_parse(const char *protocol, const char *url,
                        int expected_ret, int64_t expected_id)
{
    int64_t id = -1;
    int ret = ffkit_parse_protocol_id(url, protocol, &id);

    if (ret != expected_ret) {
        fprintf(stderr, "parse %s: expected ret %d, got %d\n",
                url, expected_ret, ret);
        return 1;
    }

    if (ret == 0 && id != expected_id) {
        fprintf(stderr, "parse %s: expected id %lld, got %lld\n",
                url, (long long)expected_id, (long long)id);
        return 1;
    }

    return 0;
}

static int expect_open(const URLProtocol *protocol, const char *url,
                       int expected_ret, int expected_streamed,
                       int expected_open_calls, int64_t expected_id)
{
    FFKitProtocolContext priv_data = { 0 };
    URLContext ctx = { 0 };
    int ret;

    reset_open_state();
    ctx.priv_data = &priv_data;
    ret = protocol->url_open(&ctx, url, 7);

    if (ret != expected_ret) {
        fprintf(stderr, "open %s: expected ret %d, got %d\n",
                url, expected_ret, ret);
        return 1;
    }

    if (open_call_count != expected_open_calls) {
        fprintf(stderr, "open %s: expected %d callback calls, got %d\n",
                url, expected_open_calls, open_call_count);
        return 1;
    }

    if (expected_open_calls > 0) {
        if (last_open_id != expected_id) {
            fprintf(stderr, "open %s: expected callback id %lld, got %lld\n",
                    url, (long long)expected_id, (long long)last_open_id);
            return 1;
        }

        if (last_open_flags != 7) {
            fprintf(stderr, "open %s: expected callback flags 7, got %d\n",
                    url, last_open_flags);
            return 1;
        }

        if (priv_data.handle != &dummy_handle) {
            fprintf(stderr, "open %s: callback handle was not stored\n", url);
            return 1;
        }

        if (ctx.is_streamed != expected_streamed) {
            fprintf(stderr, "open %s: expected is_streamed %d, got %d\n",
                    url, expected_streamed, ctx.is_streamed);
            return 1;
        }
    }

    return 0;
}

#if TEST_FFKIT_SAF
static int expect_saf_open(const char *url, int expected_ret,
                           int expected_open_calls, int expected_id)
{
    FileContext priv_data = { 0 };
    URLContext ctx = { 0 };
    int ret;

    reset_saf_open_state();
    priv_data.fd = -1;
    priv_data.seekable = -1;
    ctx.priv_data = &priv_data;

    ret = ff_ffkitsaf_protocol.url_open(&ctx, url, 7);

    if (ret != expected_ret) {
        fprintf(stderr, "saf open %s: expected ret %d, got %d\n",
                url, expected_ret, ret);
        return 1;
    }

    if (saf_open_call_count != expected_open_calls) {
        fprintf(stderr, "saf open %s: expected %d callback calls, got %d\n",
                url, expected_open_calls, saf_open_call_count);
        return 1;
    }

    if (expected_open_calls > 0 && last_saf_open_id != expected_id) {
        fprintf(stderr, "saf open %s: expected callback id %d, got %d\n",
                url, expected_id, last_saf_open_id);
        return 1;
    }

    if (ret == 0) {
        int close_ret;

        if (priv_data.fd <= 0) {
            fprintf(stderr, "saf open %s: expected positive fd, got %d\n",
                    url, priv_data.fd);
            return 1;
        }

        if (ctx.is_streamed != 0) {
            fprintf(stderr, "saf open %s: expected is_streamed 0, got %d\n",
                    url, ctx.is_streamed);
            return 1;
        }

        close_ret = ff_ffkitsaf_protocol.url_close(&ctx);
        if (close_ret != 0) {
            fprintf(stderr, "saf close %s: expected ret 0, got %d\n",
                    url, close_ret);
            return 1;
        }

        if (last_saf_close_fd != priv_data.fd) {
            fprintf(stderr, "saf close %s: expected fd %d, got %d\n",
                    url, priv_data.fd, last_saf_close_fd);
            return 1;
        }
    }

    return 0;
}

static int expect_saf_unsupported_operations(void)
{
    URLContext ctx = { 0 };
    int failures = 0;

    if (ff_ffkitsaf_protocol.url_check(&ctx, 0) != AVERROR(ENOSYS)) {
        fprintf(stderr, "saf check: expected ENOSYS\n");
        failures++;
    }

    if (ff_ffkitsaf_protocol.url_delete(&ctx) != AVERROR(ENOSYS)) {
        fprintf(stderr, "saf delete: expected ENOSYS\n");
        failures++;
    }

    if (ff_ffkitsaf_protocol.url_move(&ctx, &ctx) != AVERROR(ENOSYS)) {
        fprintf(stderr, "saf move: expected ENOSYS\n");
        failures++;
    }

    return failures;
}
#endif

int main(void)
{
    int failures = 0;

    av_set_ffkitmem_functions(test_open, NULL, NULL, NULL, NULL);
    av_set_ffkitstream_functions(test_open, NULL, NULL, NULL, NULL);

    failures += expect_parse("ffkitmem:", "ffkitmem:", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:+1", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:-1", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem: 1", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:abc", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:123abc", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:123/path", AVERROR(EINVAL), -1);
    failures += expect_parse("ffkitmem:", "ffkitmem:9223372036854775808", AVERROR(EINVAL), -1);

    failures += expect_parse("ffkitmem:", "ffkitmem:0", 0, 0);
    failures += expect_parse("ffkitmem:", "ffkitmem:123.mp4", 0, 123);
    failures += expect_parse("ffkitmem:", "ffkitmem:9223372036854775807", 0, INT64_MAX);
    failures += expect_parse("ffkitstream:", "ffkitstream:456.bin", 0, 456);

    failures += expect_open(&ff_ffkitmem_protocol, "ffkitmem:123.mp4", 0, 0, 1, 123);
    failures += expect_open(&ff_ffkitstream_protocol, "ffkitstream:456.bin", 0, 1, 1, 456);

    failures += expect_open(&ff_ffkitmem_protocol, "ffkitmem:+1", AVERROR(EINVAL), 0, 0, -1);
    failures += expect_open(&ff_ffkitmem_protocol, "ffkitmem:123abc", AVERROR(EINVAL), 0, 0, -1);
    failures += expect_open(&ff_ffkitstream_protocol, "ffkitstream:9223372036854775808",
                            AVERROR(EINVAL), 0, 0, -1);

#if TEST_FFKIT_SAF
    saf_source_fd = open("/dev/null", O_RDONLY);
    if (saf_source_fd < 0) {
        perror("open /dev/null");
        return 1;
    }

    av_set_saf_open(test_saf_open);
    av_set_saf_close(test_saf_close);

    failures += expect_saf_open("ffkitsaf:", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:+1", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:-1", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf: 1", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:abc", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:123abc", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:123/path", AVERROR(EINVAL), 0, -1);
    failures += expect_saf_open("ffkitsaf:2147483648", AVERROR(EINVAL), 0, -1);

    failures += expect_saf_open("ffkitsaf:0", 0, 1, 0);
    failures += expect_saf_open("ffkitsaf:123.mp4", 0, 1, 123);
    failures += expect_saf_open("ffkitsaf:2147483647", 0, 1, INT_MAX);

    av_set_saf_open(test_saf_open_invalid_fd);
    failures += expect_saf_open("ffkitsaf:1", AVERROR(EIO), 1, 1);

    av_set_saf_open(NULL);
    failures += expect_saf_open("ffkitsaf:1", AVERROR(ENOSYS), 0, -1);

    failures += expect_saf_unsupported_operations();
    close(saf_source_fd);
#endif

    av_set_ffkitmem_functions(NULL, NULL, NULL, NULL, NULL);
    failures += expect_open(&ff_ffkitmem_protocol, "ffkitmem:1", AVERROR(ENOSYS), 0, 0, -1);

    if (failures != 0) {
        fprintf(stderr, "%d ffkit protocol tests failed\n", failures);
        return 1;
    }

    printf("INFO: ffkit protocol parser/runtime tests passed\n\n");
    return 0;
}
