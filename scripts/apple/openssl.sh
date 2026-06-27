#!/bin/bash

# DETECT __uint128_t AVAILABILITY (PREREQUISITE FOR enable-ec_nistp_64_gcc_128)
INT128_AVAILABLE=$($CC -dM -E - </dev/null 2>>"${BASEDIR}"/build.log | grep __SIZEOF_INT128__)

echo -e "INFO: __uint128_t detection output: $INT128_AVAILABLE\n" 1>>"${BASEDIR}"/build.log 2>&1

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
arm64 | arm64e | arm64-mac-catalyst | arm64-simulator | x86-64 | x86-64-mac-catalyst)
  # 64-BIT NIST-EC OPTIMIZATION REQUIRES __uint128_t SUPPORT
  if [[ -n ${INT128_AVAILABLE} ]]; then
    ASM_OPTIONS="enable-ec_nistp_64_gcc_128"
  fi
  ;;
i386)
  ASM_OPTIONS="386"
  ;;
esac

# SELECT THE MOST ACCURATE OpenSSL CONFIGURE TARGET PER PLATFORM AND ARCH
#
# NOTE: OpenSSL 3.5.7 has no dedicated tvOS or mac-catalyst target. For those
# slices we fall back to the generic iphoneos-cross cross target; the real
# arch, sysroot and minimum-version come from ffmpeg-kit's CC/CFLAGS, so the
# target only needs to supply the correct asm/bn settings. Configure honors
# env('CC'), so the CC defined inside the xcrun targets is overridden by the
# CC ffmpeg-kit exports. The target's -arch always matches the slice arch to
# avoid -arch conflicts with the -arch ffmpeg-kit already injects via CFLAGS.
OPENSSL_TARGET=""
case ${FFMPEG_KIT_BUILD_TYPE} in
macos)
  case ${ARCH} in
  arm64)
    OPENSSL_TARGET="darwin64-arm64"
    ;;
  x86-64)
    OPENSSL_TARGET="darwin64-x86_64"
    ;;
  *)
    OPENSSL_TARGET="iphoneos-cross"
    ;;
  esac
  ;;
ios)
  case ${ARCH} in
  armv7 | armv7s)
    OPENSSL_TARGET="ios-cross"
    ;;
  arm64 | arm64e)
    OPENSSL_TARGET="ios64-cross"
    ;;
  arm64-simulator)
    OPENSSL_TARGET="iossimulator-arm64-xcrun"
    ;;
  x86-64)
    OPENSSL_TARGET="iossimulator-x86_64-xcrun"
    ;;
  i386)
    OPENSSL_TARGET="iossimulator-i386-xcrun"
    ;;
  *)
    OPENSSL_TARGET="iphoneos-cross"
    ;;
  esac
  ;;
*)
  # tvos and mac-catalyst: no dedicated OpenSSL target exists
  OPENSSL_TARGET="iphoneos-cross"
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_openssl} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./Configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  zlib \
  no-shared \
  no-engine \
  no-dso \
  no-legacy \
  no-apps \
  ${ASM_OPTIONS} \
  no-tests \
  "${OPENSSL_TARGET}" || return 1

make -j$(get_cpu_count) build_sw || return 1

make install_sw install_ssldirs || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
