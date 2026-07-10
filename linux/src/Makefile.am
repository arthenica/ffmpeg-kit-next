lib_LTLIBRARIES = libffmpegkit.la

libffmpegkit_la_LIBADD = @FFMPEG_LIBS@

EXTRA_DIST = \
    stdbit.h \
    fftools/resources/graph.css \
    fftools/resources/graph.html

libffmpegkit_la_SOURCES = \
    AbstractSession.cpp \
    ArchDetect.cpp \
    Chapter.cpp \
    FFmpegKit.cpp \
    FFmpegKitConfig.cpp \
    FFmpegKitInputBuffer.cpp \
    FFmpegKitOutputBuffer.cpp \
    FFmpegSession.cpp \
    FFmpegKitStreamInput.cpp \
    FFmpegKitStreamOutput.cpp \
    FFprobeKit.cpp \
    FFprobeSession.cpp \
    Log.cpp \
    MediaInformation.cpp \
    MediaInformationJsonParser.cpp \
    MediaInformationSession.cpp \
    Packages.cpp \
    ReturnCode.cpp \
    Statistics.cpp \
    StreamInformation.cpp \
    ffmpeg_context.c \
    ffmpegkit_exception.cpp \
    fftools/cmdutils.c \
    fftools/ffmpeg.c \
    fftools/ffmpeg_dec.c \
    fftools/ffmpeg_demux.c \
    fftools/ffmpeg_enc.c \
    fftools/ffmpeg_filter.c \
    fftools/ffmpeg_hw.c \
    fftools/ffmpeg_mux.c \
    fftools/ffmpeg_mux_init.c \
    fftools/ffmpeg_opt.c \
    fftools/ffmpeg_sched.c \
    fftools/ffprobe.c \
    fftools/graph/graphprint.c \
    fftools/opt_common.c \
    fftools/resources/graph_resources.c \
    fftools/resources/resman.c \
    fftools/sync_queue.c \
    fftools/textformat/avtextformat.c \
    fftools/textformat/tf_compact.c \
    fftools/textformat/tf_default.c \
    fftools/textformat/tf_flat.c \
    fftools/textformat/tf_ini.c \
    fftools/textformat/tf_json.c \
    fftools/textformat/tf_mermaid.c \
    fftools/textformat/tf_xml.c \
    fftools/thread_queue.c \
    fftools/textformat/tw_avio.c \
    fftools/textformat/tw_buffer.c \
    fftools/textformat/tw_stdout.c

include_HEADERS = \
    AbstractSession.h \
    ArchDetect.h \
    Chapter.h \
    FFmpegKit.h \
    FFmpegKitConfig.h \
    FFmpegKitInputBuffer.h \
    FFmpegKitOutputBuffer.h \
    FFmpegSession.h \
    FFmpegKitStreamInput.h \
    FFmpegKitStreamOutput.h \
    FFmpegSessionCompleteCallback.h \
    FFprobeKit.h \
    FFprobeSession.h \
    FFprobeSessionCompleteCallback.h \
    Level.h \
    Log.h \
    LogCallback.h \
    LogRedirectionStrategy.h \
    MediaInformation.h \
    MediaInformationJsonParser.h \
    MediaInformationSession.h \
    MediaInformationSessionCompleteCallback.h \
    Packages.h \
    ReturnCode.h \
    Session.h \
    SessionState.h \
    Statistics.h \
    StatisticsCallback.h \
    StreamInformation.h \
    ffmpeg_context.h \
    ffmpegkit_exception.h \
    fftools/cmdutils.h \
    fftools/ffmpeg.h \
    fftools/ffmpeg_mux.h \
    fftools/ffmpeg_sched.h \
    fftools/ffmpeg_utils.h \
    fftools/fopen_utf8.h \
    fftools/graph/graphprint.h \
    fftools/opt_common.h \
    fftools/resources/resman.h \
    fftools/sync_queue.h \
    fftools/textformat/avtextformat.h \
    fftools/textformat/avtextwriters.h \
    fftools/textformat/tf_internal.h \
    fftools/textformat/tf_mermaid.h \
    fftools/thread_queue.h

libffmpegkit_la_CFLAGS = $(CFLAGS)
libffmpegkit_la_OBJCFLAGS = $(CFLAGS)
libffmpegkit_la_CXXFLAGS = $(CXXFLAGS)
libffmpegkit_la_LDFLAGS = -version-info @VERSION_INFO@
