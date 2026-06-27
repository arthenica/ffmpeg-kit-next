#!/bin/bash

# SET BUILD OPTIONS
#
# OpenGL ES is disabled on platforms that do not ship the OpenGLES framework
# headers (<OpenGLES/ES2/gl.h>) or where ffmpeg-kit does not need SDL's GL
# backend:
#   - mac-catalyst slices: SDL's configure.ac routes the catalyst host triplet
#     (...-ios-darwin) into the *-ios-* UIKit branch, which would otherwise
#     enable OpenGL ES unconditionally. The configure.ac patch applied below
#     makes those ES defines/framework link honor --disable-video-opengles.
#   - plain macOS desktop slices (FFMPEG_KIT_BUILD_TYPE=macos, arch arm64/x86-64):
#     the *-*-darwin* / CheckMacGLES path honors --disable-video-opengles natively.
#   - tvOS slices: FFmpeg is built with --disable-opengl, and SDL's UIKit OpenGL
#     ES view currently trips Clang on simulator builds once the UIKit driver is
#     correctly enabled by SDL_config.h.in.uikit.patch.
#
# Real iOS device + simulator slices keep OpenGL ES (no flag passed), because
# enable_video_opengles defaults to yes upstream. The arm64/x86-64/... arch tokens
# are shared between iOS, tvOS and macOS builds, so platform disables are gated on
# FFMPEG_KIT_BUILD_TYPE.
ASM_OPTIONS=""
case ${ARCH} in
*-mac-catalyst)
  ASM_OPTIONS="--disable-video-cocoa --disable-render-metal --disable-diskaudio --disable-video-opengles"
  ;;
*)
  if [[ ${FFMPEG_KIT_BUILD_TYPE} == "macos" ]]; then
    ASM_OPTIONS="--disable-video-opengles"
  elif [[ ${FFMPEG_KIT_BUILD_TYPE} == "tvos" ]]; then
    ASM_OPTIONS="--disable-video-opengles"
  fi
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# PATCH SDL SOURCES; configure.ac patches must happen before autogen.sh.
apply_sdl_patch() {
  local PATCH_NAME="$1"
  local PATCH_FILE="${BASEDIR}/tools/patch/make/sdl/${PATCH_NAME}"

  if git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --check "${PATCH_FILE}" 2>/dev/null; then
    git -C "${BASEDIR}"/src/"${LIB_NAME}" apply "${PATCH_FILE}" || return 1
  elif git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --reverse --check "${PATCH_FILE}" 2>/dev/null; then
    echo -e "INFO: skipping sdl ${PATCH_NAME}, already applied\n" 1>>"${BASEDIR}"/build.log 2>&1
  else
    echo -e "ERROR: sdl ${PATCH_NAME} does not apply cleanly\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi
}

apply_sdl_apple_configure_ac_patches() {
  local CONFIGURE_AC="${BASEDIR}/src/${LIB_NAME}/configure.ac"
  local TVOS_PATCH="${BASEDIR}/tools/patch/make/sdl/configure.ac.tvos.patch"
  local OPENGLES_PATCH="${BASEDIR}/tools/patch/make/sdl/configure.ac.opengles.patch"

  if grep -Fq '*-ios-*|*-tvos-*)' "${CONFIGURE_AC}" ||
    grep -Fq 'EXTRA_CFLAGS="$EXTRA_CFLAGS -DGLES_SILENCE_DEPRECATION"' "${CONFIGURE_AC}" ||
    git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --reverse --check "${TVOS_PATCH}" 2>/dev/null ||
    git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --reverse --check "${OPENGLES_PATCH}" 2>/dev/null; then
    echo -e "INFO: skipping sdl Apple configure.ac patches, already applied\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 0
  fi

  apply_sdl_patch "configure.ac.opengles.patch" || return 1
  apply_sdl_patch "configure.ac.tvos.patch" || return 1
}

# Upstream SDL's Autoconf platform switch has an iOS/UIKit branch and a generic
# Darwin/macOS branch, but no tvOS host tuple branch. Without this patch,
# --host=*-tvos-darwin falls through to macOS and tries to build Cocoa/IOKit code
# against the AppleTVOS SDK. Treat the local SDL configure.ac Apple fixes as one
# logical patch state; if either one is present, assume both are present.
apply_sdl_apple_configure_ac_patches || return 1

# SDL's UIKit branch compiles Objective-C sources that use weak properties, but
# upstream only adds ARC flags from the macOS/Darwin branch. iOS keeps OpenGL ES
# enabled, so also silence SDK deprecation warnings from the UIKit GL view.
apply_sdl_patch "configure.ac.uikit-objc-arc.patch" || return 1

# SDL still compiles src/joystick/iphoneos/SDL_mfijoystick.m on the UIKit branch
# when --disable-joystick is used. On tvOS, a remote-rotation helper referenced
# GCController outside SDL_JOYSTICK_MFI, so GameController.h was not imported and
# the build failed against only the forward declaration.
apply_sdl_patch "SDL_mfijoystick.tvos-disabled-joystick.patch" || return 1

# SDL's Autoconf template is missing UIKit-specific macros that configure.ac
# defines on the UIKit branch. Without these, tvOS UIKit objects compile empty
# and libSDL2 keeps unresolved references such as SDL_IsIPad.
apply_sdl_patch "SDL_config.h.in.uikit.patch" || return 1

# ALWAYS REGENERATE BUILD FILES
# (SDL 2.30+ builds from configure.ac; the patch above replaces the old
# configure.in patch flow for Apple platform handling)
./autogen.sh || return 1

# UPDATE CONFIG FILES TO SUPPORT APPLE ARCHITECTURES
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.guess "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.guess || return 1
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.sub "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.sub || return 1

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-sysroot="${SDK_PATH}" \
  --enable-static \
  --disable-shared \
  --disable-video-opengl \
  --disable-video-x11 \
  --disable-joystick \
  --disable-haptic \
  --disable-hidapi \
  --disable-sensor \
  ${ASM_OPTIONS} \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
