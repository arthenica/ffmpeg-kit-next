#!/bin/bash

# SET BUILD OPTIONS
THREAD_OPTIONS=""
if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} != "1" ]]; then
  THREAD_OPTIONS="no-threads"
fi

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# VERIFY OPENSSL CONFIGURE SCRIPT EXISTS
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/Configure ]]; then
  echo -e "\nERROR: OpenSSL Configure script not found under ${BASEDIR}/src/${LIB_NAME}\n" 1>>"${BASEDIR}"/build.log 2>&1
  return 1
fi

./Configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  zlib \
  no-shared \
  no-engine \
  no-dso \
  no-legacy \
  no-apps \
  no-tests \
  no-asm \
  no-async \
  no-zlib-dynamic \
  no-ui-console \
  ${THREAD_OPTIONS} \
  linux-generic32 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) build_sw 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install_sw install_ssldirs 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./exporters/*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
