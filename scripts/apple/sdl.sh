#!/bin/bash

# SET BUILD OPTIONS
SDL_CMAKE_OPTIONS=()
case ${ARCH} in
*-mac-catalyst)
  SDL_CMAKE_OPTIONS+=("-DSDL_OPENGLES=OFF")
  ;;
*)
  SDL_CMAKE_OPTIONS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=$(get_min_sdk_version)")
  if [[ ${FFMPEG_KIT_BUILD_TYPE} == "ios" ]]; then
    SDL_CMAKE_OPTIONS+=("-DSDL_OPENGLES=ON")
  else
    SDL_CMAKE_OPTIONS+=("-DSDL_OPENGLES=OFF")
  fi
  ;;
esac

case ${ARCH} in
arm64e)
  SDL_CMAKE_OSX_ARCHITECTURES=""
  ;;
*)
  SDL_CMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)"
  ;;
esac

SDL_SOURCE_DIR="${BASEDIR}/src/${LIB_NAME}"
SDL_PATCH_DIR=$(get_library_patch_dir "${LIB_NAME}") || return 1

apply_sdl_patch() {
  local PATCH_FILE="${SDL_PATCH_DIR}/$1"
  if git -C "${SDL_SOURCE_DIR}" apply --check "${PATCH_FILE}" 2>/dev/null; then
    git -C "${SDL_SOURCE_DIR}" apply "${PATCH_FILE}" || return 1
  elif git -C "${SDL_SOURCE_DIR}" apply --reverse --check "${PATCH_FILE}" 2>/dev/null; then
    echo "INFO: skipping sdl $1, already applied"
  else
    echo "ERROR: sdl $1 does not apply cleanly" >&2
    return 1
  fi
}

apply_sdl_patch "cmake.mac-catalyst.patch" || return 1
apply_sdl_patch "uikit.visionos-availability.patch" || return 1
apply_sdl_patch "uikit.disabled-joystick.patch" || return 1

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

cmake -Wno-dev \
  -S "${SDL_SOURCE_DIR}" \
  -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
  -DCMAKE_OSX_SYSROOT="${SDK_PATH}" \
  -DCMAKE_OSX_ARCHITECTURES="${SDL_CMAKE_OSX_ARCHITECTURES}" \
  -DCMAKE_C_COMPILER="$(command -v "${CC}")" \
  -DCMAKE_CXX_COMPILER="$(command -v "${CXX}")" \
  -DCMAKE_OBJC_COMPILER="${OBJC}" \
  -DCMAKE_AR="${AR}" \
  -DCMAKE_RANLIB="${RANLIB}" \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_OBJC_FLAGS="${CFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
  -DSDL_SHARED=OFF \
  -DSDL_STATIC=ON \
  -DSDL_INSTALL=ON \
  -DSDL_INSTALL_CPACK=OFF \
  -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DSDL_AUDIO=ON \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_METAL=ON \
  -DSDL_RENDER_METAL=ON \
  -DSDL_GPU=ON \
  -DSDL_OPENGL=OFF \
  -DSDL_JOYSTICK=OFF \
  -DSDL_HAPTIC=OFF \
  -DSDL_HIDAPI=OFF \
  -DSDL_SENSOR=OFF \
  -DSDL_CAMERA=OFF \
  -DSDL_DIALOG=OFF \
  -DSDL_TRAY=OFF \
  -DSDL_DISKAUDIO=OFF \
  "${SDL_CMAKE_OPTIONS[@]}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

cp "${LIB_INSTALL_PREFIX}/lib/pkgconfig/sdl3.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1
