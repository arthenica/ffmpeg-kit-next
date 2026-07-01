/*
 * Original FFmpeg source:
 * Derived from FFmpeg source file fftools/objpool.h.
 *
 * FFmpegKitNext modifications:
 * Copyright (c) 2022, 2026 Taner Sener
 *
 * This modified file is part of FFmpegKitNext.
 * It is derived from FFmpeg's fftools/objpool.h at tag n7.1.5.
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
 * ffmpeg-kit changes by ARTHENICA LTD
 *
 * 06.2026
 * --------------------------------------------------------
 * - FFmpeg 7.1.5 changes migrated
 * - FFmpegKitNext integration updates preserved, including wrapper API,
 *   callbacks, cancellation and thread/session-local execution where applicable
 *
 * 07.2023
 * --------------------------------------------------------
 * - FFmpeg 6.0 changes migrated
 */

#ifndef FFTOOLS_OBJPOOL_H
#define FFTOOLS_OBJPOOL_H

typedef struct ObjPool ObjPool;

typedef void* (*ObjPoolCBAlloc)(void);
typedef void  (*ObjPoolCBReset)(void *);
typedef void  (*ObjPoolCBFree)(void **);

void     objpool_free(ObjPool **op);
ObjPool *objpool_alloc(ObjPoolCBAlloc cb_alloc, ObjPoolCBReset cb_reset,
                       ObjPoolCBFree cb_free);
ObjPool *objpool_alloc_packets(void);
ObjPool *objpool_alloc_frames(void);

int  objpool_get(ObjPool *op, void **obj);
void objpool_release(ObjPool *op, void **obj);

#endif // FFTOOLS_OBJPOOL_H
