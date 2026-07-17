#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libsndfile} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-sqlite \
  --disable-alsa \
  --disable-full-suite \
  --disable-external-libs \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# The default all/install targets depend on generated test sources, which need
# the host autogen tool. The wasm package only needs the library artifacts.
emmake make -j$(get_cpu_count) src/libsndfile.la 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install-libLTLIBRARIES install-includeHEADERS install-pkgconfigDATA 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
