/*
 * Copyright (c) 2022, 2026 Taner Sener
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

#ifndef FFMPEG_KIT_FFMPEG_SESSION_COMPLETE_CALLBACK_H
#define FFMPEG_KIT_FFMPEG_SESSION_COMPLETE_CALLBACK_H

#include <functional>
#include <iostream>
#include <memory>

namespace ffmpegkit {

class FFmpegSession;

/**
 * <p>Callback invoked when an asynchronous <code>FFmpeg</code> session ends
 * running. It runs on the asynchronous worker thread.
 */
typedef std::function<void(
    const std::shared_ptr<ffmpegkit::FFmpegSession> session)>
    FFmpegSessionCompleteCallback;

} // namespace ffmpegkit

// The trampoline that carries this callback across the ABI needs the complete
// session type, so it is defined at the end of FFmpegSession.h
#include "FFmpegSession.h"

#endif // FFMPEG_KIT_FFMPEG_SESSION_COMPLETE_CALLBACK_H
