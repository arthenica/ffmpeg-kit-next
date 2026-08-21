#!/bin/bash

# SET BUILD OPTIONS
# opus IMPLEMENTS ARM RUNTIME CPU DETECTION FOR MSVC, linux, APPLE AND FreeBSD
# ONLY. A MinGW BUILD MATCHES NONE OF THOSE, SO celt/arm/armcpu.c FALLS THROUGH
# TO ITS "no CPU detection method available" #error, WHICH ALSO SUGGESTS
# --disable-rtcd. THAT IS THE RIGHT ANSWER HERE: NEON IS MANDATORY ON ARMv8-A,
# SO configure ALREADY SETS OPUS_ARM_PRESUME_AARCH64_NEON_INTR AND SELECTS THE
# NEON PATHS AT COMPILE TIME. DISABLING RTCD DROPS THE REDUNDANT DETECTION CODE
# WITHOUT LOSING NEON. x86-64 KEEPS RTCD, WHERE DETECTION USES cpuid AND BUILDS
# FINE UNDER MinGW.
RTCD_OPTIONS=""
case ${ARCH} in
arm64)
  RTCD_OPTIONS="--disable-rtcd"
  ;;
x86-64)
  RTCD_OPTIONS="--enable-rtcd"
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_opus} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  ${RTCD_OPTIONS} \
  --enable-asm \
  --enable-check-asm \
  --enable-custom-modes \
  --disable-shared \
  --disable-fast-install \
  --disable-maintainer-mode \
  --disable-doc \
  --disable-extra-programs \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./opus.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
