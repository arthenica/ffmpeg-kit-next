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

// Turns rejections of the JS package into PlatformExceptions, the error type
// the plugin reports on every platform. Dart errors (for example the loader's
// StateError) pass through unchanged.

import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'package:flutter/services.dart';

/// The code used when a JS error carries no POSIX `code`.
const String webErrorCode = 'WEB_ERROR';

/// Runs [call]; a rejected JS promise becomes a [PlatformException] carrying
/// the JS error's `code` (or [webErrorCode]) and `message`.
Future<T> guardJs<T>(Future<T> Function() call) async {
  try {
    return await call();
  } catch (e) {
    if (e is Exception || e is Error) rethrow;
    throw toPlatformException(e);
  }
}

/// The [PlatformException] for an error thrown by the JS package.
PlatformException toPlatformException(Object error) {
  final value = error as JSAny?;
  if (value != null && value.isA<JSObject>()) {
    final object = value as JSObject;
    final code = object.getProperty<JSAny?>('code'.toJS);
    final message = object.getProperty<JSAny?>('message'.toJS);
    return PlatformException(
      code: code != null && code.isA<JSString>()
          ? (code as JSString).toDart
          : webErrorCode,
      message: message != null && message.isA<JSString>()
          ? (message as JSString).toDart
          : error.toString(),
    );
  }
  return PlatformException(code: webErrorCode, message: error.toString());
}

/// The error the native plugins return for methods a platform cannot support.
PlatformException notSupportedOnWeb() => PlatformException(
    code: 'NOT_SUPPORTED', message: 'Not supported on web platform.');
