#!/bin/bash

# SET BUILD OPTIONS
if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} == "1" ]]; then
  THREAD_OPTIONS="--enable-multithread"
else
  THREAD_OPTIONS="--disable-multithread"
fi

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# NOTE THAT RECONFIGURE IS NOT SUPPORTED

# UNDO WORKAROUNDS
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/build/make/configure.sh 1>>"${BASEDIR}"/build.log 2>&1

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --target=generic-gnu \
  --extra-cflags="${CFLAGS}" \
  --extra-cxxflags="${CXXFLAGS}" \
  --log=yes \
  --enable-libs \
  --enable-install-libs \
  --enable-pic \
  --enable-optimizations \
  --enable-vp9-highbitdepth \
  --enable-vp8 \
  --enable-vp9 \
  ${THREAD_OPTIONS} \
  --enable-spatial-resampling \
  --enable-small \
  --enable-static \
  --disable-realtime-only \
  --disable-shared \
  --disable-runtime-cpu-detect \
  --disable-debug \
  --disable-gprof \
  --disable-gcov \
  --disable-ccache \
  --disable-install-bins \
  --disable-install-srcs \
  --disable-install-docs \
  --disable-docs \
  --disable-tools \
  --disable-examples \
  --disable-unit-tests \
  --disable-decode-perf-tests \
  --disable-encode-perf-tests \
  --disable-codec-srcs \
  --disable-debug-libs \
  --disable-internal-stats || return 1

emmake make -j$(get_cpu_count) || return 1

emmake make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
