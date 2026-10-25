/*
 * Copyright (c) 2019-2022, 2026 Taner Sener
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

final UnsupportedError _unsupported =
    UnsupportedError('FFmpegKitLocalFile is only available on web');

/// Web-only; every member throws [UnsupportedError] on this platform.
class FFmpegKitLocalFile {
  FFmpegKitLocalFile._();

  static Future<FFmpegKitLocalFile> fromFile(Object file, {String? name}) =>
      throw _unsupported;
  String getPath() => throw _unsupported;
  Future<void> close() => throw _unsupported;
}
