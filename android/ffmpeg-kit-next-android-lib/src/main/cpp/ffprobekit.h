/*
 * Copyright (c) 2020-2021, 2026 Taner Sener
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

#ifndef FFPROBE_KIT_H
#define FFPROBE_KIT_H

#include <jni.h>

/*
 * Class:     com_arthenica_ffmpegkit_FFmpegKitConfig
 * Method:    nativeFFprobeExecute
 * Signature: (J[Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFprobeExecute(JNIEnv *,
                                                                  jclass, jlong,
                                                                  jobjectArray);

/*
 * Class:     com_arthenica_ffmpegkit_FFmpegKitConfig
 * Method:    nativeFFprobeGetMediaInformation
 * Signature: (J[Ljava/lang/String;)[B
 *
 * Runs ffprobe with its formatted output captured into an in-memory buffer and
 * returns the raw UTF-8 bytes (decoded as UTF-8 on the Kotlin side). Used by
 * getMediaInformation so the JSON never routes through the av_log path.
 */
JNIEXPORT jbyteArray JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFprobeGetMediaInformation(
    JNIEnv *, jclass, jlong, jobjectArray);

#endif /* FFPROBE_KIT_H */