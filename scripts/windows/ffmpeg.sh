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
export LDFLAGS=$(get_ldflags "${LIB_NAME}")
export PKG_CONFIG_LIBDIR="$(get_pkg_config_libdir)"
unset PKG_CONFIG_PATH

# LINK THE MinGW TOOLCHAIN RUNTIME STATICALLY INTO THE FFMPEG DLLs, SO THE FULL
# BUNDLE - WHOSE ffmpeg DLLs PULL libc++ AND libwinpthread THROUGH THE EXTERNAL
# C++ LIBRARIES - DOES NOT DEPEND ON MSYS2 DLLs. -L IS PASSED THROUGH
# --extra-ldflags SO IT LANDS BEFORE THE TOOLCHAIN's OWN LIBRARY DIRECTORY AND
# THE STATIC ARCHIVES STAGED THERE WIN OVER THE DLL IMPORT LIBRARIES.
# --no-static-mingw-runtime LEAVES IT EMPTY, KEEPING THE DLL DEPENDENCIES.
STATIC_MINGW_RUNTIME_LDFLAGS=""
if [[ -z ${NO_STATIC_MINGW_RUNTIME} ]]; then
  STATIC_MINGW_RUNTIME_DIR=$(get_static_mingw_runtime_dir) || return 1
  STATIC_MINGW_RUNTIME_LDFLAGS="--extra-ldflags=-L$(get_native_path "${STATIC_MINGW_RUNTIME_DIR}")"
fi

echo -e "\nINFO: Using PKG_CONFIG_LIBDIR: ${PKG_CONFIG_LIBDIR}\n" 1>>"${BASEDIR}"/build.log 2>&1

cd "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# SET BUILD OPTIONS
TARGET_CPU=""
TARGET_ARCH=""
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  TARGET_CPU="armv8-a"
  TARGET_ARCH="aarch64"
  ASM_OPTIONS="--enable-neon --enable-asm --enable-inline-asm"
  ;;
x86-64)
  TARGET_CPU="x86-64"
  TARGET_ARCH="x86_64"
  ASM_OPTIONS="--disable-neon --enable-asm --enable-inline-asm"
  ;;
esac

CONFIGURE_POSTFIX=""

# SET CONFIGURE OPTIONS
for library in {0..98}; do
  if [[ ${ENABLED_LIBRARIES[$library]} -eq 1 ]]; then
    ENABLED_LIBRARY=$(get_library_name ${library})

    echo -e "INFO: Enabling library ${ENABLED_LIBRARY}\n" 1>>"${BASEDIR}"/build.log 2>&1

    case ${ENABLED_LIBRARY} in
    chromaprint)
      CFLAGS+=" $(pkg-config --cflags libchromaprint 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libchromaprint 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-chromaprint"
      ;;
    dav1d)
      CFLAGS+=" $(pkg-config --cflags dav1d 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static dav1d 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libdav1d"
      ;;
    fontconfig)
      CFLAGS+=" $(pkg-config --cflags fontconfig 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static fontconfig 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfontconfig"
      ;;
    freetype)
      CFLAGS+=" $(pkg-config --cflags freetype2 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static freetype2 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfreetype"
      ;;
    fribidi)
      CFLAGS+=" $(pkg-config --cflags fribidi 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static fribidi 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libfribidi"
      ;;
    gmp)
      CFLAGS+=" $(pkg-config --cflags gmp 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static gmp 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-gmp"
      ;;
    gnutls)
      CFLAGS+=" $(pkg-config --cflags gnutls 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static gnutls 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-gnutls"
      ;;
    harfbuzz)
      CFLAGS+=" $(pkg-config --cflags harfbuzz 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static harfbuzz 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libharfbuzz"
      ;;
    kvazaar)
      CFLAGS+=" $(pkg-config --cflags kvazaar 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static kvazaar 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libkvazaar"
      ;;
    lame)
      CFLAGS+=" $(pkg-config --cflags libmp3lame 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libmp3lame 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libmp3lame"
      ;;
    libaom)
      CFLAGS+=" $(pkg-config --cflags aom 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static aom 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libaom"
      ;;
    libass)
      CFLAGS+=" $(pkg-config --cflags libass 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libass 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libass"
      ;;
    libiconv)
      CFLAGS+=" $(pkg-config --cflags libiconv 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libiconv 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-iconv"
      ;;
    libilbc)
      CFLAGS+=" $(pkg-config --cflags libilbc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libilbc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libilbc"
      ;;
    libjxl)
      CFLAGS+=" $(pkg-config --cflags --static libjxl libjxl_threads 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libjxl libjxl_threads 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libjxl"
      ;;
    liblc3)
      CFLAGS+=" $(pkg-config --cflags lc3 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static lc3 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-liblc3"
      ;;
    libsvtav1)
      CFLAGS+=" $(pkg-config --cflags SvtAv1Enc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static SvtAv1Enc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsvtav1"
      ;;
    libtheora)
      CFLAGS+=" $(pkg-config --cflags theora 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static theora 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtheora"
      ;;
    libvidstab)
      CFLAGS+=" $(pkg-config --cflags vidstab 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static vidstab 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvidstab"
      ;;
    libvorbis)
      CFLAGS+=" $(pkg-config --cflags vorbis 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static vorbis 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvorbis"
      ;;
    libvpx)
      CFLAGS+=" $(pkg-config --cflags vpx 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static vpx 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvpx"
      ;;
    libwebp)
      CFLAGS+=" $(pkg-config --cflags libwebp 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libwebp 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libwebp"
      ;;
    libxml2)
      CFLAGS+=" $(pkg-config --cflags libxml-2.0 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libxml-2.0 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libxml2"
      ;;
    opencore-amr)
      CFLAGS+=" $(pkg-config --cflags opencore-amrnb 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static opencore-amrnb 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopencore-amrnb"
      ;;
    openh264)
      CFLAGS+=" $(pkg-config --cflags openh264 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static openh264 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopenh264"
      ;;
    openssl)
      CFLAGS+=" $(pkg-config --cflags openssl 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static openssl 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-openssl"
      ;;
    opus)
      CFLAGS+=" $(pkg-config --cflags opus 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static opus 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libopus"
      ;;
    rubberband)
      CFLAGS+=" $(pkg-config --cflags rubberband 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static rubberband 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-librubberband"
      ;;
    sdl)
      CFLAGS+=" $(pkg-config --cflags sdl2 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static sdl2 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-sdl2"
      ;;
    shine)
      CFLAGS+=" $(pkg-config --cflags shine 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static shine 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libshine"
      ;;
    snappy)
      CFLAGS+=" $(pkg-config --cflags snappy 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static snappy 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsnappy"
      ;;
    soxr)
      CFLAGS+=" $(pkg-config --cflags soxr 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static soxr 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsoxr"
      ;;
    speex)
      CFLAGS+=" $(pkg-config --cflags speex 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static speex 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libspeex"
      ;;
    srt)
      CFLAGS+=" $(pkg-config --cflags srt 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static srt 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libsrt"
      ;;
    tesseract)
      CFLAGS+=" $(pkg-config --cflags tesseract 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static tesseract 2>>"${BASEDIR}"/build.log)"
      CFLAGS+=" $(pkg-config --cflags giflib 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static giflib 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtesseract"
      ;;
    twolame)
      CFLAGS+=" $(pkg-config --cflags twolame 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static twolame 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libtwolame"
      ;;
    vo-amrwbenc)
      CFLAGS+=" $(pkg-config --cflags vo-amrwbenc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static vo-amrwbenc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvo-amrwbenc"
      ;;
    vvenc)
      CFLAGS+=" $(pkg-config --cflags libvvenc 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libvvenc 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libvvenc"
      ;;
    x264)
      CFLAGS+=" $(pkg-config --cflags x264 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static x264 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libx264"
      ;;
    x265)
      CFLAGS+=" $(pkg-config --cflags x265 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static x265 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libx265"
      ;;
    xvidcore)
      CFLAGS+=" $(pkg-config --cflags xvidcore 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static xvidcore 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libxvid"
      ;;
    zimg)
      CFLAGS+=" $(pkg-config --cflags zimg 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static zimg 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-libzimg"
      ;;
    expat)
      CFLAGS+=" $(pkg-config --cflags expat 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static expat 2>>"${BASEDIR}"/build.log)"
      ;;
    giflib)
      CFLAGS+=" $(pkg-config --cflags giflib 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static giflib 2>>"${BASEDIR}"/build.log)"
      ;;
    jpeg)
      CFLAGS+=" $(pkg-config --cflags libjpeg 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libjpeg 2>>"${BASEDIR}"/build.log)"
      ;;
    leptonica)
      CFLAGS+=" $(pkg-config --cflags lept 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static lept 2>>"${BASEDIR}"/build.log)"
      ;;
    libogg)
      CFLAGS+=" $(pkg-config --cflags ogg 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static ogg 2>>"${BASEDIR}"/build.log)"
      ;;
    libpng)
      CFLAGS+=" $(pkg-config --cflags libpng 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libpng 2>>"${BASEDIR}"/build.log)"
      ;;
    libsamplerate)
      CFLAGS+=" $(pkg-config --cflags samplerate 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static samplerate 2>>"${BASEDIR}"/build.log)"
      ;;
    libsndfile)
      CFLAGS+=" $(pkg-config --cflags sndfile 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static sndfile 2>>"${BASEDIR}"/build.log)"
      ;;
    nettle)
      CFLAGS+=" $(pkg-config --cflags nettle 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static nettle 2>>"${BASEDIR}"/build.log)"
      CFLAGS+=" $(pkg-config --cflags hogweed 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static hogweed 2>>"${BASEDIR}"/build.log)"
      ;;
    tiff)
      CFLAGS+=" $(pkg-config --cflags libtiff-4 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static libtiff-4 2>>"${BASEDIR}"/build.log)"
      ;;
    zlib)
      CFLAGS+=" $(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)"
      LDFLAGS+=" $(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)"
      CONFIGURE_POSTFIX+=" --enable-zlib"
      ;;
    esac
  else

    # THE FOLLOWING LIBRARIES SHOULD BE EXPLICITLY DISABLED TO PREVENT AUTODETECT
    if [[ ${library} -eq ${LIBRARY_ZLIB} ]]; then
      CONFIGURE_POSTFIX+=" --disable-zlib"
    elif [[ ${library} -eq ${LIBRARY_CHROMAPRINT} ]]; then
      CONFIGURE_POSTFIX+=" --disable-chromaprint"
    elif [[ ${library} -eq ${LIBRARY_DAV1D} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libdav1d"
    elif [[ ${library} -eq ${LIBRARY_FONTCONFIG} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libfontconfig"
    elif [[ ${library} -eq ${LIBRARY_FREETYPE} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libfreetype"
    elif [[ ${library} -eq ${LIBRARY_FRIBIDI} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libfribidi"
    elif [[ ${library} -eq ${LIBRARY_GMP} ]]; then
      CONFIGURE_POSTFIX+=" --disable-gmp"
    elif [[ ${library} -eq ${LIBRARY_GNUTLS} ]]; then
      CONFIGURE_POSTFIX+=" --disable-gnutls"
    elif [[ ${library} -eq ${LIBRARY_HARFBUZZ} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libharfbuzz"
    elif [[ ${library} -eq ${LIBRARY_KVAZAAR} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libkvazaar"
    elif [[ ${library} -eq ${LIBRARY_LAME} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libmp3lame"
    elif [[ ${library} -eq ${LIBRARY_LIBAOM} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libaom"
    elif [[ ${library} -eq ${LIBRARY_LIBASS} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libass"
    elif [[ ${library} -eq ${LIBRARY_LIBICONV} ]]; then
      CONFIGURE_POSTFIX+=" --disable-iconv"
    elif [[ ${library} -eq ${LIBRARY_LIBILBC} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libilbc"
    elif [[ ${library} -eq ${LIBRARY_LIBJXL} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libjxl"
    elif [[ ${library} -eq ${LIBRARY_LIBLC3} ]]; then
      CONFIGURE_POSTFIX+=" --disable-liblc3"
    elif [[ ${library} -eq ${LIBRARY_LIBSVTAV1} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libsvtav1"
    elif [[ ${library} -eq ${LIBRARY_LIBTHEORA} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libtheora"
    elif [[ ${library} -eq ${LIBRARY_LIBVIDSTAB} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libvidstab"
    elif [[ ${library} -eq ${LIBRARY_LIBVORBIS} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libvorbis"
    elif [[ ${library} -eq ${LIBRARY_LIBVPX} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libvpx"
    elif [[ ${library} -eq ${LIBRARY_LIBWEBP} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libwebp"
    elif [[ ${library} -eq ${LIBRARY_LIBXML2} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libxml2"
    elif [[ ${library} -eq ${LIBRARY_OPENCOREAMR} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libopencore-amrnb"
    elif [[ ${library} -eq ${LIBRARY_OPENH264} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libopenh264"
    elif [[ ${library} -eq ${LIBRARY_OPENSSL} ]]; then
      CONFIGURE_POSTFIX+=" --disable-openssl"
    elif [[ ${library} -eq ${LIBRARY_OPUS} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libopus"
    elif [[ ${library} -eq ${LIBRARY_RUBBERBAND} ]]; then
      CONFIGURE_POSTFIX+=" --disable-librubberband"
    elif [[ ${library} -eq ${LIBRARY_SDL} ]]; then
      CONFIGURE_POSTFIX+=" --disable-sdl2"
    elif [[ ${library} -eq ${LIBRARY_SHINE} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libshine"
    elif [[ ${library} -eq ${LIBRARY_SNAPPY} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libsnappy"
    elif [[ ${library} -eq ${LIBRARY_SOXR} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libsoxr"
    elif [[ ${library} -eq ${LIBRARY_SPEEX} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libspeex"
    elif [[ ${library} -eq ${LIBRARY_SRT} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libsrt"
    elif [[ ${library} -eq ${LIBRARY_TESSERACT} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libtesseract"
    elif [[ ${library} -eq ${LIBRARY_TWOLAME} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libtwolame"
    elif [[ ${library} -eq ${LIBRARY_VO_AMRWBENC} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libvo-amrwbenc"
    elif [[ ${library} -eq ${LIBRARY_VVENC} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libvvenc"
    elif [[ ${library} -eq ${LIBRARY_X264} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libx264"
    elif [[ ${library} -eq ${LIBRARY_X265} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libx265"
    elif [[ ${library} -eq ${LIBRARY_XVIDCORE} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libxvid"
    elif [[ ${library} -eq ${LIBRARY_ZIMG} ]]; then
      CONFIGURE_POSTFIX+=" --disable-libzimg"
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
  LDFLAGS+=" $(pkg-config --libs --static ${!pc_file_name} 2>>"${BASEDIR}"/build.log)"
  CONFIGURE_POSTFIX+=" --enable-${!ffmpeg_flag_name}"
done

# SET ENABLE GPL FLAG WHEN REQUESTED
if [ "$GPL_ENABLED" == "yes" ]; then
  CONFIGURE_POSTFIX+=" --enable-gpl"
fi

# ALWAYS BUILD SHARED LIBRARIES
BUILD_LIBRARY_OPTIONS="--disable-static --enable-shared"

# OPTIMIZE FOR SPEED INSTEAD OF SIZE
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

echo -n -e "\n${LIB_NAME}: "

if [[ -z ${NO_WORKSPACE_CLEANUP_ffmpeg} ]]; then
  echo -e "INFO: Cleaning workspace for ${LIB_NAME}\n" 1>>"${BASEDIR}"/build.log 2>&1
  make distclean 2>/dev/null 1>/dev/null

  # WORKAROUND TO MANUALLY DELETE UNCLEANED FILES
  rm -f "${BASEDIR}"/src/"${LIB_NAME}"/libavfilter/opencl/*.o 1>>"${BASEDIR}"/build.log 2>&1
  rm -f "${BASEDIR}"/src/"${LIB_NAME}"/libavcodec/neon/*.o 1>>"${BASEDIR}"/build.log 2>&1

  # DELETE SHARED FRAMEWORK WORKAROUNDS
  git checkout "${BASEDIR}/src/ffmpeg/ffbuild" 1>>"${BASEDIR}"/build.log 2>&1
fi

# USE HIGHER LIMITS FOR FFMPEG LINKING
ulimit -n 2048 1>>"${BASEDIR}"/build.log 2>&1

########################### CUSTOMIZATIONS #######################
cd "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
git checkout libavformat/file.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavformat/hls.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavformat/protocols.c 1>>"${BASEDIR}"/build.log 2>&1
git checkout libavutil 1>>"${BASEDIR}"/build.log 2>&1

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
  "${BASEDIR}/scripts/windows/ffmpeg-kit-protocols-test.sh" "${BASEDIR}" "${BASEDIR}/src/${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

###################################################################

./configure \
  --cross-prefix="${HOST}-" \
  --prefix="${FFMPEG_LIBRARY_PATH}" \
  --pkg-config="${HOST_PKG_CONFIG_PATH}" \
  --pkg-config-flags=--static \
  --enable-version3 \
  --arch="${TARGET_ARCH}" \
  --cpu="${TARGET_CPU}" \
  --target-os=mingw32 \
  ${ASM_OPTIONS} \
  --ar="${AR}" \
  --cc="${CC}" \
  --cxx="${CXX}" \
  --host-cc="${CC}" \
  --ranlib="${RANLIB}" \
  --strip="${STRIP}" \
  --nm="${NM}" \
  --windres="${WINDRES}" \
  --disable-autodetect \
  --enable-cross-compile \
  --enable-pic \
  --enable-optimizations \
  --enable-swscale \
  ${BUILD_LIBRARY_OPTIONS} \
  --enable-w32threads \
  ${SIZE_OPTIONS} \
  --disable-xmm-clobber-test \
  ${DEBUG_OPTIONS} \
  --disable-neon-clobber-test \
  --disable-programs \
  --disable-doc \
  --disable-htmlpages \
  --disable-manpages \
  --disable-podpages \
  --disable-txtpages \
  --disable-sndio \
  --disable-schannel \
  --disable-securetransport \
  --disable-cuda \
  --disable-cuvid \
  --disable-nvenc \
  --disable-vaapi \
  --disable-vdpau \
  --disable-videotoolbox \
  --disable-audiotoolbox \
  --disable-appkit \
  ${STATIC_MINGW_RUNTIME_LDFLAGS} \
  ${CONFIGURE_POSTFIX} 1>>"${BASEDIR}"/build.log 2>&1

if [[ $? -ne 0 ]]; then
  exit 1
fi

# MinGW may link fcntl(), but does not provide F_SETFD or FD_CLOEXEC.
${SED_INLINE} 's/^#define HAVE_FCNTL 1$/#define HAVE_FCNTL 0/' \
  "${BASEDIR}/src/ffmpeg/config.h" \
  1>>"${BASEDIR}/build.log" 2>&1 || return 1

# FFMPEG DERIVES dlltool AS "${cross_prefix}dlltool" AND HAS NO CONFIGURE OPTION
# TO OVERRIDE IT. THE MinGW DLL RULE USES $(DLLTOOL) TO CREATE THE IMPORT LIBRARY,
# SO OVERRIDE IT ON THE MAKE COMMAND LINE WITH THE RESOLVED dlltool.
if [[ -z ${NO_OUTPUT_REDIRECTION} ]]; then
  make -j$(get_cpu_count) DLLTOOL="${DLLTOOL}" 1>>"${BASEDIR}"/build.log 2>&1

  if [[ $? -ne 0 ]]; then
    exit 1
  fi
else
  echo -e "started\n"
  make -j$(get_cpu_count) DLLTOOL="${DLLTOOL}"

  echo -n -e "\n${LIB_NAME}: "
  if [[ $? -ne 0 ]]; then
    exit 1
  fi
fi

# DELETE THE PREVIOUS BUILD OF THE LIBRARY BEFORE INSTALLING
if [ -d "${FFMPEG_LIBRARY_PATH}" ]; then
  rm -rf "${FFMPEG_LIBRARY_PATH}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi
make install DLLTOOL="${DLLTOOL}" 1>>"${BASEDIR}"/build.log 2>&1

if [[ $? -ne 0 ]]; then
  exit 1
fi

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavformat.pc "${INSTALL_PKG_CONFIG_DIR}/libavformat.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libswresample.pc "${INSTALL_PKG_CONFIG_DIR}/libswresample.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libswscale.pc "${INSTALL_PKG_CONFIG_DIR}/libswscale.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavdevice.pc "${INSTALL_PKG_CONFIG_DIR}/libavdevice.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavfilter.pc "${INSTALL_PKG_CONFIG_DIR}/libavfilter.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavcodec.pc "${INSTALL_PKG_CONFIG_DIR}/libavcodec.pc" || return 1
copy_and_update_path "${FFMPEG_LIBRARY_PATH}"/lib/pkgconfig/libavutil.pc "${INSTALL_PKG_CONFIG_DIR}/libavutil.pc" || return 1

# MANUALLY ADD REQUIRED HEADERS
mkdir -p "${FFMPEG_LIBRARY_PATH}"/include 1>>"${BASEDIR}"/build.log 2>&1
overwrite_file "${BASEDIR}"/src/ffmpeg/config.h "${FFMPEG_LIBRARY_PATH}"/include/config.h 1>>"${BASEDIR}"/build.log 2>&1
rsync -am --include='*.h' --include='*/' --exclude='*' "${BASEDIR}"/src/ffmpeg/ "${FFMPEG_LIBRARY_PATH}"/include/ 1>>"${BASEDIR}"/build.log 2>&1

if [ $? -eq 0 ]; then
  echo "ok"
else
  exit 1
fi
