#!/bin/bash

display_help() {
  local COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""
  local PROFILE_OPTION=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
    PROFILE_OPTION="  -p, --profile PROFILE\t\tnix develop profile to use\n      --list-profiles\t\tlist local nix develop profiles"
  fi

  echo -e "\n'$COMMAND' builds FFmpegKit for Android platform. By default five Android architectures (armeabi-v7a, \
armeabi-v7a-neon, arm64-v8a, x86 and x86_64) are built without any external libraries enabled. Options can be used to \
disable architectures and/or enable external libraries. Please note that GPL libraries (external libraries with GPL \
license) need --enable-gpl flag to be set explicitly. When compilation ends an Android Archive (AAR) file is created \
under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]... [VAR=VALUE]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "      --jobs=N\t\t\tnumber of jobs to run [auto]\n      --api-level=api\t\toverride Android api level [24]\n      --toolchain=path\t\toverride the default (llvm) toolchain path\n      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitsaf, ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --disable-arm-v7a\t\tdo not build arm-v7a architecture [yes]"
  echo -e "  --disable-arm-v7a-neon\tdo not build arm-v7a-neon architecture [yes]"
  echo -e "  --disable-arm64-v8a\t\tdo not build arm64-v8a architecture [yes]"
  echo -e "  --disable-x86\t\t\tdo not build x86 architecture [yes]"
  echo -e "  --disable-x86-64\t\tdo not build x86-64 architecture [yes]\n"

  echo -e "Libraries:"
  echo -e "  --full\t\t\tenables all external libraries"
  echo -e "  --enable-android-media-codec\tbuild with built-in Android MediaCodec support [no]"
  echo -e "  --enable-android-zlib\t\tbuild with built-in zlib support [no]"

  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  display_help_advanced_options "  --no-archive\t\t\tdo not build Android archive [no]"
}
