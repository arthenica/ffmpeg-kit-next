/*
 * Copyright (c) 2024 ARTHENICA LTD
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

/*
 * FFmpegKitNext changes:
 *
 * 06.2026
 * --------------------------------------------------------
 * - FFmpeg 7.1.5 migration updated this file for wrapper API,
 *   callbacks, cancellation and thread/session-local execution state.
 */

#include "ffmpeg_context.h"

/*
 * Per-session thread-local state propagation.
 * ---------------------------------------------------------------------------
 * FFmpegKit isolates concurrent ffmpeg/ffprobe invocations by storing fftools'
 * former globals in thread-local (__thread) storage, one independent copy per
 * session thread. Starting with the FFmpeg 7.x scheduler, a single invocation
 * fans out into several worker threads (demuxer, decoder(s), filtergraph(s),
 * encoder(s), muxer) created in task_start() in fftools_ffmpeg_sched.c. A newly
 * created pthread gets ZERO-INITIALIZED thread-local storage, NOT a copy of the
 * parent's, so without intervention every worker would see globalSessionId == 0,
 * options == NULL, etc.
 *
 * saveFFmpegContext() snapshots the session thread's thread-local globals into a
 * heap struct just before a worker thread is spawned; loadFFmpegContext() copies
 * that snapshot into the new worker thread's own thread-local storage. This is
 * what keeps each session's logging (routed by the thread-local globalSessionId),
 * cancellation and configuration correct inside the scheduler's worker threads.
 *
 * INVARIANT — keep these three in sync for every such variable:
 *   1. declared as __thread (in fftools_*.c / fftools_*.h),
 *   2. copied into the struct in saveFFmpegContext() below, and
 *   3. restored from the struct in loadFFmpegContext() below (in the same order).
 *
 * When upgrading FFmpeg: any NEW fftools global that becomes __thread AND is read
 * by code that can run after transcoding starts (i.e. on a worker thread) MUST be
 * added to BOTH functions below. A field that is saved but not loaded (or loaded
 * but not saved) makes worker threads silently run with a zero/garbage value — a
 * bug that only surfaces under the scheduler's multithreading and is hard to
 * trace. Read-only-after-parse config still counts: workers read it.
 */

FFmpegContext *saveFFmpegContext(void *arg) {
    FFmpegContext *context = (FFmpegContext *)av_mallocz(sizeof(FFmpegContext));
    if (!context)
        return NULL;

    // cmdutils.c
    context->program_name = program_name;
    context->program_birth_year = program_birth_year;
    context->sws_dict = sws_dict;
    context->swr_opts = swr_opts;
    context->format_opts = format_opts;
    context->codec_opts = codec_opts;
    context->hide_banner = hide_banner;
#if HAVE_COMMANDLINETOARGVW && defined(_WIN32)
    /* Will be leaked on exit */
    context->win32_argv_utf8 = win32_argv_utf8;
    context->win32_argc = win32_argc;
#endif

    // ffmpeg.c
    context->vstats_file = vstats_file;
    context->nb_output_dumped_ref = nb_output_dumped_ref;
    context->current_time = current_time;
    context->progress_avio = progress_avio;
    context->input_files = input_files;
    context->nb_input_files = nb_input_files;
    context->output_files = output_files;
    context->nb_output_files = nb_output_files;
    context->filtergraphs = filtergraphs;
    context->nb_filtergraphs = nb_filtergraphs;
    context->decoders = decoders;
    context->nb_decoders = nb_decoders;
#if HAVE_TERMIOS_H
    /* init terminal so that we can grab keys */
    context->oldtty = oldtty;
    context->restore_tty = restore_tty;
#endif
    context->received_sigterm = received_sigterm;
    context->received_nb_signals = received_nb_signals;
    context->transcode_init_done = transcode_init_done;
    context->ffmpeg_exited = ffmpeg_exited;
    context->copy_ts_first_pts = copy_ts_first_pts;

    // ffmpeg_hw.c
    context->nb_hw_devices = nb_hw_devices;
    context->hw_devices = hw_devices;

    // ffmpeg_mux_init.c
    context->enc_stats_files = enc_stats_files;
    context->nb_enc_stats_files = nb_enc_stats_files;

    // ffmpeg_opt.c
    context->options = options;
    context->filter_hw_device = filter_hw_device;
    context->vstats_filename = vstats_filename;
    context->audio_drift_threshold = audio_drift_threshold;
    context->dts_delta_threshold = dts_delta_threshold;
    context->dts_error_threshold = dts_error_threshold;
    context->video_sync_method = video_sync_method;
    context->frame_drop_threshold = frame_drop_threshold;
    context->do_benchmark = do_benchmark;
    context->do_benchmark_all = do_benchmark_all;
    context->do_hex_dump = do_hex_dump;
    context->do_pkt_dump = do_pkt_dump;
    context->copy_ts = copy_ts;
    context->start_at_zero = start_at_zero;
    context->copy_tb = copy_tb;
    context->debug_ts = debug_ts;
    context->exit_on_error = exit_on_error;
    context->abort_on_flags = abort_on_flags;
    context->print_stats = print_stats;
    context->stdin_interaction = stdin_interaction;
    context->max_error_rate = max_error_rate;
    context->filter_nbthreads = filter_nbthreads;
    context->filter_complex_nbthreads = filter_complex_nbthreads;
    context->vstats_version = vstats_version;
    context->auto_conversion_filters = auto_conversion_filters;
    context->stats_period = stats_period;
    context->file_overwrite = file_overwrite;
    context->no_file_overwrite = no_file_overwrite;
    context->ignore_unknown_streams = ignore_unknown_streams;
    context->copy_unknown_streams = copy_unknown_streams;
    context->recast_media = recast_media;

    // opt_common.c
    context->report_file = report_file;
    context->report_file_level = report_file_level;
    context->warned_cfg = warned_cfg;

    // FFmpegKit session context
    context->globalSessionId = globalSessionId;
    context->arg = arg;

    return context;
}

void *loadFFmpegContext(FFmpegContext *context) {

    // cmdutils.c
    program_name = context->program_name;
    program_birth_year = context->program_birth_year;
    sws_dict = context->sws_dict;
    swr_opts = context->swr_opts;
    format_opts = context->format_opts;
    codec_opts = context->codec_opts;
    hide_banner = context->hide_banner;
#if HAVE_COMMANDLINETOARGVW && defined(_WIN32)
    /* Will be leaked on exit */
    win32_argv_utf8 = context->win32_argv_utf8;
    win32_argc = context->win32_argc;
#endif

    // ffmpeg.c
    vstats_file = context->vstats_file;
    nb_output_dumped_ref = context->nb_output_dumped_ref;
    current_time = context->current_time;
    progress_avio = context->progress_avio;
    input_files = context->input_files;
    nb_input_files = context->nb_input_files;
    output_files = context->output_files;
    nb_output_files = context->nb_output_files;
    filtergraphs = context->filtergraphs;
    nb_filtergraphs = context->nb_filtergraphs;
    decoders = context->decoders;
    nb_decoders = context->nb_decoders;
#if HAVE_TERMIOS_H
    /* init terminal so that we can grab keys */
    oldtty = context->oldtty;
    restore_tty = context->restore_tty;
#endif
    received_sigterm = context->received_sigterm;
    received_nb_signals = context->received_nb_signals;
    transcode_init_done = context->transcode_init_done;
    ffmpeg_exited = context->ffmpeg_exited;
    copy_ts_first_pts = context->copy_ts_first_pts;

    // ffmpeg_hw.c
    nb_hw_devices = context->nb_hw_devices;
    hw_devices = context->hw_devices;

    // ffmpeg_mux_init.c
    enc_stats_files = context->enc_stats_files;
    nb_enc_stats_files = context->nb_enc_stats_files;

    // ffmpeg_opt.c
    options = context->options;
    filter_hw_device = context->filter_hw_device;
    vstats_filename = context->vstats_filename;
    audio_drift_threshold = context->audio_drift_threshold;
    dts_delta_threshold = context->dts_delta_threshold;
    dts_error_threshold = context->dts_error_threshold;
    video_sync_method = context->video_sync_method;
    frame_drop_threshold = context->frame_drop_threshold;
    do_benchmark = context->do_benchmark;
    do_benchmark_all = context->do_benchmark_all;
    do_hex_dump = context->do_hex_dump;
    do_pkt_dump = context->do_pkt_dump;
    copy_ts = context->copy_ts;
    start_at_zero = context->start_at_zero;
    copy_tb = context->copy_tb;
    debug_ts = context->debug_ts;
    exit_on_error = context->exit_on_error;
    abort_on_flags = context->abort_on_flags;
    print_stats = context->print_stats;
    stdin_interaction = context->stdin_interaction;
    max_error_rate = context->max_error_rate;
    filter_nbthreads = context->filter_nbthreads;
    filter_complex_nbthreads = context->filter_complex_nbthreads;
    vstats_version = context->vstats_version;
    auto_conversion_filters = context->auto_conversion_filters;
    stats_period = context->stats_period;
    file_overwrite = context->file_overwrite;
    no_file_overwrite = context->no_file_overwrite;
    ignore_unknown_streams = context->ignore_unknown_streams;
    copy_unknown_streams = context->copy_unknown_streams;
    recast_media = context->recast_media;

    // opt_common.c
    report_file = context->report_file;
    report_file_level = context->report_file_level;
    warned_cfg = context->warned_cfg;

    // FFmpegKit session context
    globalSessionId = context->globalSessionId;

    return context->arg;
}
