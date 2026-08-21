#!/bin/bash

display_help() {
  local COMMAND=$(echo "$0" | sed -e 's/\.\///g')

  echo -e "\n'$COMMAND' builds FFmpegKitNext for the Windows platform using the MinGW-w64 toolchain \
under MSYS2. Windows libraries are compiled natively, therefore only the architecture of the host \
machine (arm64 or x86-64) is built, without any external libraries enabled. Options can be used to \
enable external libraries. Please note that GPL libraries (external libraries with GPL license) need \
--enable-gpl flag to be set explicitly. arm64 (aarch64) is the primary target and requires an arm64 \
host. When compilation ends, libraries are created under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND [OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "      --jobs=N\t\t\tnumber of jobs to run [auto]" "      --package-name=name\t\tset FFmpegKit package name in the native build [empty]" "      --no-ffmpeg-kit-protocols\t\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]" "      --no-static-mingw-runtime\t\tship libc++ and libwinpthread as dlls in the bundle instead of linking them statically [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --disable-arm64\t\tdo not build arm64 architecture [yes, on arm64 hosts]"
  echo -e "  --disable-x86-64\t\tdo not build x86-64 architecture [yes, on x86-64 hosts]\n"

  echo -e "Libraries:"
  echo -e "  --full\t\t\tenables all external libraries"
  echo -e "  --enable-zlib\t\t\tbuild zlib from source [no]"
  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  display_help_advanced_options
}
