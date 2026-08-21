lib_LTLIBRARIES = libffmpegkit.la

libffmpegkit_la_LIBADD = @FFMPEG_LIBS@ @WINDOWS_LIBS@

EXTRA_DIST = \
    stdbit.h \
    fftools/resources/graph.css \
    fftools/resources/graph.html

# THE C++ IMPLEMENTATION LIVES IN internal/ AND IS COMPILED INTO THE DLL ONLY.
# ffmpegkit_c.cpp IS THE SHIM BETWEEN IT AND THE FLAT C ABI.
libffmpegkit_la_SOURCES = \
    internal/AbstractSession.cpp \
    internal/ArchDetect.cpp \
    internal/Chapter.cpp \
    internal/FFmpegKit.cpp \
    internal/FFmpegKitConfig.cpp \
    internal/FFmpegKitInputBuffer.cpp \
    internal/FFmpegKitOutputBuffer.cpp \
    internal/FFmpegSession.cpp \
    internal/FFmpegKitStreamInput.cpp \
    internal/FFmpegKitStreamOutput.cpp \
    internal/FFprobeKit.cpp \
    internal/FFprobeSession.cpp \
    internal/Log.cpp \
    internal/MediaInformation.cpp \
    internal/MediaInformationJsonParser.cpp \
    internal/MediaInformationSession.cpp \
    internal/Packages.cpp \
    internal/ReturnCode.cpp \
    internal/Statistics.cpp \
    internal/StreamInformation.cpp \
    internal/ffmpegkit_exception.cpp \
    ffmpeg_context.c \
    ffmpegkit_c.cpp \
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

# THE INSTALLED PUBLIC API IS THE FLAT C ABI IN ffmpegkit_c.h PLUS THE
# HEADER-ONLY C++ FACADE THAT SITS ON TOP OF IT. THESE HEADERS INSTALL WITH THE
# SAME NAMES THEY HAVE HERE, SO THIS DIRECTORY MIRRORS $(includedir). EVERYTHING
# UNDER internal/ AND fftools/ IS BUILD-ONLY AND IS NOT INSTALLED.
include_HEADERS = \
    AbstractSession.h \
    ArchDetect.h \
    Chapter.h \
    FFmpegKit.h \
    FFmpegKitConfig.h \
    FFmpegKitInputBuffer.h \
    FFmpegKitOutputBuffer.h \
    FFmpegKitStreamInput.h \
    FFmpegKitStreamOutput.h \
    FFmpegSession.h \
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
    SessionDeleteListener.h \
    SessionState.h \
    Statistics.h \
    StatisticsCallback.h \
    StreamInformation.h \
    ffmpegkit_c.h \
    ffmpegkit_exception.h \
    ffmpegkit_facade.h

# json/Value.h is installed with its subdirectory preserved so that the
# "json/Value.h" include in the public headers keeps resolving after install
nobase_include_HEADERS = \
    json/Value.h

# THE SHIM AND THE IMPLEMENTATION INCLUDE "internal/...", "fftools/..." AND
# "json/Value.h" RELATIVE TO THIS DIRECTORY, AND FFMPEG_KIT_BUILDING_DLL FLIPS
# FFK_API OVER TO __declspec(dllexport) FOR THE LIBRARY ITSELF.
AM_CPPFLAGS = -I$(srcdir) -DFFMPEG_KIT_BUILDING_DLL

libffmpegkit_la_CFLAGS = $(CFLAGS)
libffmpegkit_la_OBJCFLAGS = $(CFLAGS)
libffmpegkit_la_CXXFLAGS = $(CXXFLAGS)

# -no-undefined IS REQUIRED FOR libtool TO PRODUCE A WINDOWS DLL: EVERY SYMBOL
# MUST BE RESOLVED AT LINK TIME. ALL DEPENDENCIES ARE LISTED IN LIBADD ABOVE.
#
# -export-symbols-regex RESTRICTS THE DLL EXPORT TABLE TO THE FLAT C API. WITHOUT
# IT MinGW AUTO-EXPORTS EVERY SYMBOL, INCLUDING THE MANGLED C++ NAMES OF THE
# IMPLEMENTATION CLASSES, WHICH NO MSVC CONSUMER CAN LINK AGAINST.
libffmpegkit_la_LDFLAGS = -no-undefined -version-info @VERSION_INFO@ \
    -export-symbols-regex '^ffk_'
