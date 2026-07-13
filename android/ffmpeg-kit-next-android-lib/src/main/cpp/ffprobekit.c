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

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "ffmpegkit.h"
#include "libavcodec/jni.h"
#include "libavutil/bprint.h"
#include "libavutil/mem.h"

/** Forward declaration for function defined in fftools/ffprobe.c */
int ffprobe_execute(int argc, char **argv);

/** Forward declaration for function defined in fftools/ffprobe.c */
void ffprobe_set_media_information_buffer(AVBPrint *buffer);

extern int configuredLogLevel;
extern __thread long globalSessionId;
extern void addSession(long sessionId);
extern void removeSession(long sessionId);
extern void resetMessagesInTransmit(long sessionId);

/**
 * Synchronously executes FFprobe natively with arguments provided.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param id session id
 * @param stringArray reference to the object holding FFprobe command arguments
 * @return zero on successful execution, non-zero on error
 */
JNIEXPORT jint JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFprobeExecute(
    JNIEnv *env, jclass object, jlong id, jobjectArray stringArray) {
    jstring *tempArray = NULL;
    int argumentCount = 1;
    char **argv = NULL;

    // SETS DEFAULT LOG LEVEL BEFORE STARTING A NEW RUN
    av_log_set_level(configuredLogLevel);

    if (stringArray) {
        int programArgumentCount = (*env)->GetArrayLength(env, stringArray);
        argumentCount = programArgumentCount + 1;

        tempArray =
            (jstring *)av_mallocz(sizeof(jstring) * programArgumentCount);
    }

    /* PRESERVE USAGE FORMAT
     *
     * ffprobe <arguments>
     */
    argv = (char **)av_mallocz(sizeof(char *) * (argumentCount + 1));
    argv[0] = (char *)av_malloc(sizeof(char) * (strlen(LIB_NAME) + 1));
    strcpy(argv[0], LIB_NAME);

    // PREPARE ARRAY ELEMENTS
    if (stringArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            tempArray[i] =
                (jstring)(*env)->GetObjectArrayElement(env, stringArray, i);
            if (tempArray[i] != NULL) {
                argv[i + 1] =
                    (char *)(*env)->GetStringUTFChars(env, tempArray[i], 0);
            }
        }
    }
    argv[argumentCount] = NULL;

    // REGISTER THE ID BEFORE STARTING THE SESSION
    globalSessionId = (long)id;
    addSession((long)id);

    resetMessagesInTransmit(globalSessionId);

    // RUN
    int returnCode = ffprobe_execute(argumentCount, argv);

    // ALWAYS REMOVE THE ID FROM THE MAP
    removeSession((long)id);

    // CLEANUP
    if (tempArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            if (tempArray[i] != NULL && argv[i + 1] != NULL) {
                (*env)->ReleaseStringUTFChars(env, tempArray[i], argv[i + 1]);
            }
        }

        av_free(tempArray);
    }
    av_free(argv[0]);
    av_free(argv);

    return returnCode;
}

/**
 * Synchronously executes FFprobe natively, capturing its formatted output into
 * an in-memory buffer, and returns the raw bytes.
 *
 * Unlike nativeFFprobeExecute, the structured output is written straight into a
 * caller-owned AVBPrint (via the ffprobe buffer writer) instead of the av_log
 * path, so getMediaInformation never truncates large values. The bytes are
 * returned raw (UTF-8) and decoded on the Kotlin side to avoid JNI modified
 * UTF-8 issues with 4-byte sequences.
 *
 * @param env pointer to native method interface
 * @param object reference to the class on which this method is invoked
 * @param id session id
 * @param stringArray reference to the object holding FFprobe command arguments
 * @return captured output bytes on success, or NULL on a non-zero return code
 */
JNIEXPORT jbyteArray JNICALL
Java_com_arthenica_ffmpegkit_FFmpegKitConfig_nativeFFprobeGetMediaInformation(
    JNIEnv *env, jclass object, jlong id, jobjectArray stringArray) {
    jstring *tempArray = NULL;
    int argumentCount = 1;
    char **argv = NULL;

    // SETS DEFAULT LOG LEVEL BEFORE STARTING A NEW RUN
    av_log_set_level(configuredLogLevel);

    if (stringArray) {
        int programArgumentCount = (*env)->GetArrayLength(env, stringArray);
        argumentCount = programArgumentCount + 1;

        tempArray =
            (jstring *)av_mallocz(sizeof(jstring) * programArgumentCount);
    }

    /* PRESERVE USAGE FORMAT
     *
     * ffprobe <arguments>
     */
    argv = (char **)av_mallocz(sizeof(char *) * (argumentCount + 1));
    argv[0] = (char *)av_malloc(sizeof(char) * (strlen(LIB_NAME) + 1));
    strcpy(argv[0], LIB_NAME);

    // PREPARE ARRAY ELEMENTS
    if (stringArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            tempArray[i] =
                (jstring)(*env)->GetObjectArrayElement(env, stringArray, i);
            if (tempArray[i] != NULL) {
                argv[i + 1] =
                    (char *)(*env)->GetStringUTFChars(env, tempArray[i], 0);
            }
        }
    }
    argv[argumentCount] = NULL;

    // REGISTER THE ID BEFORE STARTING THE SESSION
    globalSessionId = (long)id;
    addSession((long)id);

    resetMessagesInTransmit(globalSessionId);

    // ffprobe WRITES ITS FORMATTED OUTPUT INTO THIS CALLER-OWNED BUFFER INSTEAD
    // OF THE av_log PATH. THE POINTER IS THREAD-LOCAL AND IS CLEARED IMMEDIATELY
    // AFTER THE RUN SO A REUSED THREAD NEVER SEES A STALE (FINALIZED) BUFFER.
    AVBPrint mediaInformationBuffer;
    av_bprint_init(&mediaInformationBuffer, 0, AV_BPRINT_SIZE_UNLIMITED);
    ffprobe_set_media_information_buffer(&mediaInformationBuffer);

    // RUN
    int returnCode = ffprobe_execute(argumentCount, argv);

    ffprobe_set_media_information_buffer(NULL);

    // ALWAYS REMOVE THE ID FROM THE MAP
    removeSession((long)id);

    // BUILD THE RESULT (RAW UTF-8 BYTES; NULL WHEN ffprobe FAILED)
    jbyteArray outputBytes = NULL;
    if (returnCode == 0) {
        size_t stored = mediaInformationBuffer.len;
        if (stored >= mediaInformationBuffer.size) {
            stored = mediaInformationBuffer.size - 1; // truncated (e.g. OOM)
        }
        outputBytes = (*env)->NewByteArray(env, (jsize)stored);
        if (outputBytes != NULL) {
            (*env)->SetByteArrayRegion(
                env, outputBytes, 0, (jsize)stored,
                (const jbyte *)mediaInformationBuffer.str);
        }
    }
    av_bprint_finalize(&mediaInformationBuffer, NULL);

    // CLEANUP
    if (tempArray) {
        for (int i = 0; i < (argumentCount - 1); i++) {
            if (tempArray[i] != NULL && argv[i + 1] != NULL) {
                (*env)->ReleaseStringUTFChars(env, tempArray[i], argv[i + 1]);
            }
        }

        av_free(tempArray);
    }
    av_free(argv[0]);
    av_free(argv);

    return outputBytes;
}
