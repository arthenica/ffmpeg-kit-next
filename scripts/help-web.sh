#!/bin/bash

display_help() {
  COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""
  local PROFILE_OPTION=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
    PROFILE_OPTION="  -p, --profile PROFILE\t\tnix develop profile to use [web-wasm32-emscripten]\n      --list-profiles\t\tlist local nix develop profiles"
  fi

  echo -e "\n'$COMMAND' builds FFmpegKit for the WebAssembly browser target using Emscripten. \
The default target is wasm32-unknown-emscripten with WebAssembly SIMD enabled. The web C++ \
ffmpeg-kit wrapper currently requires Emscripten pthreads, so browser consumers must serve the \
final application with SharedArrayBuffer-compatible COOP/COEP headers when ffmpeg-kit is built.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "      --jobs=N\t\t\tnumber of jobs to run [auto]" "      --package-name=name\t\tset FFmpegKit package name in the native build [empty]" "      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --arch=ARCH[,ARCH...]\t\tbuild only the listed architectures [default]"
  echo -e "  --disable-arch-wasm32\tdo not build wasm32 architecture [no]\n"

  echo -e "Web options:"
  echo -e "  --static\t\t\tbuild static archives and link one main wasm module [no]"
  echo -e "  --enable-pthreads\t\tbuild with Emscripten pthread support [yes]"
  echo -e "  --disable-pthreads\t\tbuild FFmpeg core without pthread support [no]"
  echo -e "  --enable-relaxed-simd\t\tadd -mrelaxed-simd for experimental optimized builds [no]\n"

  echo -e "Libraries:"

  echo -e "  --enable-lib-all\t\tenable all libraries allowed by the selected license policy"
  echo -e "  --enable-lib-web-iconv\tbuild with system iconv through libiconv compatibility [no]"
  echo -e "  --enable-lib-web-zlib\t\tbuild with built-in zlib [no]"

  display_help_common_libraries
  display_help_gpl_libraries
  display_help_custom_libraries
  display_help_advanced_options
}
