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

import 'ffmpeg_kit_js.dart';
import 'ffmpeg_kit_web_errors.dart';
import 'ffmpeg_kit_web_loader.dart';

/// A folder in FFmpegKit's virtual file storage, the storage that paths in
/// FFmpeg commands refer to on web. It lives in memory: private to FFmpegKit
/// and gone on page reload. Like dart:io's Directory, an object is only a path.
/// Failures throw a PlatformException whose code is the POSIX name (ENOENT,
/// ENOTDIR, EPERM, ...).
class FFmpegKitVirtualDirectory {
  FFmpegKitVirtualDirectory._(this._path);

  final String _path;

  /// A reference to [path]. Does not create or check anything. Relative paths
  /// resolve from "/".
  static FFmpegKitVirtualDirectory from(String path) {
    if (path.isEmpty) throw ArgumentError.value(path, 'path', 'is empty');
    return FFmpegKitVirtualDirectory._(path);
  }

  String getPath() => _path;

  Future<VirtualDirectoryJs> _js() async =>
      (await FFmpegKitWebLoader.module()).virtualDirectory.from(_path.toJS);

  /// True only when a folder exists at the path.
  Future<bool> exists() =>
      guardJs(() async => (await (await _js()).exists().toDart).toDart);

  /// Creates the folder. Succeeds if it already exists. Without [recursive],
  /// fails with ENOENT when the parent folder is missing.
  Future<FFmpegKitVirtualDirectory> create({bool recursive = false}) =>
      guardJs(() async {
        final options = {'recursive': recursive}.jsify() as JSObject;
        await (await _js()).create(options).toDart;
        return this;
      });

  /// Names of the files directly inside, sorted.
  Future<List<String>> listFiles() => guardJs(() async => [
        for (final name in (await (await _js()).listFiles().toDart).toDart)
          name.toDart
      ]);

  /// Names of the folders directly inside, sorted.
  Future<List<String>> listDirectories() => guardJs(() async => [
        for (final name
            in (await (await _js()).listDirectories().toDart).toDart)
          name.toDart
      ]);

  /// Moves the folder and returns an object for [newPath]; this one keeps the
  /// old path. Fails with ENOENT when the parent of [newPath] is missing.
  Future<FFmpegKitVirtualDirectory> rename(String newPath) {
    final target = FFmpegKitVirtualDirectory.from(newPath);
    return guardJs(() async {
      await (await _js()).rename(newPath.toJS).toDart;
      return target;
    });
  }

  /// Deletes the folder and everything in it.
  Future<void> delete() =>
      guardJs(() async => (await _js()).delete().toDart);
}
