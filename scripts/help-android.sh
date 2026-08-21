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
select architectures and/or enable external libraries. Please note that GPL libraries (external libraries with GPL \
license) need --enable-gpl flag to be set explicitly. When compilation ends an Android Archive (AAR) file is created \
under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]... [VAR=VALUE]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "      --jobs=N\t\t\tnumber of jobs to run [auto]\n      --api-level=api\t\toverride Android api level [24]\n      --toolchain=path\t\toverride the default (llvm) toolchain path\n      --package-name=name\t\tset FFmpegKit package name in the native build [empty]\n      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitsaf, ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --arch=ARCH[,ARCH...]\t\tbuild only the listed architectures [default]"
  echo -e "  --disable-arch-arm-v7a\tdo not build arm-v7a architecture [yes]"
  echo -e "  --disable-arch-arm-v7a-neon\tdo not build arm-v7a-neon architecture [yes]"
  echo -e "  --disable-arch-arm64-v8a\tdo not build arm64-v8a architecture [yes]"
  echo -e "  --disable-arch-x86\t\tdo not build x86 architecture [yes]"
  echo -e "  --disable-arch-x86-64\tdo not build x86-64 architecture [yes]\n"

  echo -e "Libraries:"
  echo -e "  --enable-lib-all\t\tenable all libraries allowed by the selected license policy"
  echo -e "  --enable-lib-android-media-codec\tbuild with built-in Android MediaCodec support [no]"
  echo -e "  --enable-lib-android-zlib\tbuild with built-in zlib support [no]"

  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  display_help_advanced_options "  --no-archive\t\t\tdo not build Android archive [no]\n  --prefab\t\t\tadd a prefab payload to the AAR for native/CMake consumers [no]"
}
