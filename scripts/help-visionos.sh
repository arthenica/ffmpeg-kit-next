#!/bin/bash

display_help() {
  COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""
  local PROFILE_OPTION=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
    PROFILE_OPTION="  -p, --profile PROFILE\t\tnix develop profile to use\n      --list-profiles\t\tlist local nix develop profiles"
  fi

  echo -e "\n'$COMMAND' builds FFmpegKit for visionOS platform. By default two architectures (arm64, arm64-simulator) \
are enabled without any external libraries. Options can be used to disable architectures and/or enable \
external libraries. Please note that GPL libraries (external libraries with GPL license) need --enable-gpl flag to be \
set explicitly. When compilation ends, libraries are created under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "  -x, --xcframework\t\tbuild xcframework bundles instead of framework bundles" "      --spm\t\t\tcreate a local Swift package (Package.swift) next to xcframeworks; requires -x" "      --jobs=N\t\t\tnumber of jobs to run [auto]" "      --target=visionos sdk version\toverride minimum deployment target [1.0]" "      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"

  echo -e "  --disable-arm64\t\tdo not build arm64 architecture [yes]"
  echo -e "  --disable-arm64-simulator\tdo not build arm64-simulator architecture [yes]\n"

  echo -e "Libraries:"
  echo -e "  --full\t\t\tenables all non-GPL external libraries"
  echo -e "  --enable-visionos-audiotoolbox\tbuild with built-in Apple AudioToolbox support [no]"
  echo -e "  --enable-visionos-bzip2\t\tbuild with built-in bzip2 support [no]"
  echo -e "  --enable-visionos-videotoolbox\tbuild with built-in Apple VideoToolbox support [no]"
  echo -e "  --enable-visionos-zlib\t\tbuild with built-in zlib [no]"
  echo -e "  --enable-visionos-libiconv\tbuild with built-in libiconv [no]"

  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  if [[ -n ${FFMPEG_KIT_XCF_BUILD} ]]; then
    display_help_advanced_options "  --no-framework\t\tdo not build xcframework bundles [no]"
  else
    display_help_advanced_options "  --no-framework\t\tdo not build framework bundles [no]"
  fi
}
