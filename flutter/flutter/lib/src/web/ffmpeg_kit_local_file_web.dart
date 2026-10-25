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

import 'package:web/web.dart' as web;

import 'ffmpeg_kit_js.dart';
import 'ffmpeg_kit_web_errors.dart';
import 'ffmpeg_kit_web_loader.dart';

/// A file on the user's computer, for example one chosen with
/// `<input type="file">`. It gets a read-only path in FFmpegKit's virtual file
/// storage; the file is not copied. Pass the path to FFmpeg as an argument
/// (FFmpegKit.executeWithArguments): it keeps the file's own name, which may
/// contain spaces or quotes.
class FFmpegKitLocalFile {
  FFmpegKitLocalFile._(this._js);

  final LocalFileJs _js;

  /// Gives [file] a read-only path FFmpeg can read, without copying it.
  /// [name] is required unless [file] is a `web.File`, and must not contain
  /// "/".
  static Future<FFmpegKitLocalFile> fromFile(web.Blob file, {String? name}) {
    final isFile = file.isA<web.File>();
    final fileName = name ?? (isFile ? (file as web.File).name : null);
    if (fileName == null || fileName.isEmpty) {
      return Future.error(ArgumentError.value(
          name, 'name', 'is required for a Blob that is not a File'));
    }
    if (fileName.contains('/')) {
      return Future.error(
          ArgumentError.value(name, 'name', 'must not contain "/"'));
    }
    final JSObject argument = isFile && name == null
        ? file
        : {'name': fileName, 'data': file}.jsify() as JSObject;
    return guardJs(() async {
      final module = await FFmpegKitWebLoader.module();
      return FFmpegKitLocalFile._(
          await module.localFile.fromFile(argument).toDart);
    });
  }

  /// The path FFmpeg commands use for this file.
  String getPath() => _js.getPath().toDart;

  /// Releases the file. Safe to call more than once. Call it only after every
  /// FFmpeg session that reads the file has finished.
  Future<void> close() => guardJs(() => _js.close().toDart);
}
