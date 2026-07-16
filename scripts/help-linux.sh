#!/bin/bash

display_help() {
  local COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""
  local PROFILE_OPTION=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
    PROFILE_OPTION="  -p, --profile PROFILE\t\tnix develop profile to use\n      --list-profiles\t\tlist local nix develop profiles"
  fi

  echo -e "\n'$COMMAND' builds FFmpegKit for Linux platform. Linux libraries are compiled natively, \
therefore only the architecture of the host machine (arm64 or x86-64) is built, without any external \
libraries enabled. Options can be used to enable external libraries. Please note that GPL libraries \
(external libraries with GPL license) need --enable-gpl flag to be set explicitly. When compilation ends, \
libraries are created under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "      --jobs=N\t\t\tnumber of jobs to run [auto]" "      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --disable-arm64\t\tdo not build arm64 architecture [yes, on arm64 hosts]"
  echo -e "  --disable-x86-64\t\tdo not build x86-64 architecture [yes, on x86-64 hosts]\n"

  echo -e "Libraries:"
  echo -e "  --full\t\t\tenables all external libraries"
  echo -e "  --enable-linux-alsa\t\tbuild with built-in alsa support [no]"
  echo -e "  --enable-linux-fontconfig\tbuild with built-in fontconfig support [no]"
  echo -e "  --enable-linux-freetype\tbuild with built-in freetype support [no]"
  echo -e "  --enable-linux-fribidi\tbuild with built-in fribidi support [no]"
  echo -e "  --enable-linux-gmp\t\tbuild with built-in gmp support [no]"
  echo -e "  --enable-linux-gnutls\t\tbuild with built-in gnutls support [no]"
  echo -e "  --enable-linux-harfbuzz\tbuild with built-in harfbuzz support [no]"
  echo -e "  --enable-linux-lame\t\tbuild with built-in lame support [no]"
  echo -e "  --enable-linux-libass\t\tbuild with built-in libass support [no]"
  echo -e "  --enable-linux-libiconv\tbuild with built-in libiconv support [no]"
  echo -e "  --enable-linux-libtheora\tbuild with built-in libtheora support [no]"
  echo -e "  --enable-linux-libvorbis\tbuild with built-in libvorbis support [no]"
  echo -e "  --enable-linux-libvpx\t\tbuild with built-in libvpx support [no]"
  echo -e "  --enable-linux-libwebp\tbuild with built-in libwebp support [no]"
  echo -e "  --enable-linux-libxml2\tbuild with built-in libxml2 support [no]"
  echo -e "  --enable-linux-opencl\t\tbuild with built-in opencl support [no]"
  echo -e "  --enable-linux-opencore-amr\tbuild with built-in opencore-amr support [no]"
  echo -e "  --enable-linux-opus\t\tbuild with built-in opus support [no]"
  echo -e "  --enable-linux-sdl\t\tbuild with built-in sdl support [no]"
  echo -e "  --enable-linux-shine\t\tbuild with built-in shine support [no]"
  echo -e "  --enable-linux-snappy\t\tbuild with built-in snappy support [no]"
  echo -e "  --enable-linux-soxr\t\tbuild with built-in soxr support [no]"
  echo -e "  --enable-linux-speex\t\tbuild with built-in speex support [no]"
  echo -e "  --enable-linux-tesseract\tbuild with built-in tesseract support [no]"
  echo -e "  --enable-linux-twolame\tbuild with built-in twolame support [no]"
  echo -e "  --enable-linux-vaapi\t\tbuild with built-in vaapi support [no]"
  echo -e "  --enable-linux-v4l2\t\tbuild with built-in v4l2 support [no]"
  echo -e "  --enable-linux-vo-amrwbenc\tbuild with built-in vo-amrwbenc support [no]"
  echo -e "  --enable-linux-zlib\t\tbuild with built-in zlib support [no]"
  echo -e "  --enable-chromaprint\t\tbuild with chromaprint support [no]"
  echo -e "  --enable-dav1d\t\tbuild with dav1d [no]"
  echo -e "  --enable-kvazaar\t\tbuild with kvazaar [no]"
  echo -e "  --enable-libaom\t\tbuild with libaom [no]"
  echo -e "  --enable-libjxl\t\tbuild with libjxl [no]"
  echo -e "  --enable-liblc3\t\tbuild with liblc3 [no]"
  echo -e "  --enable-libsvtav1\t\tbuild with libsvtav1 [no]"
  echo -e "  --enable-libilbc\t\tbuild with libilbc [no]"
  echo -e "  --enable-openh264\t\tbuild with openh264 [no]"
  echo -e "  --enable-openssl\t\tbuild with openssl [no]"
  echo -e "  --enable-srt\t\t\tbuild with srt [no]"
  echo -e "  --enable-vvenc\t\tbuild with vvenc [no]"
  echo -e "  --enable-zimg\t\t\tbuild with zimg [no]\n"

  echo -e "GPL libraries:"
  echo -e "  --enable-linux-libvidstab\tbuild with built-in libvidstab support [no]"
  echo -e "  --enable-linux-rubberband\tbuild with built-in rubber band support [no]"
  echo -e "  --enable-linux-x265\t\tbuild with built-in x265 support [no]"
  echo -e "  --enable-linux-xvidcore\tbuild with built-in xvidcore support [no]"
  echo -e "  --enable-x264\t\t\tbuild with x264 [no]\n"

  display_help_custom_libraries
  display_help_advanced_options
}
