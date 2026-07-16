/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
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

#ifndef FLUTTER_PLUGIN_F_FMPEG_KIT_FLUTTER_PLUGIN_H_
#define FLUTTER_PLUGIN_F_FMPEG_KIT_FLUTTER_PLUGIN_H_

#include <flutter_linux/flutter_linux.h>

G_BEGIN_DECLS

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLUTTER_PLUGIN_EXPORT
#endif

typedef struct _FfmpegKitNextFlutterPlugin FfmpegKitNextFlutterPlugin;
typedef struct {
  GObjectClass parent_class;
} FfmpegKitNextFlutterPluginClass;

FLUTTER_PLUGIN_EXPORT GType ffmpeg_kit_next_flutter_plugin_get_type();

// The registrar entry-point name is derived by Flutter's Linux tooling from the
// pubspec pluginClass (FFmpegKitFlutterPlugin -> f_fmpeg_kit_flutter_plugin, the
// leading "FF" becoming "f_f"). It MUST match the call generated into a
// consuming app's linux/flutter/generated_plugin_registrant.cc, hence the header
// filename and this symbol both use the f_fmpeg prefix.
FLUTTER_PLUGIN_EXPORT void f_fmpeg_kit_flutter_plugin_register_with_registrar(
    FlPluginRegistrar* registrar);

G_END_DECLS

#endif  // FLUTTER_PLUGIN_F_FMPEG_KIT_FLUTTER_PLUGIN_H_
