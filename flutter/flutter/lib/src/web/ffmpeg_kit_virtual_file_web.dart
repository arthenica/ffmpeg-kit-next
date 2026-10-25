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

import 'dart:js_interop';
import 'dart:typed_data';

import 'package:web/web.dart' as web;

import 'ffmpeg_kit_js.dart';
import 'ffmpeg_kit_web_errors.dart';
import 'ffmpeg_kit_web_loader.dart';

/// A file in FFmpegKit's virtual file storage, the storage that paths in
/// FFmpeg commands refer to on web. It lives in memory: private to FFmpegKit
/// and gone on page reload. Like dart:io's File, an object is only a path.
/// Failures throw a PlatformException whose code is the POSIX name (ENOENT,
/// EISDIR, ...).
class FFmpegKitVirtualFile {
  FFmpegKitVirtualFile._(this._path);

  final String _path;

  /// A reference to [path]. Does not create or check anything. Relative paths
  /// resolve from "/".
  static FFmpegKitVirtualFile from(String path) {
    if (path.isEmpty) throw ArgumentError.value(path, 'path', 'is empty');
    return FFmpegKitVirtualFile._(path);
  }

  /// Creates an empty file at a unique path chosen by the library under
  /// /ffmpegkit/tmp/, keeping [extension] (e.g. "/ffmpegkit/tmp/7.srt").
  static Future<FFmpegKitVirtualFile> createTemp({String? extension}) =>
      guardJs(() async {
        final module = await FFmpegKitWebLoader.module();
        final options = {'extension': extension ?? ''}.jsify() as JSObject;
        final created = await module.virtualFile.createTemp(options).toDart;
        return FFmpegKitVirtualFile._(created.getPath().toDart);
      });

  String getPath() => _path;

  Future<VirtualFileJs> _js() async =>
      (await FFmpegKitWebLoader.module()).virtualFile.from(_path.toJS);

  /// True only when a regular file exists at the path.
  Future<bool> exists() =>
      guardJs(() async => (await (await _js()).exists().toDart).toDart);

  /// The file's size in bytes.
  Future<int> length() =>
      guardJs(() async => (await (await _js()).length().toDart).toDartInt);

  /// Copies the file's contents out. For small data such as thumbnails; use
  /// [readAsBlob] for large outputs.
  Future<Uint8List> readAsBytes() =>
      guardJs(() async => (await (await _js()).readAsBytes().toDart).toDart);

  /// Creates or replaces the file with a copy of [bytes]. Fails with ENOENT
  /// when the parent folder is missing. The caller keeps its own list.
  Future<FFmpegKitVirtualFile> writeAsBytes(Uint8List bytes) =>
      guardJs(() async {
        await (await _js()).writeAsBytes(bytes.toJS).toDart;
        return this;
      });

  /// The contents as a Blob made inside the library worker and handed over
  /// without a copy, for downloads, sharing and uploads of large outputs.
  Future<web.Blob> readAsBlob({String? type}) => guardJs(() async {
        final options = {'type': type ?? ''}.jsify() as JSObject;
        return (await (await _js()).readAsBlob(options).toDart) as web.Blob;
      });

  /// Moves the file and returns an object for [newPath]; this one keeps the
  /// old path. An existing file at [newPath] is replaced.
  Future<FFmpegKitVirtualFile> rename(String newPath) {
    final target = FFmpegKitVirtualFile.from(newPath);
    return guardJs(() async {
      await (await _js()).rename(newPath.toJS).toDart;
      return target;
    });
  }

  Future<void> delete() =>
      guardJs(() async => (await _js()).delete().toDart);
}
