#!/bin/bash

mkdir -p "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emcmake cmake "${BASEDIR}/src/${LIB_NAME}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DBUILD_SHARED_LIBS=OFF \
  -DWEBP_ENABLE_SIMD=OFF \
  -DWEBP_BUILD_LIBWEBPMUX=ON \
  -DWEBP_BUILD_CWEBP=OFF \
  -DWEBP_BUILD_DWEBP=OFF \
  -DWEBP_BUILD_GIF2WEBP=OFF \
  -DWEBP_BUILD_IMG2WEBP=OFF \
  -DWEBP_BUILD_VWEBP=OFF \
  -DWEBP_BUILD_WEBPINFO=OFF \
  -DWEBP_BUILD_WEBPMUX=OFF \
  -DWEBP_BUILD_EXTRAS=OFF \
  -DWEBP_BUILD_ANIM_UTILS=OFF 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${LIB_INSTALL_PREFIX}"/lib/pkgconfig/*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
