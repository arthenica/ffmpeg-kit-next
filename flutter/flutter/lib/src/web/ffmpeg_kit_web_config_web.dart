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

import 'ffmpeg_kit_web_loader.dart';

/// Web-only configuration of the FFmpegKit plugin.
class FFmpegKitWebConfig {
  FFmpegKitWebConfig._();

  /// Loads the FFmpegKit web bundle from `url` (its dist/index.js) instead of
  /// the plugin's assets, for example from the app's own server or a CDN. Call
  /// it before the first FFmpegKit call; afterwards it throws [StateError].
  static void setBundleUrl(String url) =>
      FFmpegKitWebLoader.setBundleUrl(url);
}
