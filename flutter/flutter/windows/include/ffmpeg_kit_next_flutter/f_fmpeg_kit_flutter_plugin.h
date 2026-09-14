#ifndef F_FMPEG_KIT_FLUTTER_PLUGIN_H
#define F_FMPEG_KIT_FLUTTER_PLUGIN_H

#include <flutter_plugin_registrar.h>

#ifdef FLUTTER_PLUGIN_IMPL
#define FFMPEG_KIT_FLUTTER_EXPORT __declspec(dllexport)
#else
#define FFMPEG_KIT_FLUTTER_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

FFMPEG_KIT_FLUTTER_EXPORT void FFmpegKitFlutterPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar);

#ifdef __cplusplus
}
#endif

#endif
