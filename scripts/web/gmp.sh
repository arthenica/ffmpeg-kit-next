#!/bin/bash

# SET BUILD OPTIONS
if [[ -z ${GMP_CC_FOR_BUILD} ]]; then
  if [[ -x /usr/bin/cc ]]; then
    GMP_CC_FOR_BUILD="/usr/bin/cc"
  else
    GMP_CC_FOR_BUILD="$(command -v cc || command -v clang || command -v gcc)"
  fi
fi
if [[ -z ${GMP_CC_FOR_BUILD} ]]; then
  echo -e "\nERROR: Native C compiler not found for GMP build-time generators\n" 1>>"${BASEDIR}"/build.log 2>&1
  return 1
fi

# gmp HAS NO OPTION TO SKIP ITS MANUAL. IT DOES SHIP doc/gmp.info, BUT git DOES
# NOT PRESERVE TIMESTAMPS, SO AFTER A CLONE gmp.texi IS OFTEN A FEW MILLISECONDS
# NEWER THAN gmp.info AND make REGENERATES THE MANUAL. THAT NEEDS makeinfo,
# WHICH IS NOT ALWAYS INSTALLED. POINTING MAKEINFO AT true SKIPS THE
# REGENERATION AND LEAVES THE LIBRARY ITSELF UNAFFECTED.
export MAKEINFO=true

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_gmp} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

# UPDATE CONFIG FILES TO SUPPORT wasm32-unknown-emscripten
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.guess "${BASEDIR}"/src/"${LIB_NAME}"/config.guess 1>>"${BASEDIR}"/build.log 2>&1 || return 1
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.sub "${BASEDIR}"/src/"${LIB_NAME}"/config.sub 1>>"${BASEDIR}"/build.log 2>&1 || return 1

ABI=standard emconfigure env \
  CC_FOR_BUILD="${GMP_CC_FOR_BUILD}" \
  HOST_CC="${GMP_CC_FOR_BUILD}" \
  CPP_FOR_BUILD="${GMP_CC_FOR_BUILD} -E" \
  ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-assembly \
  --disable-shared \
  --disable-fast-install \
  --disable-maintainer-mode \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MAKEINFO IS ALSO PASSED ON THE COMMAND LINE SO IT OVERRIDES THE VALUE BAKED
# INTO THE GENERATED Makefiles IN EVERY RECURSIVE SUBMAKE
emmake make -j$(get_cpu_count) MAKEINFO=true 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install MAKEINFO=true 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_gmp_package_config "6.3.0" || return 1
