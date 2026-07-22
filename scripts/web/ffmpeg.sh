#!/bin/bash

HOST_PKG_CONFIG_PATH=$(command -v pkg-config)
if [ -z "${HOST_PKG_CONFIG_PATH}" ]; then
  echo -e "\n(*) pkg-config command not found\n"
  exit 1
fi

LIB_NAME="ffmpeg"

echo -e "----------------------------------------------------------------" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "\nINFO: Building ${LIB_NAME} for ${HOST} with the following environment variables\n" 1>>"${BASEDIR}"/build.log 2>&1
env 1>>"${BASEDIR}"/build.log 2>&1
echo -e "----------------------------------------------------------------\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "INFO: System information\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "INFO: $(uname -a)\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "----------------------------------------------------------------\n" 1>>"${BASEDIR}"/build.log 2>&1

FFMPEG_LIBRARY_PATH="${LIB_INSTALL_BASE}/${LIB_NAME}"

# SET PATHS
set_toolchain_paths "${LIB_NAME}"

# SET BUILD FLAGS
HOST=$(get_host)
export CFLAGS="$(get_cflags "${LIB_NAME}")"
export CXXFLAGS="$(get_cxxflags "${LIB_NAME}")"
export LDFLAGS="$(get_ldflags "${LIB_NAME}")"
export PKG_CONFIG_LIBDIR="$(get_web_pkg_config_libdir)"
# emconfigure overrides PKG_CONFIG_LIBDIR with Emscripten's sysroot and sets
# PKG_CONFIG_PATH from EM_PKG_CONFIG_PATH (see emscripten tools/building.py). This is
# how our built libraries' .pc files reach FFmpeg's pkg-config search path.
export EM_PKG_CONFIG_PATH="$(get_web_pkg_config_libdir)"
unset PKG_CONFIG_PATH

echo -e "\nINFO: Using PKG_CONFIG_LIBDIR: ${PKG_CONFIG_LIBDIR}\n" 1>>"${BASEDIR}"/build.log 2>&1

cd "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

TARGET_CPU="generic"
TARGET_ARCH="wasm"
ASM_OPTIONS="--disable-asm --disable-inline-asm --disable-x86asm --enable-simd128"
CONFIGURE_POSTFIX=""
HIGH_PRIORITY_INCLUDES=""

# SET CONFIGURE OPTIONS
for library in {0..61} ${LIBRARY_VVENC} ${LIBRARY_LIBSVTAV1} ${LIBRARY_LIBJXL} ${LIBRARY_LIBLC3} ${LIBRARY_WEB_LIBICONV} ${LIBRARY_WEB_ZLIB}; do
  if [[ ${ENABLED_LIBRARIES[$library]} -eq 1 ]]; then
    ENABLED_LIBRARY=$(get_library_name ${library})

    echo -e "INFO: Enabling library ${ENABLED_LIBRARY}\n" 1>>"${BASEDIR}"/build.log 2>&1

    case ${ENABLED_LIBRARY} in
    chromaprint)
      CFLAGS+=" $(pkg-config --cflags libchromaprint 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libchromaprint 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-chromaprint"
      ;;
    dav1d)
      CFLAGS+=" $(pkg-config --cflags dav1d 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L dav1d 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libdav1d"
      ;;
    fontconfig)
      CFLAGS+=" $(pkg-config --cflags fontconfig 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L fontconfig 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfontconfig"
      ;;
    freetype)
      CFLAGS+=" $(pkg-config --cflags freetype2 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L freetype2 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfreetype"
      ;;
    fribidi)
      CFLAGS+=" $(pkg-config --cflags fribidi 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L fribidi 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfribidi"
      ;;
    harfbuzz)
      CFLAGS+=" $(pkg-config --cflags harfbuzz 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L harfbuzz 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libharfbuzz"
      ;;
    gmp)
      CFLAGS+=" $(pkg-config --cflags gmp 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L gmp 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-gmp"
      ;;
    gnutls)
      CFLAGS+=" $(pkg-config --cflags gnutls 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L gnutls 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-gnutls"
      ;;
    kvazaar)
      CFLAGS+=" $(pkg-config --cflags kvazaar 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L kvazaar 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libkvazaar"
      ;;
    vvenc)
      CFLAGS+=" $(pkg-config --cflags libvvenc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libvvenc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvvenc"
      ;;
    lame)
      CFLAGS+=" $(pkg-config --cflags libmp3lame 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libmp3lame 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libmp3lame"
      ;;
    libaom)
      CFLAGS+=" $(pkg-config --cflags aom 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L aom 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libaom"
      ;;
    libjxl)
      CFLAGS+=" $(pkg-config --cflags --static libjxl libjxl_threads 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libjxl libjxl_threads 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libjxl"
      ;;
    liblc3)
      CFLAGS+=" $(pkg-config --cflags lc3 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L lc3 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-liblc3"
      ;;
    libsvtav1)
      CFLAGS+=" $(pkg-config --cflags SvtAv1Enc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L SvtAv1Enc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsvtav1"
      ;;
    libass)
      CFLAGS+=" $(pkg-config --cflags libass 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libass 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libass"
      ;;
    libilbc)
      CFLAGS+=" $(pkg-config --cflags libilbc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libilbc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libilbc"
      ;;
    libtheora)
      CFLAGS+=" $(pkg-config --cflags theora 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L theora 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtheora"
      ;;
    libvidstab)
      CFLAGS+=" $(pkg-config --cflags vidstab 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L vidstab 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvidstab"
      ;;
    libvorbis)
      CFLAGS+=" $(pkg-config --cflags vorbis 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L vorbis 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvorbis"
      ;;
    libvpx)
      CFLAGS+=" $(pkg-config --cflags vpx 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L vpx 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvpx"
      ;;
    libwebp)
      CFLAGS+=" $(pkg-config --cflags libwebp 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libwebp 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libwebp"
      ;;
    libxml2)
      CFLAGS+=" $(pkg-config --cflags libxml-2.0 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libxml-2.0 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libxml2"
      ;;
    web-libiconv)
      CFLAGS+=" $(pkg-config --cflags libiconv 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs libiconv 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-iconv"
      ;;
    web-zlib)
      CFLAGS+=" $(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs zlib 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-zlib"
      ;;
    opencore-amr)
      CFLAGS+=" $(pkg-config --cflags opencore-amrnb 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L opencore-amrnb 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopencore-amrnb"
      ;;
    openh264)
      CFLAGS+=" $(pkg-config --cflags openh264 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L openh264 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopenh264"
      ;;
    openssl)
      CFLAGS+=" $(pkg-config --cflags openssl 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L openssl 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-openssl"
      ;;
    opus)
      CFLAGS+=" $(pkg-config --cflags opus 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L opus 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopus"
      ;;
    rubberband)
      CFLAGS+=" $(pkg-config --cflags rubberband 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L rubberband 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-librubberband"
      ;;
    sdl)
      CFLAGS+=" $(pkg-config --cflags sdl2 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L sdl2 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-sdl2"
      ;;
    shine)
      CFLAGS+=" $(pkg-config --cflags shine 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L shine 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libshine"
      ;;
    snappy)
      CFLAGS+=" $(pkg-config --cflags snappy 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L snappy 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsnappy"
      ;;
    soxr)
      CFLAGS+=" $(pkg-config --cflags soxr 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L soxr 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsoxr"
      ;;
    speex)
      CFLAGS+=" $(pkg-config --cflags speex 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L speex 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libspeex"
      ;;
    srt)
      CFLAGS+=" $(pkg-config --cflags srt 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L srt 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsrt"
      ;;
    tesseract)
      CFLAGS+=" $(pkg-config --cflags tesseract 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L tesseract 2>>"${BASEDIR}"/build.log)"
      CFLAGS+=" $(pkg-config --cflags giflib 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L giflib 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtesseract"
      ;;
    twolame)
      CFLAGS+=" $(pkg-config --cflags twolame 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L twolame 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtwolame"
      ;;
    vo-amrwbenc)
      CFLAGS+=" $(pkg-config --cflags vo-amrwbenc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L vo-amrwbenc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvo-amrwbenc"
      ;;
    x264)
      CFLAGS+=" $(pkg-config --cflags x264 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L x264 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libx264"
      ;;
    x265)
      CFLAGS+=" $(pkg-config --cflags x265 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L x265 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libx265"
      ;;
    xvidcore)
      CFLAGS+=" $(pkg-config --cflags xvidcore 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L xvidcore 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libxvid"
      ;;
    zimg)
      CFLAGS+=" $(pkg-config --cflags zimg 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L zimg 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libzimg"
      ;;
    expat)
      CFLAGS+=" $(pkg-config --cflags expat 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L expat 2>>"${BASEDIR}"/build.log)"
      ;;
    libogg)
      CFLAGS+=" $(pkg-config --cflags ogg 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L ogg 2>>"${BASEDIR}"/build.log)"
      ;;
    libpng)
      CFLAGS+=" $(pkg-config --cflags libpng 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L libpng 2>>"${BASEDIR}"/build.log)"
      ;;
    nettle)
      CFLAGS+=" $(pkg-config --cflags nettle 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L nettle 2>>"${BASEDIR}"/build.log)"
      CFLAGS+=" $(pkg-config --cflags hogweed 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs-only-L hogweed 2>>"${BASEDIR}"/build.log)"
      ;;
    esac
  else
    if [[ ${library} -eq ${LIBRARY_WEB_LIBICONV} ]]; then
      CONFIGURE_POSTFIX+=" --disable-iconv"
    elif [[ ${library} -eq ${LIBRARY_WEB_ZLIB} ]]; then
      CONFIGURE_POSTFIX+=" --disable-zlib"
    fi
  fi
done

# SET CONFIGURE OPTIONS FOR CUSTOM LIBRARIES
for custom_library_index in "${CUSTOM_LIBRARIES[@]}"; do
  library_name="CUSTOM_LIBRARY_${custom_library_index}_NAME"
  pc_file_name="CUSTOM_LIBRARY_${custom_library_index}_PACKAGE_CONFIG_FILE_NAME"
  ffmpeg_flag_name="CUSTOM_LIBRARY_${custom_library_index}_FFMPEG_ENABLE_FLAG"

  echo -e "INFO: Enabling custom library ${!library_name}\n" 1>>"${BASEDIR}"/build.log 2>&1

  CFLAGS+=" $(pkg-config --cflags ${!pc_file_name} 2>>"${BASEDIR}"/build.log)"
  LDFLAGS+=" $(pkg-config --libs-only-L ${!pc_file_name} 2>>"${BASEDIR}"/build.log)"
  CONFIGURE_POSTFIX+=" --enable-${!ffmpeg_flag_name}"
done

# SET ENABLE GPL FLAG WHEN REQUESTED
if [ "$GPL_ENABLED" == "yes" ]; then
  CONFIGURE_POSTFIX+=" --enable-gpl"
fi

if web_linkage_is_static; then
  # Static mode links FFmpeg archives into one libffmpegkit.wasm.
  BUILD_LIBRARY_OPTIONS="--enable-static --disable-shared"
  SHARED_LIBRARY_OPTIONS=""
else
  # Dynamic mode builds FFmpeg shared libraries as Emscripten side modules.
  BUILD_LIBRARY_OPTIONS="--disable-static --enable-shared"
  SHARED_LIBRARY_OPTIONS="--extra-ldsoflags=-sSIDE_MODULE=1"
fi

if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} == "1" ]]; then
  CONFIGURE_POSTFIX+=" --enable-pthreads --disable-w32threads --disable-os2threads"
else
  CONFIGURE_POSTFIX+=" --disable-pthreads --disable-w32threads --disable-os2threads"
fi

# OPTIMIZE FOR SPEED INSTEAD OF SIZE WHEN REQUESTED
if [[ -z ${FFMPEG_KIT_OPTIMIZED_FOR_SPEED} ]]; then
  SIZE_OPTIONS="--enable-small"
else
  SIZE_OPTIONS=""
fi

# SET DEBUG OPTIONS
if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
  DEBUG_OPTIONS="--disable-debug"
else
  DEBUG_OPTIONS="--enable-debug --disable-stripping"
fi

if [[ -n ${STRIP} ]]; then
  CONFIGURE_POSTFIX+=" --strip=${STRIP}"
fi

if [[ -n ${NM} ]]; then
  CONFIGURE_POSTFIX+=" --nm=${NM}"
fi

echo -n -e "\n${LIB_NAME}: "

if [[ -z ${NO_WORKSPACE_CLEANUP_ffmpeg} ]]; then
  echo -e "INFO: Cleaning workspace for ${LIB_NAME}\n" 1>>"${BASEDIR}"/build.log 2>&1
  make distclean 2>/dev/null 1>/dev/null

  git checkout "${BASEDIR}/src/ffmpeg/ffbuild" 1>>"${BASEDIR}"/build.log 2>&1
fi

########################### CUSTOMIZATIONS #######################
cd "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
git checkout configure 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavformat/file.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavformat/hls.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavformat/protocols.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavutil 1>>"${BASEDIR}"/build.log 2>&1

# 0. Link theora through the unified libtheora archive. libtheoraenc.a and
# libtheoradec.a each embed their own copy of theora's common objects
# (apiwrapper.o, fragment.o, ...), and side modules whole-include static
# archives, so linking both duplicates every common symbol in libavcodec.so.
# The legacy combined libtheora.a contains the full th_* API exactly once.
${SED_INLINE} 's|-ltheoraenc -ltheoradec|-ltheora|g' configure 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# 1. Use thread local log levels
${SED_INLINE} 's/static atomic_int av_log_level/__thread atomic_int av_log_level/g' "${BASEDIR}"/src/"${LIB_NAME}"/libavutil/log.c 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# 2. Enable ffmpeg-kit protocols
if [[ ${NO_FFMPEG_KIT_PROTOCOLS} == "1" ]]; then
  echo -e "\nINFO: Disabled custom ffmpeg-kit protocols\n" 1>>"${BASEDIR}"/build.log 2>&1
else
  cat ../../tools/protocols/libavformat_file_ffkitmem_stream.c >> libavformat/file.c
  cat ../../tools/protocols/libavutil_file_h.inc >> libavutil/file.h
  cat ../../tools/protocols/libavutil_file_c.inc >> libavutil/file.c
  awk '{gsub(/ff_file_protocol;/,"ff_file_protocol;\nextern const URLProtocol ff_ffkitmem_protocol;\nextern const URLProtocol ff_ffkitstream_protocol;")}1' libavformat/protocols.c > libavformat/protocols.c.tmp
  cat libavformat/protocols.c.tmp > libavformat/protocols.c
  ${SED_INLINE} "s|av_strstart(proto_name, \"file\", NULL))|av_strstart(proto_name, \"file\", NULL) \|\| av_strstart(proto_name, \"ffkitmem\", NULL) \|\| av_strstart(proto_name, \"ffkitstream\", NULL))|g" libavformat/hls.c 1>>"${BASEDIR}"/build.log 2>&1
  echo -e "\nINFO: Enabled custom ffmpeg-kit protocols\n" 1>>"${BASEDIR}"/build.log 2>&1
  "${BASEDIR}/scripts/web/ffmpeg-kit-protocols-test.sh" "${BASEDIR}" "${BASEDIR}/src/${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

###################################################################

emconfigure ./configure \
  --prefix="${FFMPEG_LIBRARY_PATH}" \
  --pkg-config="${HOST_PKG_CONFIG_PATH}" \
  --pkg-config-flags=--static \
  --enable-version3 \
  --arch="${TARGET_ARCH}" \
  --cpu="${TARGET_CPU}" \
  --target-os=none \
  ${ASM_OPTIONS} \
  --ar="${AR}" \
  --cc="${CC}" \
  --cxx="${CXX}" \
  --dep-cc="${CC}" \
  --objcc="${CC}" \
  --ranlib="${RANLIB}" \
  --disable-autodetect \
  --enable-cross-compile \
  --enable-pic \
  --disable-symver \
  ${SHARED_LIBRARY_OPTIONS} \
  --enable-optimizations \
  --enable-swscale \
  ${BUILD_LIBRARY_OPTIONS} \
  --disable-runtime-cpudetect \
  --disable-stripping \
  ${SIZE_OPTIONS} \
  ${DEBUG_OPTIONS} \
  --disable-programs \
  --disable-doc \
  --disable-htmlpages \
  --disable-manpages \
  --disable-podpages \
  --disable-txtpages \
  --disable-sndio \
  --disable-schannel \
  --disable-securetransport \
  --disable-xlib \
  --disable-cuda \
  --disable-cuvid \
  --disable-nvenc \
  --disable-vaapi \
  --disable-vdpau \
  --disable-videotoolbox \
  --disable-audiotoolbox \
  --disable-appkit \
  --disable-v4l2-m2m \
  ${CONFIGURE_POSTFIX} 1>>"${BASEDIR}"/build.log 2>&1

if [[ $? -ne 0 ]]; then
  exit 1
fi

# FFmpeg appends -l<lib> once per detected codec, so e.g. -lvpx lands in
# EXTRALIBS-avcodec four times (vp8/vp9 decoder+encoder). Native linkers ignore
# the repeats (lazy archive inclusion), but the web build links FFmpeg libraries
# as -sSIDE_MODULE, which whole-includes static archives, so a repeated -l<lib>
# pulls the same objects multiple times -> duplicate symbols.
#
# Deduplicate the -l<archive> tokens GLOBALLY across all EXTRALIBS lines, keeping
# each on the first line it appears. FFmpeg links each shared lib against the
# combined EXTRALIBS of itself and its dependencies (FFEXTRALIBS), so a token in
# both EXTRALIBS-avcodec and EXTRALIBS-avfilter (e.g. -lwebpmux, pulled via the
# tesseract -> leptonica -> libwebp chain) would whole-include the same archive
# twice in libavfilter.so. Since every side module is combined into the main
# module, each static archive only needs to be whole-included once anywhere; an
# undefined reference in another side module resolves at the main-module link.
# System libs (-lm) and -L search paths are left per-line (harmless when repeated).
# -lz (pulled transitively via libpng's Libs.private) is dropped entirely:
# emscripten gives side modules no system libraries or ports, so -lz cannot
# resolve there. The zlib port is linked into the main module instead (see
# create_web_main_module), which exports its symbols to the side modules.
if [[ -f ffbuild/config.mak ]]; then
  awk '
    /^EXTRALIBS/ {
      eq = index($0, "=")
      n = split(substr($0, eq + 1), tok, " ")
      out = ""
      delete lineseen
      for (i = 1; i <= n; i++) {
        t = tok[i]
        if (t == "" || t == "-lz") continue
        if (t !~ /^-l/ || t == "-lm" || t == "-lpthread") {
          # non-library flags (-L, -s..., -pthread) and system libs: per-line dedup
          if (t in lineseen) continue
          lineseen[t] = 1
        } else {
          # our static-archive -l<name>: global dedup across all EXTRALIBS lines
          if (t in globalseen) continue
          globalseen[t] = 1
          lineseen[t] = 1
        }
        out = (out == "" ? t : out " " t)
      }
      print substr($0, 1, eq) out
      next
    }
    { print }
  ' ffbuild/config.mak >ffbuild/config.mak.dedup && mv ffbuild/config.mak.dedup ffbuild/config.mak || exit 1
fi

if [[ -z ${NO_OUTPUT_REDIRECTION} ]]; then
  emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1

  if [[ $? -ne 0 ]]; then
    exit 1
  fi
else
  echo -e "started\n"
  emmake make -j$(get_cpu_count)

  echo -n -e "\n${LIB_NAME}: "
  if [[ $? -ne 0 ]]; then
    exit 1
  fi
fi

# DELETE THE PREVIOUS BUILD OF THE LIBRARY BEFORE INSTALLING
if [ -d "${FFMPEG_LIBRARY_PATH}" ]; then
  rm -rf "${FFMPEG_LIBRARY_PATH}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi
emmake make install 1>>"${BASEDIR}"/build.log 2>&1

if [[ $? -ne 0 ]]; then
  exit 1
fi

# MANUALLY COPY PKG-CONFIG FILES
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavformat.pc "${INSTALL_PKG_CONFIG_DIR}/libavformat.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libswresample.pc "${INSTALL_PKG_CONFIG_DIR}/libswresample.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libswscale.pc "${INSTALL_PKG_CONFIG_DIR}/libswscale.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavdevice.pc "${INSTALL_PKG_CONFIG_DIR}/libavdevice.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavfilter.pc "${INSTALL_PKG_CONFIG_DIR}/libavfilter.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavcodec.pc "${INSTALL_PKG_CONFIG_DIR}/libavcodec.pc" || return 1
overwrite_file "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavutil.pc "${INSTALL_PKG_CONFIG_DIR}/libavutil.pc" || return 1

# MANUALLY ADD REQUIRED HEADERS
mkdir -p "${FFMPEG_LIBRARY_PATH}"/include 1>>"${BASEDIR}"/build.log 2>&1
overwrite_file "${BASEDIR}"/src/ffmpeg/config.h "${FFMPEG_LIBRARY_PATH}"/include/config.h 1>>"${BASEDIR}"/build.log 2>&1
rsync -am --include='*.h' --include='*/' --exclude='*' "${BASEDIR}"/src/ffmpeg/ "${FFMPEG_LIBRARY_PATH}"/include/ 1>>"${BASEDIR}"/build.log 2>&1

if [ $? -eq 0 ]; then
  echo "ok"
else
  exit 1
fi
