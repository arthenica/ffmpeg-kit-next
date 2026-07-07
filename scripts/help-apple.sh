#!/bin/bash

display_help() {
  COMMAND=$(echo "$0" | sed -e 's/\.\///g')
  local PROFILE_USAGE=""

  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    PROFILE_USAGE="-p PROFILE "
  fi

  echo -e "\n'$COMMAND' combines FFmpegKit frameworks created for Apple architecture variants in an xcframework. \
It uses frameworks created under the prebuilt folder for iOS, tvOS, macOS and visionOS architecture variants (iphoneos, \
iphonesimulator, mac-catalyst, appletvos, appletvsimulator, macosx, xros, xrsimulator) as input and builds an umbrella xcframework under \
the prebuilt folder.\n\nPlease note that this script is only responsible of packaging existing frameworks, created by \
'start-ios.sh', 'start-tvos.sh', 'start-macos.sh' and 'start-visionos.sh'. Running it will not compile any of these libraries again. Top level build scripts \
('start-ios.sh', 'start-tvos.sh', 'start-macos.sh', 'start-visionos.sh') must be used to build ffmpeg with support for a specific external library first. \
After that this script should be used to create an umbrella xcframework.\n"
  echo -e "Usage: ./$COMMAND ${PROFILE_USAGE}[OPTION]...\n"
  echo -e "Specify environment variables as VARIABLE=VALUE to override default build options.\n"

  echo -e "Options:"
  echo -e "  -h, --help\t\t\tdisplay this help and exit"
  echo -e "  -v, --version\t\t\tdisplay version information and exit"
  if [[ -n ${FFMPEG_KIT_NIX_HELP:-} ]]; then
    echo -e "  -p, --profile PROFILE\t\tnix develop profile to use"
    echo -e "      --list-profiles\t\tlist local nix develop profiles"
  fi
  echo -e "  -f, --force\t\t\tignore warnings"
  echo -e "  --spm\t\t\t\tcreate a local Swift package (Package.swift) for the umbrella xcframework [no]"
  echo -e "  --ios-target=ios sdk version\t\toverride minimum deployment target for iOS [12.1]"
  echo -e "  --mac-catalyst-target=ios sdk version\toverride minimum deployment target for Mac Catalyst [14.0]"
  echo -e "  --macos-target=macos sdk version\toverride minimum deployment target for macOS [10.15]"
  echo -e "  --tvos-target=tvos sdk version\toverride minimum deployment target for tvOS [11.0]"
  echo -e "  --visionos-target=visionos sdk version\toverride minimum deployment target for visionOS [1.0]\n"

  echo -e "Architectures:"
  echo -e "  --disable-iphoneos\t\tdo not include iphoneos architecture variant [yes]"
  echo -e "  --disable-iphonesimulator\tdo not include iphonesimulator architecture variant [yes]"
  echo -e "  --disable-mac-catalyst\tdo not include ios mac-catalyst architecture variant [yes]"
  echo -e "  --disable-appletvos\t\tdo not include appletvos architecture variant [yes]"
  echo -e "  --disable-appletvsimulator\tdo not include appletvsimulator architecture variant [yes]"
  echo -e "  --disable-macosx\t\tdo not include macosx architecture variant [yes]"
  echo -e "  --disable-xros\t\tdo not include xros architecture variant [yes]"
  echo -e "  --disable-xrsimulator\t\tdo not include xrsimulator architecture variant [yes]\n"
}
