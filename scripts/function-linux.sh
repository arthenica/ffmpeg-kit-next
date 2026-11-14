#!/bin/bash

source "${BASEDIR}/scripts/function.sh"

prepare_inline_sed || exit 1

# LINUX LIBRARIES ARE COMPILED NATIVELY, SO ONLY THE ARCHITECTURE OF THE HOST MACHINE CAN BE BUILT
enable_default_linux_architectures() {
  case $(uname -m) in
  aarch64 | arm64)
    ENABLED_ARCHITECTURES[ARCH_ARM64]=1
    ;;
  *)
    ENABLED_ARCHITECTURES[ARCH_X86_64]=1
    ;;
  esac
}

get_ffmpeg_kit_version() {
  local FFMPEG_KIT_VERSION=$(sed -n 's/.*FFmpegKitVersion = "\([^"]*\)".*/\1/p' "${BASEDIR}/linux/src/FFmpegKitConfig.h" 2>>"${BASEDIR}"/build.log)

  echo "${FFMPEG_KIT_VERSION}"
}

get_pkg_config_libdir() {
  local PKG_CONFIG_LIBDIR_VALUE="${INSTALL_PKG_CONFIG_DIR}"

  if [[ -n ${FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR} ]]; then
    PKG_CONFIG_LIBDIR_VALUE+=":${FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR}"
  fi

  if [[ -n ${FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR} ]]; then
    PKG_CONFIG_LIBDIR_VALUE+=":${FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR}"
  fi

  if [[ -z ${FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR} ]] && [[ -z ${FFMPEG_KIT_NIX_PKG_CONFIG_LIBDIR} ]]; then
    local HOST_PC_PATH
    HOST_PC_PATH=$(pkg-config --variable pc_path pkg-config 2>>"${BASEDIR}"/build.log)
    if [[ -n ${HOST_PC_PATH} ]]; then
      PKG_CONFIG_LIBDIR_VALUE+=":${HOST_PC_PATH}"
    fi
  fi

  echo "${PKG_CONFIG_LIBDIR_VALUE}"
}

set_default_min_linux_platform_version() {
  local _TMP
}

is_linux_pkg_config_build_directory() {
  [[ "$1" == "$2" || "$1" == "$2/"* || "$1" == /nix/store/* ]]
}

filter_linux_pkg_config_private_flags() {
  local INPUT="$1" BUILD_ROOT="$2"
  local RAW="" VALUE="" QUOTE="" CHAR NEXT ACTIVE=0 INDEX
  local -a RAW_FLAGS=() FLAGS=()

  # Parse shell quoting as data. Never evaluate metadata as shell commands.
  for ((INDEX = 0; INDEX < ${#INPUT}; INDEX++)); do
    CHAR="${INPUT:INDEX:1}"
    if [[ -z "$QUOTE" && "$CHAR" == [[:space:]] ]]; then
      if [[ "$ACTIVE" == 1 ]]; then
        RAW_FLAGS+=("$RAW")
        FLAGS+=("$VALUE")
        RAW="" VALUE="" ACTIVE=0
      fi
      continue
    fi
    RAW+="$CHAR"
    ACTIVE=1
    if [[ "$QUOTE" == "'" ]]; then
      if [[ "$CHAR" == "'" ]]; then QUOTE=""; else VALUE+="$CHAR"; fi
    elif [[ "$CHAR" == '"' ]]; then
      if [[ "$QUOTE" == '"' ]]; then QUOTE=""; else QUOTE='"'; fi
    elif [[ -z "$QUOTE" && "$CHAR" == "'" ]]; then
      QUOTE="'"
    elif [[ "$CHAR" == '\' ]]; then
      NEXT="${INPUT:INDEX+1:1}"
      if [[ -z "$NEXT" ]]; then
        printf 'error: trailing escape in Libs.private\n' >&2
        return 1
      fi
      if [[ -z "$QUOTE" || "$NEXT" == [\$\`\"\\] ]]; then
        RAW+="$NEXT"
        VALUE+="$NEXT"
        INDEX=$((INDEX + 1))
      else
        VALUE+="$CHAR"
      fi
    else
      VALUE+="$CHAR"
    fi
  done
  if [[ -n "$QUOTE" ]]; then
    printf 'error: unterminated quote in Libs.private\n' >&2
    return 1
  fi
  if [[ "$ACTIVE" == 1 ]]; then
    RAW_FLAGS+=("$RAW")
    FLAGS+=("$VALUE")
  fi

  printf 'Libs.private:'
  for ((INDEX = 0; INDEX < ${#FLAGS[@]}; INDEX++)); do
    VALUE="${FLAGS[INDEX]}"
    if [[ "$VALUE" == -L && $((INDEX + 1)) -lt ${#FLAGS[@]} ]] &&
      is_linux_pkg_config_build_directory "${FLAGS[INDEX+1]}" "$BUILD_ROOT"; then
      INDEX=$((INDEX + 1))
      continue
    fi
    if [[ "$VALUE" == -L* ]] && is_linux_pkg_config_build_directory "${VALUE:2}" "$BUILD_ROOT"; then
      continue
    fi
    printf ' %s' "${RAW_FLAGS[INDEX]}"
  done
}

rebase_linux_pkg_config_paths() {
  local LINE="$1" BUILD_ROOT="$2" PREFIX BEFORE AFTER RESULT
  for PREFIX in "$BUILD_ROOT/ffmpeg-kit" "$BUILD_ROOT/ffmpeg"; do
    RESULT=""
    while [[ "$LINE" == *"$PREFIX"* ]]; do
      BEFORE="${LINE%%"$PREFIX"*}"
      AFTER="${LINE#*"$PREFIX"}"
      if [[ -z "$AFTER" || "${AFTER:0:1}" == [/[:space:]\"\'] ]]; then
        RESULT+="$BEFORE"'${prefix}'
      else
        RESULT+="$BEFORE$PREFIX"
      fi
      LINE="$AFTER"
    done
    LINE="$RESULT$LINE"
  done
  printf '%s\n' "$LINE"
}

relocate_linux_pkg_config_file() {
  local FILE="$1" BUILD_ROOT="$2" TEMP LINE
  while [[ "$BUILD_ROOT" == */ && "$BUILD_ROOT" != / ]]; do BUILD_ROOT="${BUILD_ROOT%/}"; done
  if [[ "$BUILD_ROOT" != /* || "$BUILD_ROOT" == / ]]; then
    printf 'error: original build directory must be an absolute path\n' >&2
    return 1
  fi
  if [[ ! -f "$FILE" ]]; then
    printf 'error: descriptor not found: %s\n' "$FILE" >&2
    return 1
  fi
  TEMP="$(mktemp "${TMPDIR:-/tmp}/ffmpeg-kit-pkgconfig.XXXXXX")" || return 1
  if ! (
    while IFS= read -r LINE || [[ -n "$LINE" ]]; do
      case "$LINE" in
      prefix=*)
        printf '%s\n' 'prefix=${pcfiledir}/..'
        continue
        ;;
      Libs.private:*)
        # Only shared libraries are shipped. Private search paths refer to the
        # static build inputs; retain library flags and non-build directories.
        LINE="$(filter_linux_pkg_config_private_flags "${LINE#Libs.private:}" "$BUILD_ROOT")" || exit 1
        ;;
      esac
      rebase_linux_pkg_config_paths "$LINE" "$BUILD_ROOT"
    done < "$FILE"
  ) > "$TEMP"; then
    rm -f "$TEMP"
    return 1
  fi
  # Preserve the descriptor's permissions, and leave it intact on parse errors.
  if ! cat "$TEMP" > "$FILE"; then
    rm -f "$TEMP"
    return 1
  fi
  rm -f "$TEMP"
}

install_pkg_config_file() {
  local FILE_NAME="$1"
  local SOURCE="${INSTALL_PKG_CONFIG_DIR}/${FILE_NAME}"
  local DESTINATION="${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}/${FILE_NAME}"

  # DELETE OLD FILE
  rm -f "$DESTINATION" 2>>"${BASEDIR}"/build.log
  if [[ $? -ne 0 ]]; then
    exit 1
  fi

  # INSTALL THE NEW FILE
  cp "$SOURCE" "$DESTINATION" 2>>"${BASEDIR}"/build.log
  if [[ $? -ne 0 ]]; then
    exit 1
  fi

  # REBASE PUBLIC PATHS AND DROP PRIVATE SEARCH PATHS TO STATIC BUILD INPUTS.
  # THE PUBLISHED LINUX BUNDLE CONTAINS SHARED LIBRARIES ONLY.
  relocate_linux_pkg_config_file "$DESTINATION" "${LIB_INSTALL_BASE}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
}

# PRINTS A LINKER FLAG THAT GIVES A SHARED LIBRARY A RUNPATH OF $ORIGIN, SO THE
# BUNDLE'S LIBRARIES FIND EACH OTHER IN THEIR OWN DIRECTORY WITHOUT
# LD_LIBRARY_PATH. $ORIGIN IS PASSED IN A LINKER RESPONSE FILE BECAUSE IT DOES
# NOT SURVIVE FFMPEG'S configure, make AND libtool UNLESS IT IS ESCAPED
# DIFFERENTLY FOR EACH OF THEM
get_origin_runpath_ldflag() {
  local RESPONSE_FILE="${FFMPEG_KIT_TMPDIR}/linux-origin-runpath.rsp"

  mkdir -p "${FFMPEG_KIT_TMPDIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  printf '%s\n' '-rpath' '$ORIGIN' >"${RESPONSE_FILE}" 2>>"${BASEDIR}"/build.log || return 1

  echo "-Wl,@${RESPONSE_FILE}"
}

get_bundle_directory() {
  echo "bundle-linux"
}

create_linux_bundle() {
  set_toolchain_paths ""

  local FFMPEG_KIT_VERSION=$(get_ffmpeg_kit_version)

  local FFMPEG_KIT_BUNDLE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next"
  local FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/include"
  local FFMPEG_KIT_BUNDLE_LIB_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib"
  local FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/pkgconfig"
  local FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/licenses"

  initialize_folder "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY}"

  # COPY HEADERS
  cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/include/* "${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/include" 2>>"${BASEDIR}"/build.log
  cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg/include/* "${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/include" 2>>"${BASEDIR}"/build.log

  # COPY LIBS
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/lib/lib* "${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib" 2>>"${BASEDIR}"/build.log
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg/lib/lib* "${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib" 2>>"${BASEDIR}"/build.log

  install_pkg_config_file "libavformat.pc"
  install_pkg_config_file "libswresample.pc"
  install_pkg_config_file "libswscale.pc"
  install_pkg_config_file "libavdevice.pc"
  install_pkg_config_file "libavfilter.pc"
  install_pkg_config_file "libavcodec.pc"
  install_pkg_config_file "libavutil.pc"
  install_pkg_config_file "ffmpeg-kit-next.pc"

  # COPY EXTERNAL LIBRARY LICENSES
  LICENSE_BASEDIR="${FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY}"
  rm -f "${LICENSE_BASEDIR}"/*.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  for library in $(get_common_library_indexes); do
    if [[ ${ENABLED_LIBRARIES[$library]} -eq 1 ]]; then
      ENABLED_LIBRARY=$(get_library_name ${library} | sed 's/-/_/g')
      LICENSE_FILE="${LICENSE_BASEDIR}/license_${ENABLED_LIBRARY}.txt"

      RC=$(copy_external_library_license_file ${library} "${LICENSE_FILE}")

      if [[ ${RC} -ne 0 ]]; then
        echo -e "ERROR: Failed to copy the license file of ${ENABLED_LIBRARY}\n" 1>>"${BASEDIR}"/build.log 2>&1
        exit 1
      fi

      echo -e "DEBUG: Copied the license file of ${ENABLED_LIBRARY} successfully\n" 1>>"${BASEDIR}"/build.log 2>&1

      RC=$(copy_bundled_license_files "$(get_library_name ${library})" "${LICENSE_BASEDIR}" lowercase)

      if [[ ${RC} -ne 0 ]]; then
        echo -e "ERROR: Failed to copy the bundled license files of ${ENABLED_LIBRARY}\n" 1>>"${BASEDIR}"/build.log 2>&1
        exit 1
      fi
    fi
  done

  # COPY CUSTOM LIBRARY LICENSES
  for custom_library_index in "${CUSTOM_LIBRARIES[@]}"; do
    library_name="CUSTOM_LIBRARY_${custom_library_index}_NAME"
    relative_license_path="CUSTOM_LIBRARY_${custom_library_index}_LICENSE_FILE"

    destination_license_path="${LICENSE_BASEDIR}/license_${!library_name}.txt"

    cp "${BASEDIR}/src/${!library_name}/${!relative_license_path}" "${destination_license_path}" 1>>"${BASEDIR}"/build.log 2>&1

    RC=$?

    if [[ ${RC} -ne 0 ]]; then
      echo -e "ERROR: Failed to copy the license file of custom library ${!library_name}\n" 1>>"${BASEDIR}"/build.log 2>&1
      exit 1
    fi

    echo -e "DEBUG: Copied the license file of custom library ${!library_name} successfully\n" 1>>"${BASEDIR}"/build.log 2>&1
  done

  # COPY LIBRARY LICENSES
  if [[ ${GPL_ENABLED} == "yes" ]]; then
    cp "${BASEDIR}"/tools/license/LICENSE.GPLv3 "${LICENSE_BASEDIR}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  else
    cp "${BASEDIR}"/LICENSE "${LICENSE_BASEDIR}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  fi

  cp "${BASEDIR}"/tools/source/SOURCE "${LICENSE_BASEDIR}"/source.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1

  echo -e "DEBUG: Copied the ffmpeg-kit license successfully\n" 1>>"${BASEDIR}"/build.log 2>&1
}

get_cmake_system_processor() {
  case ${ARCH} in
  arm64)
    echo "aarch64"
    ;;
  x86-64)
    echo "x86_64"
    ;;
  esac
}

get_target_cpu() {
  case ${ARCH} in
  arm64)
    echo "aarch64"
    ;;
  x86-64)
    echo "x86_64"
    ;;
  esac
}

get_common_includes() {
  echo "-I${LLVM_CONFIG_INCLUDEDIR:-.}"
}

get_common_cflags() {
  echo "-fstrict-aliasing -fPIC -DLINUX ${LLVM_CONFIG_CFLAGS}"
}

get_arch_specific_cflags() {
  case ${ARCH} in
  arm64)
    echo "-march=armv8-a+crc+crypto -DFFMPEG_KIT_ARM64"
    ;;
  x86-64)
    echo "-march=x86-64 -msse4.2 -mpopcnt -m64 -DFFMPEG_KIT_X86_64"
    ;;
  esac
}

get_size_optimization_cflags() {
  if [[ -z ${NO_LINK_TIME_OPTIMIZATION} ]]; then
    local LINK_TIME_OPTIMIZATION_FLAGS="-flto"
  else
    local LINK_TIME_OPTIMIZATION_FLAGS=""
  fi

  local ARCH_OPTIMIZATION=""
  case ${ARCH} in
  arm64 | x86-64)
    case $1 in
    ffmpeg)
      ARCH_OPTIMIZATION="${LINK_TIME_OPTIMIZATION_FLAGS} -Os -ffunction-sections -fdata-sections"
      ;;
    *)
      ARCH_OPTIMIZATION="-Os -ffunction-sections -fdata-sections"
      ;;
    esac
    ;;
  esac

  local LIB_OPTIMIZATION=""

  echo "${ARCH_OPTIMIZATION} ${LIB_OPTIMIZATION}"
}

get_app_specific_cflags() {
  local APP_FLAGS=""
  case $1 in
  ffmpeg)
    APP_FLAGS="-Wno-unused-function"
    ;;
  ffmpeg-kit)
    APP_FLAGS="-Wno-unused-function -Wno-pointer-sign -Wno-switch -Wno-deprecated-declarations $(get_package_name_cflag)"
    ;;
  kvazaar | libsvtav1)
    APP_FLAGS="-std=gnu99 -Wno-unused-function"
    ;;
  openh264)
    APP_FLAGS="-std=gnu99 -Wno-unused-function -fstack-protector-all"
    ;;
  libplacebo | openssl | sdl | srt)
    APP_FLAGS="-Wno-unused-function"
    ;;
  *)
    APP_FLAGS="-std=c99 -Wno-unused-function"
    ;;
  esac

  echo "${APP_FLAGS}"
}

get_cflags() {
  local ARCH_FLAGS=$(get_arch_specific_cflags)
  local APP_FLAGS=$(get_app_specific_cflags "$1")
  local COMMON_FLAGS=$(get_common_cflags)
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS=$(get_size_optimization_cflags "$1")
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi
  local COMMON_INCLUDES=$(get_common_includes)

  echo "${ARCH_FLAGS} ${APP_FLAGS} ${COMMON_FLAGS} ${OPTIMIZATION_FLAGS} ${COMMON_INCLUDES} ${EXTRA_CFLAGS}"
}

get_cxxflags() {
  if [[ -z ${NO_LINK_TIME_OPTIMIZATION} ]]; then
    local LINK_TIME_OPTIMIZATION_FLAGS="-flto"
  else
    local LINK_TIME_OPTIMIZATION_FLAGS=""
  fi

  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS="-Os -ffunction-sections -fdata-sections"
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi

  local BUILD_DATE="-DFFMPEG_KIT_BUILD_DATE=$(date +%Y%m%d 2>>"${BASEDIR}"/build.log)"
  if [[ -z ${NO_FFMPEG_KIT_PROTOCOLS} ]]; then
    local USES_FFMPEG_KIT_PROTOCOLS="-DUSES_FFMPEG_KIT_PROTOCOLS"
  else
    local USES_FFMPEG_KIT_PROTOCOLS=""
  fi
  local COMMON_FLAGS="-stdlib=libstdc++ -std=c++11 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE} $(get_arch_specific_cflags)"

  case $1 in
  ffmpeg)
    if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
      echo "${LINK_TIME_OPTIMIZATION_FLAGS} -stdlib=libstdc++ -std=c++11 -O2 -ffunction-sections -fdata-sections ${EXTRA_CXXFLAGS}"
    else
      echo "${FFMPEG_KIT_DEBUG} -stdlib=libstdc++ -std=c++11 ${EXTRA_CXXFLAGS}"
    fi
    ;;
  ffmpeg-kit)
    echo "-fPIC ${COMMON_FLAGS} $(get_package_name_cflag) ${USES_FFMPEG_KIT_PROTOCOLS}"
    ;;
  libjxl | shaderc)
    echo "-stdlib=libstdc++ -std=c++17 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE} $(get_arch_specific_cflags) -fcxx-exceptions -fPIC"
    ;;
  libplacebo)
    # NO -std, libplacebo SETS ITS OWN C++ STANDARD (cpp_std=c++20)
    echo "-stdlib=libstdc++ ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE} $(get_arch_specific_cflags) -fno-exceptions -fno-rtti"
    ;;
  libsvtav1 | srt | vvenc)
    echo "${COMMON_FLAGS} -fcxx-exceptions -fPIC"
    ;;
  tesseract)
    echo "${COMMON_FLAGS} -fcxx-exceptions -include cstdlib -fPIC"
    ;;
  zimg)
    echo "${COMMON_FLAGS} -fcxx-exceptions -include exception -fPIC"
    ;;
  *)
    echo "${COMMON_FLAGS} -fno-exceptions -fno-rtti"
    ;;
  esac
}

get_common_linked_libraries() {
  local COMMON_LIBRARIES=""

  case $1 in
  chromaprint | ffmpeg-kit | kvazaar | libjxl | shaderc | srt | vvenc | zimg)
    echo "-stdlib=libstdc++ -lstdc++ -lc -lm ${COMMON_LIBRARIES}"
    ;;
  *)
    echo "-lc -lm -ldl ${COMMON_LIBRARIES}"
    ;;
  esac
}

get_size_optimization_ldflags() {
  if [[ -z ${NO_LINK_TIME_OPTIMIZATION} ]]; then
    local LINK_TIME_OPTIMIZATION_FLAGS="-flto"
  else
    local LINK_TIME_OPTIMIZATION_FLAGS=""
  fi

  case ${ARCH} in
  arm64 | x86-64)
    case $1 in
    ffmpeg)
      echo "${LINK_TIME_OPTIMIZATION_FLAGS} -O2 -ffunction-sections -fdata-sections -finline-functions"
      ;;
    *)
      echo "-Os -ffunction-sections -fdata-sections"
      ;;
    esac
    ;;
  esac
}

get_arch_specific_ldflags() {
  case ${ARCH} in
  arm64)
    echo "-march=armv8-a+crc+crypto -Wl,-z,text"
    ;;
  x86-64)
    echo "-march=x86-64 -Wl,-z,text"
    ;;
  esac
}

get_ldflags() {
  local ARCH_FLAGS=$(get_arch_specific_ldflags)
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS="$(get_size_optimization_ldflags "$1")"
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi
  local COMMON_LINKED_LIBS=$(get_common_linked_libraries "$1")

  echo "${ARCH_FLAGS} ${OPTIMIZATION_FLAGS} ${COMMON_LINKED_LIBS} ${EXTRA_LDFLAGS} ${LLVM_CONFIG_LDFLAGS} -Wl,--hash-style=both -fuse-ld=lld"
}

create_mason_cross_file() {
  cat >"$1" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = 'pkg-config'

[properties]
has_function_printf = true

[host_machine]
system = '$(get_meson_target_host_family)'
cpu_family = '$(get_meson_target_cpu_family)'
cpu = '$(get_cmake_system_processor)'
endian = 'little'

[built-in options]
default_library = 'static'
prefix = '${LIB_INSTALL_PREFIX}'
EOF
}

create_chromaprint_package_config() {
  local CHROMAPRINT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libchromaprint.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/chromaprint
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: chromaprint
Description: Audio fingerprint library
URL: http://acoustid.org/chromaprint
Version: ${CHROMAPRINT_VERSION}
Libs: -L\${libdir} -lchromaprint -lstdc++
Cflags: -I\${includedir}
EOF
}

create_ffmpegkit_package_config() {
  local FFMPEGKIT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/ffmpeg-kit-next.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/ffmpeg-kit
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: ffmpeg-kit-next
Description: FFmpeg for applications
Version: ${FFMPEGKIT_VERSION}

Libs: -L\${libdir} -lstdc++ -lffmpegkit -lavutil
Requires: libavdevice, libavfilter, libswscale, libavformat, libavcodec, libswresample, libavutil
Cflags: -I\${includedir}
EOF
}

create_libaom_package_config() {
  local AOM_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/aom.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/libaom
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: aom
Description: AV1 codec library v${AOM_VERSION}.
Version: ${AOM_VERSION}

Requires:
Libs: -L\${libdir} -laom -lm
Cflags: -I\${includedir}
EOF
}

create_libplacebo_package_config() {
  local LIBPLACEBO_VERSION="$1"

  # libplacebo HAS C++ SOURCES, SO IT ALSO NEEDS THE C++ RUNTIME. SHADERC ARRIVES THROUGH Requires
  cat >"${INSTALL_PKG_CONFIG_DIR}/libplacebo.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/libplacebo
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

pl_has_d3d11=0
pl_has_dovi=1
pl_has_gl_proc_addr=1
pl_has_glslang=0
pl_has_lcms=0
pl_has_libdovi=0
pl_has_opengl=0
pl_has_shaderc=1
pl_has_vk_proc_addr=0
pl_has_vulkan=1
pl_has_xxhash=0

Name: libplacebo
Description: Reusable library for GPU-accelerated video/image rendering
Version: ${LIBPLACEBO_VERSION}
Requires: shaderc >= 2019.1
Libs: -L\${libdir} -lplacebo -lm -pthread
Libs.private: -lstdc++
Cflags: -I\${includedir} -DPL_HAVE_PTHREAD -DPTHREAD_HAS_SETCLOCK -pthread -DPL_STATIC
EOF
}

#
# 1. shaderc version
# 2. library directory name (lib or lib64)
#
create_shaderc_package_config() {
  local SHADERC_VERSION="$1"
  local SHADERC_LIB_DIR_NAME="$2"
  local SHADERC_PC_NAME

  # BOTH FILES DESCRIBE THE STATIC BUNDLE OF SHADERC, GLSLANG AND SPIRV-TOOLS (libshaderc_combined.a), WHICH NEEDS THE
  # C++ RUNTIME. libplacebo READS shaderc.pc
  for SHADERC_PC_NAME in shaderc shaderc_combined; do
    cat >"${INSTALL_PKG_CONFIG_DIR}/${SHADERC_PC_NAME}.pc" <<EOF || return 1
prefix=${LIB_INSTALL_BASE}/shaderc
exec_prefix=\${prefix}
libdir=\${exec_prefix}/${SHADERC_LIB_DIR_NAME}
includedir=\${prefix}/include

Name: shaderc
Description: Tools and libraries for Vulkan shader compilation
Version: ${SHADERC_VERSION}
URL: https://github.com/google/shaderc

Libs: -L\${libdir} -lshaderc_combined
Libs.private: -lstdc++ -lpthread -lm
Cflags: -I\${includedir}
EOF
  done
}

create_srt_package_config() {
  local SRT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/srt.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/srt
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: srt
Description: SRT library set
Version: ${SRT_VERSION}

Libs: -L\${libdir} -lsrt
Libs.private: -lc -lm -ldl -lstdc++
Cflags: -I\${includedir} -I\${includedir}/srt
Requires: openssl libcrypto
EOF
}

create_vulkan_package_config() {
  local VULKAN_VERSION="$1"

  # HEADERS ONLY, FFMPEG LOADS libvulkan.so.1 AT RUNTIME
  cat >"${INSTALL_PKG_CONFIG_DIR}/vulkan.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/vulkan-headers
exec_prefix=\${prefix}
includedir=\${prefix}/include

Name: vulkan
Description: Vulkan Headers
Version: ${VULKAN_VERSION}

Cflags: -I\${includedir}
EOF
}

create_zimg_package_config() {
  local ZIMG_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/zimg.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/zimg
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zimg
Description: Scaling, colorspace conversion, and dithering library
Version: ${ZIMG_VERSION}

Libs: -L\${libdir} -lzimg -lstdc++
Cflags: -I\${includedir}
EOF
}

get_build_directory() {
  echo "linux-$(get_target_cpu)"
}

detect_clang_version() {
  for clang_version in 6 .. 20; do
    if [[ $(command_exists "clang-$clang_version") -eq 0 ]]; then
      echo "$clang_version"
      return
    elif [[ $(command_exists "clang-$clang_version.0") -eq 0 ]]; then
      echo "$clang_version.0"
      return
    fi
  done
  echo "none"
}

set_toolchain_paths() {
  HOST=$(get_host)
  CLANG_VERSION=$(detect_clang_version)

  if [[ $CLANG_VERSION != "none" ]]; then
    local CLANG_POSTFIX="-$CLANG_VERSION"
    export LLVM_CONFIG_CFLAGS=$(llvm-config-$CLANG_VERSION --cflags 2>>"${BASEDIR}"/build.log)
    export LLVM_CONFIG_INCLUDEDIR=$(llvm-config-$CLANG_VERSION --includedir 2>>"${BASEDIR}"/build.log)
    export LLVM_CONFIG_LDFLAGS=$(llvm-config-$CLANG_VERSION --ldflags 2>>"${BASEDIR}"/build.log)
  else
    local CLANG_POSTFIX=""
    export LLVM_CONFIG_CFLAGS=$(llvm-config --cflags 2>>"${BASEDIR}"/build.log)
    export LLVM_CONFIG_INCLUDEDIR=$(llvm-config --includedir 2>>"${BASEDIR}"/build.log)
    export LLVM_CONFIG_LDFLAGS=$(llvm-config --ldflags 2>>"${BASEDIR}"/build.log)
  fi

  export CC=$(command -v "clang$CLANG_POSTFIX")
  export CXX=$(command -v "clang++$CLANG_POSTFIX")
  export AS=$(command -v "llvm-as$CLANG_POSTFIX")
  export AR=$(command -v "llvm-ar$CLANG_POSTFIX")
  export LD=$(command -v "ld.lld$CLANG_POSTFIX")
  export RANLIB=$(command -v "llvm-ranlib$CLANG_POSTFIX")
  export STRIP=$(command -v "llvm-strip$CLANG_POSTFIX")
  export NM=$(command -v "llvm-nm$CLANG_POSTFIX")

  # LINUX BUILDS ARE NATIVE, SO THE C COMPILER ALSO BUILDS THE TOOLS THAT RUN ON
  # THE BUILD MACHINE
  set_host_cc "${CC}"

  # ARM64 ASSEMBLY SOURCES ARE GAS .S FILES, WHICH THE C COMPILER ASSEMBLES.
  # LLVM-AS ONLY UNDERSTANDS LLVM IR, SO IT CANNOT BE USED HERE.
  case ${ARCH} in
  arm64)
    export AS="${CC}"
    ;;
  esac

  export INSTALL_PKG_CONFIG_DIR="${BASEDIR}"/prebuilt/$(get_build_directory)/pkgconfig

  if [ ! -d "${INSTALL_PKG_CONFIG_DIR}" ]; then
    mkdir -p "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  fi
}
