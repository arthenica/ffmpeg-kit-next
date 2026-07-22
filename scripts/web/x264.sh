#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# PATCH X264 SOURCES
apply_x264_patch() {
  local PATCH_NAME="$1"
  local PATCH_FILE="${BASEDIR}/tools/patch/make/x264/${PATCH_NAME}"

  if git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --check "${PATCH_FILE}" 2>/dev/null; then
    git -C "${BASEDIR}"/src/"${LIB_NAME}" apply "${PATCH_FILE}" || return 1
  elif git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --reverse --check "${PATCH_FILE}" 2>/dev/null; then
    echo -e "INFO: skipping x264 ${PATCH_NAME}, already applied\n" 1>>"${BASEDIR}"/build.log 2>&1
  else
    echo -e "ERROR: x264 ${PATCH_NAME} does not apply cleanly\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi
}

apply_x264_patch "wasm-pthread-signatures.patch" || return 1

# Emscripten's libc has no sched_getaffinity(), which x264_cpu_num_processors()
# calls on the SYS_LINUX path when threads are enabled. Skip that branch so it
# falls through to sysconf(_SC_NPROCESSORS_ONLN), which emscripten implements
# (navigator.hardwareConcurrency). The other SYS_LINUX #elif is PPC-only code.
${SED_INLINE} 's|#elif SYS_LINUX$|#elif SYS_LINUX \&\& !defined(__EMSCRIPTEN__)|g' common/cpu.c 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# THREADS FOLLOW THE WEB PTHREADS SETTING
if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} == "1" ]]; then
  THREAD_OPTIONS=""
else
  THREAD_OPTIONS="--disable-thread"
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --host=wasm32-unknown-linux \
  --enable-pic \
  --enable-static \
  --disable-cli \
  --disable-asm \
  --disable-opencl \
  ${THREAD_OPTIONS} 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp x264.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
