#!/bin/bash

display_help() {
  COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""
  local PROFILE_OPTION=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
    PROFILE_OPTION="  -p, --profile PROFILE\t\tnix develop profile to use\n      --list-profiles\t\tlist local nix develop profiles"
  fi

  echo -e "\n'$COMMAND' builds FFmpegKit for macOS platform. By default two architectures (arm64 and x86-64) are \
enabled without any external libraries. Options can be used to disable architectures and/or enable external libraries. \
Please note that GPL libraries (external libraries with GPL license) need --enable-gpl flag to be set explicitly. \
When compilation ends, libraries are created under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "  -x, --xcframework\t\tbuild xcframework bundles instead of framework bundles" "      --spm\t\t\t\tcreate a local Swift package (Package.swift) next to xcframeworks; requires -x" "      --jobs=N\t\t\t\tnumber of jobs to run [auto]" "      --target=macos sdk version\toverride minimum deployment target [10.15]" "      --no-ffmpeg-kit-protocols\t\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --disable-arm64\t\tdo not build arm64 architecture [yes]"
  echo -e "  --disable-x86-64\t\tdo not build x86-64 architecture [yes]\n"

  echo -e "Libraries:"

  echo -e "  --full\t\t\tenables all non-GPL external libraries"
  echo -e "  --enable-macos-audiotoolbox\tbuild with built-in Apple AudioToolbox support [no]"
  echo -e "  --enable-macos-avfoundation\tbuild with built-in Apple AVFoundation support [no]"
  echo -e "  --enable-macos-bzip2\t\tbuild with built-in bzip2 support [no]"
  echo -e "  --enable-macos-videotoolbox\tbuild with built-in Apple VideoToolbox support [no]"
  echo -e "  --enable-macos-zlib\t\tbuild with built-in zlib [no]"
  echo -e "  --enable-macos-libiconv\tbuild with built-in libiconv [no]"
  echo -e "  --enable-macos-coreimage\tbuild with built-in Apple CoreImage support [no]"
  echo -e "  --enable-macos-opencl\t\tbuild with built-in OpenCL [no]"
  echo -e "  --enable-macos-opengl\t\tbuild with built-in OpenGL [no]"

  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  if [[ -n ${FFMPEG_KIT_XCF_BUILD} ]]; then
    display_help_advanced_options "  --no-framework\t\tdo not build xcframework bundles [no]"
  else
    display_help_advanced_options "  --no-framework\t\tdo not build framework bundles [no]"
  fi
}
