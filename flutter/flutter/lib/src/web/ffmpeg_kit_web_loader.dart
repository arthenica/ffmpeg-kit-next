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

// Loads the ffmpeg-kit-next-web JS package once, from the plugin's web-only assets or from the URL
// the app set with FFmpegKitWebConfig.setBundleUrl(), after checking that the page is
// cross-origin isolated (the wasm module needs SharedArrayBuffer).

import 'dart:js_interop';
import 'dart:js_interop_unsafe';
import 'dart:ui_web' as ui_web;

import 'package:flutter/foundation.dart';
import 'package:web/web.dart' as web;

import 'ffmpeg_kit_js.dart';

/// The asset key of the bundle's entry module.
const String bundleAssetKey =
    'packages/ffmpeg_kit_next_flutter/web/ffmpeg-kit-next/dist/index.js';

const String _isolationMessage =
    'FFmpegKit on web needs a cross-origin isolated page. Serve it with the '
    'headers "Cross-Origin-Opener-Policy: same-origin" and '
    '"Cross-Origin-Embedder-Policy: require-corp".';

class FFmpegKitWebLoader {
  static String? _bundleUrl;
  static Future<FFmpegKitJsModule>? _loading;
  static bool Function() _crossOriginIsolated = () =>
      web.window.crossOriginIsolated;
  static Future<JSObject> Function(String url) _importer = (url) =>
      importModule(url.toJS).toDart;

  /// The JS package, imported and initialised on the first call.
  static Future<FFmpegKitJsModule> module() => _loading ??= _load().catchError((
    Object error,
    StackTrace stackTrace,
  ) {
    // A failed load can be retried, for example after setBundleUrl(). This runs after the
    // future is stored, also when _load() fails before its first await.
    _loading = null;
    Error.throwWithStackTrace(error, stackTrace);
  });

  /// True once loading has started. The bundle URL can no longer change then.
  static bool get isLoaded => _loading != null;

  /// Loads the bundle from `url` (its dist/index.js) instead of the plugin
  /// assets. Throws [StateError] once loading has started.
  static void setBundleUrl(String url) {
    if (isLoaded) {
      throw StateError(
        'FFmpegKitWebConfig.setBundleUrl() must be called '
        'before the first FFmpegKit call',
      );
    }
    _bundleUrl = url;
  }

  /// The plugin-asset URL of the bundle, made absolute against the page.
  static String defaultBundleUrl() {
    final relative = ui_web.assetManager.getAssetUrl(bundleAssetKey);
    return Uri.parse(web.window.location.href).resolve(relative).toString();
  }

  static Future<FFmpegKitJsModule> _load() async {
    if (!_crossOriginIsolated()) throw StateError(_isolationMessage);
    final url = _bundleUrl ?? defaultBundleUrl();
    final JSObject namespace;
    try {
      namespace = await _importer(url);
    } catch (e) {
      throw StateError(
        'FFmpegKit web bundle could not be loaded from $url; '
        'run ./copy_local_binaries.sh web or call '
        'FFmpegKitWebConfig.setBundleUrl() ($e)',
      );
    }
    final module = namespace as FFmpegKitJsModule;
    // The newest JS member the plugin calls; a bundle without it predates this plugin.
    if (!module.config.has('addSessionDeleteListener')) {
      throw StateError(
        'FFmpegKit web bundle at $url is older than this plugin; '
        'rebuild the web bundle and run ./copy_local_binaries.sh web',
      );
    }
    // Keep the native load confirmation, as on the other platforms; the Dart
    // initializer prints the plugin's own line after it.
    await module.config.init(true.toJS).toDart;
    return module;
  }

  @visibleForTesting
  static void debugReset({
    FFmpegKitJsModule? module,
    bool Function()? crossOriginIsolated,
    Future<JSObject> Function(String url)? importer,
  }) {
    _bundleUrl = null;
    _loading = module == null ? null : Future.value(module);
    _crossOriginIsolated =
        crossOriginIsolated ?? () => web.window.crossOriginIsolated;
    _importer = importer ?? (url) => importModule(url.toJS).toDart;
  }
}
