#!/bin/bash

# DISCARD ANY PLATFORM WORKAROUNDS FROM PREVIOUS RUNS
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/build 1>>"${BASEDIR}"/build.log 2>&1
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/codec 1>>"${BASEDIR}"/build.log 2>&1

# ALWAYS CLEAN THE PREVIOUS BUILD
make clean 2>/dev/null 1>/dev/null

# Emscripten is neither __linux__ nor _WIN32, so openh264's include block routes it
# into the BSD path and pulls <sys/sysctl.h>, which Emscripten has no header for.
# Exclude emscripten from that include (only __Fuchsia__ was excluded upstream); the
# CPU-count code already has a dedicated #elif defined(__EMSCRIPTEN__) branch.
for f in codec/common/src/WelsThreadLib.cpp codec/decoder/core/src/wels_decoder_thread.cpp; do
  ${SED_INLINE} 's|^#ifndef __Fuchsia__$|#if !defined(__Fuchsia__) \&\& !defined(__EMSCRIPTEN__)|g' "${BASEDIR}"/src/"${LIB_NAME}"/"$f" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
done

emmake make -j$(get_cpu_count) \
  ARCH="$(get_target_cpu)" \
  AR="${AR}" \
  CC="${CC}" \
  CFLAGS="${CFLAGS}" \
  CXX="${CXX}" \
  CXXFLAGS="${CXXFLAGS}" \
  LDFLAGS="${LDFLAGS}" \
  OS=linux \
  USE_ASM=No \
  PREFIX="${LIB_INSTALL_PREFIX}" \
  install-static 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${BASEDIR}"/src/"${LIB_NAME}"/openh264-static.pc "${INSTALL_PKG_CONFIG_DIR}"/openh264.pc 1>>"${BASEDIR}"/build.log 2>&1 || return 1
