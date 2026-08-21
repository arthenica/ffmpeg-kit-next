#!/bin/bash

# UPDATE BUILD FLAGS
export CPPFLAGS="${CPPFLAGS} $(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)" || return 1
export LDFLAGS="${LDFLAGS} $(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_tiff} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-jpeg-include-dir="${LIB_INSTALL_BASE}"/jpeg/include \
  --with-jpeg-lib-dir="${LIB_INSTALL_BASE}"/jpeg/lib \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-maintainer-mode \
  --disable-cxx \
  --disable-libdeflate \
  --disable-jbig \
  --disable-lerc \
  --disable-lzma \
  --disable-zstd \
  --disable-webp \
  --disable-tools \
  --disable-tests \
  --disable-contrib \
  --disable-docs \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
