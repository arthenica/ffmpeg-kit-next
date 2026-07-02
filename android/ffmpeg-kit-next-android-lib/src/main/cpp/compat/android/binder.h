/*
 * Original FFmpeg source:
 * Copyright (c) 2025 Dmitrii Okunev
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2026 Taner Sener
 *
 * This file is part of FFmpegKitNext.
 * It is copied unmodified from FFmpeg's compat/android/binder.h at tag n8.1.2.
 *
 * The original FFmpeg source is licensed under the GNU Lesser General
 * Public License version 2.1 or later. FFmpegKitNext distributes this
 * file under the GNU Lesser General Public License version 3 or later,
 * as permitted by that original "or later" license.
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

#ifndef COMPAT_ANDROID_BINDER_H
#define COMPAT_ANDROID_BINDER_H

/**
 * Initialize Android Binder thread pool.
 */
void android_binder_threadpool_init_if_required(void);

#endif                          // COMPAT_ANDROID_BINDER_H
