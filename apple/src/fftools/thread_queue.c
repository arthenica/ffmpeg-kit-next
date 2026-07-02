/*
 * Original FFmpeg source:
 * Derived from FFmpeg source file fftools/thread_queue.c.
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2026 Taner Sener
 *
 * This modified file is part of FFmpegKitNext.
 * It is derived from FFmpeg's fftools/thread_queue.c at tag n8.1.2.
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
 * - FFmpeg 8.1.2 changes migrated
 * - Files moved under apple/src/fftools using upstream filenames
 * - FFmpegKitNext integration updates preserved, including wrapper API,
 *   callbacks, cancellation and thread/session-local execution where applicable
 *
 * 06.2026
 * --------------------------------------------------------
 * - FFmpeg 7.1.5 changes migrated
 * - FFmpegKitNext integration updates preserved, including wrapper API,
 *   callbacks, cancellation and thread/session-local execution where applicable
 *
 * ffmpeg-kit changes by ARTHENICA LTD
 *
 * 07.2023
 * --------------------------------------------------------
 * - FFmpeg 6.0 changes migrated
 * - fftools header names updated
 */

#include <stdint.h>
#include <string.h>

#include "libavutil/avassert.h"
#include "libavutil/container_fifo.h"
#include "libavutil/error.h"
#include "libavutil/fifo.h"
#include "libavutil/frame.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"
#include "libavutil/thread.h"

#include "libavcodec/packet.h"

#include "thread_queue.h"

enum {
    FINISHED_SEND = (1 << 0),
    FINISHED_RECV = (1 << 1),
};

struct ThreadQueue {
    int             choked;
    int              *finished;
    unsigned int    nb_streams;

    enum ThreadQueueType type;

    AVContainerFifo *fifo;
    AVFifo          *fifo_stream_index;

    pthread_mutex_t lock;
    pthread_cond_t  cond;
};

void tq_free(ThreadQueue **ptq)
{
    ThreadQueue *tq = *ptq;

    if (!tq)
        return;

    av_container_fifo_free(&tq->fifo);
    av_fifo_freep2(&tq->fifo_stream_index);

    av_freep(&tq->finished);

    pthread_cond_destroy(&tq->cond);
    pthread_mutex_destroy(&tq->lock);

    av_freep(ptq);
}

ThreadQueue *tq_alloc(unsigned int nb_streams, size_t queue_size,
                      enum ThreadQueueType type)
{
    ThreadQueue *tq;
    int ret;

    tq = av_mallocz(sizeof(*tq));
    if (!tq)
        return NULL;

    ret = pthread_cond_init(&tq->cond, NULL);
    if (ret) {
        av_freep(&tq);
        return NULL;
    }

    ret = pthread_mutex_init(&tq->lock, NULL);
    if (ret) {
        pthread_cond_destroy(&tq->cond);
        av_freep(&tq);
        return NULL;
    }

    tq->finished = av_calloc(nb_streams, sizeof(*tq->finished));
    if (!tq->finished)
        goto fail;
    tq->nb_streams = nb_streams;

    tq->type = type;

    tq->fifo = (type == THREAD_QUEUE_FRAMES) ?
               av_container_fifo_alloc_avframe(0) : av_container_fifo_alloc_avpacket(0);
    if (!tq->fifo)
        goto fail;

    tq->fifo_stream_index = av_fifo_alloc2(queue_size, sizeof(unsigned), 0);
    if (!tq->fifo_stream_index)
        goto fail;

    return tq;
fail:
    tq_free(&tq);
    return NULL;
}

int tq_send(ThreadQueue *tq, unsigned int stream_idx, void *data)
{
    int *finished;
    int ret;

    av_assert0(stream_idx < tq->nb_streams);
    finished = &tq->finished[stream_idx];

    pthread_mutex_lock(&tq->lock);

    if (*finished & FINISHED_SEND) {
        ret = AVERROR(EINVAL);
        goto finish;
    }

    while (!(*finished & FINISHED_RECV) && !av_fifo_can_write(tq->fifo_stream_index))
        pthread_cond_wait(&tq->cond, &tq->lock);

    if (*finished & FINISHED_RECV) {
        ret = AVERROR_EOF;
        *finished |= FINISHED_SEND;
    } else {
        ret = av_fifo_write(tq->fifo_stream_index, &stream_idx, 1);
        if (ret < 0)
            goto finish;

        ret = av_container_fifo_write(tq->fifo, data, 0);
        if (ret < 0)
            goto finish;

        pthread_cond_broadcast(&tq->cond);
    }

finish:
    pthread_mutex_unlock(&tq->lock);

    return ret;
}

static int receive_locked(ThreadQueue *tq, int *stream_idx,
                          void *data)
{
    unsigned int nb_finished = 0;

    if (tq->choked)
        return AVERROR(EAGAIN);

    while (av_container_fifo_read(tq->fifo, data, 0) >= 0) {
        unsigned idx;
        int ret;

        ret = av_fifo_read(tq->fifo_stream_index, &idx, 1);
        av_assert0(ret >= 0);
        if (tq->finished[idx] & FINISHED_RECV) {
            (tq->type == THREAD_QUEUE_FRAMES) ?
            av_frame_unref(data) : av_packet_unref(data);
            continue;
        }

        *stream_idx = idx;
        return 0;
    }

    for (unsigned int i = 0; i < tq->nb_streams; i++) {
        if (!tq->finished[i])
            continue;

        /* return EOF to the consumer at most once for each stream */
        if (!(tq->finished[i] & FINISHED_RECV)) {
            tq->finished[i] |= FINISHED_RECV;
            *stream_idx   = i;
            return AVERROR_EOF;
        }

        nb_finished++;
    }

    return nb_finished == tq->nb_streams ? AVERROR_EOF : AVERROR(EAGAIN);
}

int tq_receive(ThreadQueue *tq, int *stream_idx, void *data)
{
    int ret;

    *stream_idx = -1;

    pthread_mutex_lock(&tq->lock);

    while (1) {
        size_t can_read = av_container_fifo_can_read(tq->fifo);

        ret = receive_locked(tq, stream_idx, data);

        // signal other threads if the fifo state changed
        if (can_read != av_container_fifo_can_read(tq->fifo))
            pthread_cond_broadcast(&tq->cond);

        if (ret == AVERROR(EAGAIN)) {
            pthread_cond_wait(&tq->cond, &tq->lock);
            continue;
        }

        break;
    }

    pthread_mutex_unlock(&tq->lock);

    return ret;
}

void tq_send_finish(ThreadQueue *tq, unsigned int stream_idx)
{
    av_assert0(stream_idx < tq->nb_streams);

    pthread_mutex_lock(&tq->lock);

    /* mark the stream as send-finished;
     * next time the consumer thread tries to read this stream it will get
     * an EOF and recv-finished flag will be set */
    tq->finished[stream_idx] |= FINISHED_SEND;
    tq->choked = 0;
    pthread_cond_broadcast(&tq->cond);

    pthread_mutex_unlock(&tq->lock);
}

void tq_receive_finish(ThreadQueue *tq, unsigned int stream_idx)
{
    av_assert0(stream_idx < tq->nb_streams);

    pthread_mutex_lock(&tq->lock);

    /* mark the stream as recv-finished;
     * next time the producer thread tries to send for this stream, it will
     * get an EOF and send-finished flag will be set */
    tq->finished[stream_idx] |= FINISHED_RECV;
    pthread_cond_broadcast(&tq->cond);

    pthread_mutex_unlock(&tq->lock);
}

void tq_choke(ThreadQueue *tq, int choked)
{
    pthread_mutex_lock(&tq->lock);

    int prev_choked = tq->choked;
    tq->choked = choked;
    if (choked != prev_choked)
        pthread_cond_broadcast(&tq->cond);

    pthread_mutex_unlock(&tq->lock);
}
