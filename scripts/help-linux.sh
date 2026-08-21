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
libraries enabled. Options can be used to select architectures and/or enable external libraries. Please note that GPL libraries \
(external libraries with GPL license) need --enable-gpl flag to be set explicitly. When compilation ends, \
libraries are created under the prebuilt folder.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  display_help_options "${PROFILE_OPTION}" "      --jobs=N\t\t\tnumber of jobs to run [auto]" "      --package-name=name\t\tset FFmpegKit package name in the native build [empty]" "      --no-ffmpeg-kit-protocols\tdisable custom ffmpeg-kit protocols (ffkitmem, ffkitstream) [no]"
  display_help_licensing

  echo -e "Architectures:"
  echo -e "  --arch=ARCH[,ARCH...]\t\tbuild only the listed architectures [default]"
  echo -e "  --disable-arch-arm64\t\tdo not build arm64 architecture [yes, on arm64 hosts]"
  echo -e "  --disable-arch-x86-64\tdo not build x86-64 architecture [yes, on x86-64 hosts]\n"

  echo -e "Libraries:"
  echo -e "  --enable-lib-all\t\tenable all libraries allowed by the selected license policy"
  echo -e "  --enable-lib-linux-alsa\tbuild with built-in alsa support [no]"
  echo -e "  --enable-lib-linux-fontconfig\tbuild with built-in fontconfig support [no]"
  echo -e "  --enable-lib-linux-freetype\tbuild with built-in freetype support [no]"
  echo -e "  --enable-lib-linux-fribidi\tbuild with built-in fribidi support [no]"
  echo -e "  --enable-lib-linux-gmp\tbuild with built-in gmp support [no]"
  echo -e "  --enable-lib-linux-gnutls\tbuild with built-in gnutls support [no]"
  echo -e "  --enable-lib-linux-harfbuzz\tbuild with built-in harfbuzz support [no]"
  echo -e "  --enable-lib-linux-lame\tbuild with built-in lame support [no]"
  echo -e "  --enable-lib-linux-libass\tbuild with built-in libass support [no]"
  echo -e "  --enable-lib-linux-libiconv\tbuild with built-in libiconv support [no]"
  echo -e "  --enable-lib-linux-libtheora\tbuild with built-in libtheora support [no]"
  echo -e "  --enable-lib-linux-libvorbis\tbuild with built-in libvorbis support [no]"
  echo -e "  --enable-lib-linux-libvpx\tbuild with built-in libvpx support [no]"
  echo -e "  --enable-lib-linux-libwebp\tbuild with built-in libwebp support [no]"
  echo -e "  --enable-lib-linux-libxml2\tbuild with built-in libxml2 support [no]"
  echo -e "  --enable-lib-linux-opencl\tbuild with built-in opencl support [no]"
  echo -e "  --enable-lib-linux-opencore-amr\tbuild with built-in opencore-amr support [no]"
  echo -e "  --enable-lib-linux-opus\tbuild with built-in opus support [no]"
  echo -e "  --enable-lib-linux-sdl\tbuild with built-in sdl support [no]"
  echo -e "  --enable-lib-linux-shine\tbuild with built-in shine support [no]"
  echo -e "  --enable-lib-linux-snappy\tbuild with built-in snappy support [no]"
  echo -e "  --enable-lib-linux-soxr\tbuild with built-in soxr support [no]"
  echo -e "  --enable-lib-linux-speex\tbuild with built-in speex support [no]"
  echo -e "  --enable-lib-linux-tesseract\tbuild with built-in tesseract support [no]"
  echo -e "  --enable-lib-linux-twolame\tbuild with built-in twolame support [no]"
  echo -e "  --enable-lib-linux-vaapi\tbuild with built-in vaapi support [no]"
  echo -e "  --enable-lib-linux-v4l2\tbuild with built-in v4l2 support [no]"
  echo -e "  --enable-lib-linux-vo-amrwbenc\tbuild with built-in vo-amrwbenc support [no]"
  echo -e "  --enable-lib-linux-zlib\tbuild with built-in zlib support [no]"
  echo -e "  --enable-lib-chromaprint\tbuild with chromaprint support [no]"
  echo -e "  --enable-lib-dav1d\t\tbuild with dav1d [no]"
  echo -e "  --enable-lib-kvazaar\t\tbuild with kvazaar [no]"
  echo -e "  --enable-lib-libaom\t\tbuild with libaom [no]"
  echo -e "  --enable-lib-libjxl\t\tbuild with libjxl [no]"
  echo -e "  --enable-lib-liblc3\t\tbuild with liblc3 [no]"
  echo -e "  --enable-lib-libsvtav1\tbuild with libsvtav1 [no]"
  echo -e "  --enable-lib-libilbc\t\tbuild with libilbc [no]"
  echo -e "  --enable-lib-openh264\t\tbuild with openh264 [no]"
  echo -e "  --enable-lib-openssl\t\tbuild with openssl [no]"
  echo -e "  --enable-lib-srt\t\tbuild with srt [no]"
  echo -e "  --enable-lib-vvenc\t\tbuild with vvenc [no]"
  echo -e "  --enable-lib-zimg\t\tbuild with zimg [no]\n"

  echo -e "GPL libraries:"
  echo -e "  --enable-lib-linux-libvidstab\tbuild with built-in libvidstab support [no]"
  echo -e "  --enable-lib-linux-rubberband\tbuild with built-in rubber band support [no]"
  echo -e "  --enable-lib-linux-x265\tbuild with built-in x265 support [no]"
  echo -e "  --enable-lib-linux-xvidcore\tbuild with built-in xvidcore support [no]"
  echo -e "  --enable-lib-x264\t\tbuild with x264 [no]\n"

  display_help_custom_libraries
  display_help_advanced_options
}
