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

import 'dart:typed_data';

final UnsupportedError _unsupported =
    UnsupportedError('FFmpegKitVirtualFile is only available on web');

/// Web-only; every member throws [UnsupportedError] on this platform.
class FFmpegKitVirtualFile {
  FFmpegKitVirtualFile._();

  static FFmpegKitVirtualFile from(String path) => throw _unsupported;
  static Future<FFmpegKitVirtualFile> createTemp({String? extension}) =>
      throw _unsupported;
  String getPath() => throw _unsupported;
  Future<bool> exists() => throw _unsupported;
  Future<int> length() => throw _unsupported;
  Future<Uint8List> readAsBytes() => throw _unsupported;
  Future<FFmpegKitVirtualFile> writeAsBytes(Uint8List bytes) =>
      throw _unsupported;
  Future<Object> readAsBlob({String? type}) => throw _unsupported;
  Future<FFmpegKitVirtualFile> rename(String newPath) => throw _unsupported;
  Future<void> delete() => throw _unsupported;
}
