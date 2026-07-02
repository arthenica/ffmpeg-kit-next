MY_LOCAL_PATH := $(call my-dir)
$(call import-add-path, $(MY_LOCAL_PATH))

include $(MY_LOCAL_PATH)/build.mk

MY_ARMV7 := false
MY_ARMV7_NEON := false
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    MY_ARMV7 := ${ARMV7}
    MY_ARMV7_NEON := ${ARMV7_NEON}
endif

# DEFINE ARCH FLAGS
MY_ARM_MODE := arm
MY_ARM_NEON := false
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    MY_ARCH_FLAGS := ARM_V7A
    MY_ARM_NEON := true
    ifeq ($(MY_ARMV7_NEON), true)
        MY_BUILD_DIR := $(ARMV7_NEON_BUILD_PATH)
    else
        MY_BUILD_DIR := $(ARMV7_BUILD_PATH)
    endif
endif
ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
    MY_ARCH_FLAGS := ARM64_V8A
    MY_ARM_NEON := true
    MY_BUILD_DIR := $(ARM64_BUILD_PATH)
endif
ifeq ($(TARGET_ARCH_ABI), x86)
    MY_ARCH_FLAGS := X86
    MY_ARM_NEON := true
    MY_BUILD_DIR := $(X86_BUILD_PATH)
endif
ifeq ($(TARGET_ARCH_ABI), x86_64)
    MY_ARCH_FLAGS := X86_64
    MY_ARM_NEON := true
    MY_BUILD_DIR := $(X86_64_BUILD_PATH)
endif
FFMPEG_INCLUDES := $(MY_LOCAL_PATH)/../../prebuilt/$(MY_BUILD_DIR)/ffmpeg/include
LOCAL_PATH := $(MY_LOCAL_PATH)/../ffmpeg-kit-next-android-lib/src/main/cpp

# 16 kb page size support for arm64-v8a and x86_64
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    MY_LDFLAGS := -Wl,-z,max-page-size=16384
else ifeq ($(TARGET_ARCH_ABI),x86_64)
    MY_LDFLAGS := -Wl,-z,max-page-size=16384
endif

include $(CLEAR_VARS)
LOCAL_ARM_MODE := $(MY_ARM_MODE)
LOCAL_MODULE := ffmpegkit_abidetect
LOCAL_SRC_FILES := ffmpegkit_abidetect.c
LOCAL_CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter -DFFMPEG_KIT_${MY_ARCH_FLAGS}
LOCAL_C_INCLUDES := $(FFMPEG_INCLUDES)
LOCAL_LDFLAGS := $(MY_LDFLAGS)
LOCAL_LDLIBS := -llog -lz -landroid
LOCAL_STATIC_LIBRARIES := cpu-features
LOCAL_ARM_NEON := ${MY_ARM_NEON}
include $(BUILD_SHARED_LIBRARY)

$(call import-module, cpu-features)

MY_SRC_FILES := ffmpegkit.c ffprobekit.c ffmpegkit_exception.c fftools/cmdutils.c fftools/ffmpeg.c fftools/ffprobe.c fftools/ffmpeg_mux.c fftools/ffmpeg_mux_init.c fftools/ffmpeg_demux.c fftools/ffmpeg_enc.c fftools/ffmpeg_dec.c fftools/ffmpeg_opt.c fftools/ffmpeg_sched.c fftools/opt_common.c fftools/ffmpeg_hw.c fftools/ffmpeg_filter.c fftools/graph/graphprint.c fftools/resources/graph_resources.c fftools/resources/resman.c fftools/textformat/avtextformat.c fftools/textformat/tf_compact.c fftools/textformat/tf_default.c fftools/textformat/tf_flat.c fftools/textformat/tf_ini.c fftools/textformat/tf_json.c fftools/textformat/tf_mermaid.c fftools/textformat/tf_xml.c fftools/sync_queue.c fftools/thread_queue.c fftools/textformat/tw_avio.c fftools/textformat/tw_buffer.c fftools/textformat/tw_stdout.c android_support.c ffmpeg_context.c compat/android/binder.c

MY_CFLAGS := -Wall -Werror -Wno-unused-parameter -Wno-switch -Wno-sign-compare
MY_LDLIBS := -llog -lz -landroid

MY_BUILD_GENERIC_FFMPEG_KIT := true

ifeq ($(MY_ARMV7_NEON), true)
    include $(CLEAR_VARS)
    LOCAL_PATH := $(MY_LOCAL_PATH)/../ffmpeg-kit-next-android-lib/src/main/cpp
    LOCAL_ARM_MODE := $(MY_ARM_MODE)
    LOCAL_MODULE := ffmpegkit_armv7a_neon
    LOCAL_SRC_FILES := $(MY_SRC_FILES)
    LOCAL_C_INCLUDES := $(LOCAL_PATH)
    LOCAL_CFLAGS := $(MY_CFLAGS)
    LOCAL_LDFLAGS := $(MY_LDFLAGS)
    LOCAL_LDLIBS := $(MY_LDLIBS)
    LOCAL_SHARED_LIBRARIES := libavcodec_neon libavfilter_neon libswscale_neon libavformat_neon libavutil_neon libswresample_neon libavdevice_neon
    LOCAL_ARM_NEON := true
    include $(BUILD_SHARED_LIBRARY)

    $(call import-module, ffmpeg/neon)

    ifneq ($(MY_ARMV7), true)
        MY_BUILD_GENERIC_FFMPEG_KIT := false
    endif
endif

ifeq ($(MY_BUILD_GENERIC_FFMPEG_KIT), true)
    include $(CLEAR_VARS)
    LOCAL_PATH := $(MY_LOCAL_PATH)/../ffmpeg-kit-next-android-lib/src/main/cpp
    LOCAL_ARM_MODE := $(MY_ARM_MODE)
    LOCAL_MODULE := ffmpegkit
    LOCAL_SRC_FILES := $(MY_SRC_FILES)
    LOCAL_C_INCLUDES := $(LOCAL_PATH)
    LOCAL_CFLAGS := $(MY_CFLAGS)
    LOCAL_LDFLAGS := $(MY_LDFLAGS)
    LOCAL_LDLIBS := $(MY_LDLIBS)
    LOCAL_SHARED_LIBRARIES := libavfilter libavformat libavcodec libavutil libswresample libavdevice libswscale
    LOCAL_ARM_NEON := ${MY_ARM_NEON}
    include $(BUILD_SHARED_LIBRARY)

    $(call import-module, ffmpeg)
endif
