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
 * You should have received a copy of FFmpegKitNext. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef FFMPEG_KIT_SESSION_DELETE_LISTENER_H
#define FFMPEG_KIT_SESSION_DELETE_LISTENER_H

namespace ffmpegkit {

/**
 * Listener notified when a session is deleted from the native session history.
 */
class SessionDeleteListener {
  public:
    virtual ~SessionDeleteListener() = default;

    /**
     * Called after the session identified by sessionId has been deleted from
     * session history.
     *
     * @param sessionId session identifier
     */
    virtual void sessionDeleted(const long sessionId) = 0;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_SESSION_DELETE_LISTENER_H
