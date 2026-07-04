#!/bin/bash

enable_default_architecture_variants() {
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONEOS]=1
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONESIMULATOR]=1
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MAC_CATALYST]=1
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVOS]=1
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVSIMULATOR]=1
  ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MACOS]=1
}

get_umbrella_xcframework_directory() {
  local UMBRELLA_XCF_DIR="umbrella-apple-xcframework"

  if [[ ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONEOS]} == 1 || ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONESIMULATOR]} == 1 ]]; then
    UMBRELLA_XCF_DIR+="-ios${IOS_MIN_VERSION}"
  fi

  if [[ ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MAC_CATALYST]} == 1 ]]; then
    UMBRELLA_XCF_DIR+="-maccatalyst${IOS_MIN_VERSION}"
  fi

  if [[ ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MACOS]} == 1 ]]; then
    UMBRELLA_XCF_DIR+="-macos${MACOS_MIN_VERSION}"
  fi

  if [[ ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVOS]} == 1 || ${ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVSIMULATOR]} == 1 ]]; then
    UMBRELLA_XCF_DIR+="-tvos${TVOS_MIN_VERSION}"
  fi

  echo "${UMBRELLA_XCF_DIR}"
}

initialize_prebuilt_umbrella_xcframework_folders() {
  echo -e "DEBUG: Initializing umbrella xcframework directory at ${ROOT_UMBRELLA_XCFRAMEWORK_DIRECTORY}\n" 1>>"${BASEDIR}"/build.log 2>&1

  mkdir -p "${ROOT_UMBRELLA_XCFRAMEWORK_DIRECTORY}" 1>>"${BASEDIR}"/build.log 2>&1
}

#
# 1. framework name
#
create_umbrella_xcframework() {
  local FRAMEWORK_NAME="$1"

  local XCFRAMEWORK_PATH="${ROOT_UMBRELLA_XCFRAMEWORK_DIRECTORY}/${FRAMEWORK_NAME}.xcframework"

  initialize_folder "${XCFRAMEWORK_PATH}"

  local BUILD_COMMAND="xcodebuild -create-xcframework "

  for ARCHITECTURE_VARIANT_INDEX in "${TARGET_ARCHITECTURE_VARIANT_INDEX_ARRAY[@]}"; do
    local FRAMEWORK_PATH="${BASEDIR}"/prebuilt/$(get_framework_directory "${ARCHITECTURE_VARIANT_INDEX}")/${FRAMEWORK_NAME}.framework
    BUILD_COMMAND+=" -framework \"${FRAMEWORK_PATH}\""
  done

  BUILD_COMMAND+=" -output \"${XCFRAMEWORK_PATH}\""

  # EXECUTE CREATE FRAMEWORK COMMAND
  COMMAND_OUTPUT=$(eval ${BUILD_COMMAND} 2>&1)
  RC=$?
  echo -e "DEBUG: ${COMMAND_OUTPUT}\n" 1>>"${BASEDIR}"/build.log 2>&1

  if [[ ${RC} -ne 0 ]]; then
    echo -e "INFO: Building ${FRAMEWORK_NAME} umbrella xcframework failed\n" 1>>"${BASEDIR}"/build.log 2>&1
    echo -e "failed\n\nSee build.log for details\n"
    exit 1
  fi

  # DO NOT ALLOW EMPTY FRAMEWORKS
  if [[ ${COMMAND_OUTPUT} == *"is empty in library"* ]]; then
    echo -e "INFO: Building ${FRAMEWORK_NAME} umbrella xcframework failed\n" 1>>"${BASEDIR}"/build.log 2>&1
    echo -e "failed\n\nSee build.log for details\n"
    exit 1
  fi
}

disable_arch_variant() {
  case $1 in
  iphoneos)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONEOS]=0
    ;;
  iphonesimulator)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_IPHONESIMULATOR]=0
    ;;
  mac-catalyst)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MAC_CATALYST]=0
    ;;
  appletvos)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVOS]=0
    ;;
  appletvsimulator)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_APPLETVSIMULATOR]=0
    ;;
  macosx)
    ENABLED_ARCHITECTURE_VARIANTS[ARCH_VAR_MACOS]=0
    ;;
  *)
    print_unknown_arch_variant "$1"
    ;;
  esac
}

# CHECK IF XCODE IS INSTALLED
if [ ! -x "$(command -v xcrun)" ]; then
  echo -e "\n(*) xcrun command not found. Please check your Xcode installation\n"
  exit 1
fi

if [ ! -x "$(command -v xcodebuild)" ]; then
  echo -e "\n(*) xcodebuild command not found. Please check your Xcode installation\n"
  exit 1
fi

# LOAD INITIAL SETTINGS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BASEDIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${BASEDIR}"
source "${SCRIPT_DIR}"/variable.sh
export FFMPEG_KIT_BUILD_TYPE="apple"
source "${SCRIPT_DIR}"/function-${FFMPEG_KIT_BUILD_TYPE}.sh
source "${SCRIPT_DIR}"/help-${FFMPEG_KIT_BUILD_TYPE}.sh

# SET DEFAULT SETTINGS
enable_default_architecture_variants

# SELECT XCODE VERSION USED FOR BUILDING
XCODE_FOR_FFMPEG_KIT=$(ls ~/.xcode.for.ffmpeg.kit.sh)
if [[ -f ${XCODE_FOR_FFMPEG_KIT} ]]; then
  source "${XCODE_FOR_FFMPEG_KIT}" 1>>"${BASEDIR}"/build.log 2>&1
fi

# DETECT SDK VERSIONS
DETECTED_IOS_SDK_VERSION="$(xcrun --sdk iphoneos --show-sdk-version 2>>"${BASEDIR}"/build.log)"
DETECTED_TVOS_SDK_VERSION="$(xcrun --sdk appletvos --show-sdk-version 2>>"${BASEDIR}"/build.log)"
DETECTED_MACOS_SDK_VERSION="$(xcrun --sdk macosx --show-sdk-version 2>>"${BASEDIR}"/build.log)"
enable_ios_main_build
enable_macos_main_build
enable_tvos_main_build
XCODE_PATH=$(xcode-select -p 2>>"${BASEDIR}"/build.log)
echo -e "INFO: Build options: $*\n" 1>>"${BASEDIR}"/build.log 2>&1

# SET DEFAULT BUILD OPTIONS
DISPLAY_HELP=""
BUILD_TYPE_ID=""
BUILD_FULL=""
FFMPEG_KIT_XCF_BUILD=""
FFMPEG_KIT_SPM_BUILD=""
BUILD_FORCE=""
BUILD_VERSION=$(git describe --tags --always 2>>"${BASEDIR}"/build.log)
if [[ -z ${BUILD_VERSION} ]]; then
  echo -e "\n(*): Can not run git commands in this folder. See build.log.\n"
  exit 1
fi

# PROCESS BUILD OPTIONS
while [ ! $# -eq 0 ]; do
  case $1 in
  -h | --help)
    DISPLAY_HELP="1"
    ;;
  -v | --version)
    display_version
    exit 0
    ;;
  -f | --force)
    export BUILD_FORCE="1"
    ;;
  --ios-target=*)
    TARGET="${1#--ios-target=}"

    export IOS_MIN_VERSION=${TARGET}
    ;;
  --mac-catalyst-target=*)
    TARGET="${1#--mac-catalyst-target=}"

    export MAC_CATALYST_MIN_VERSION=${TARGET}
    ;;
  --macos-target=*)
    TARGET="${1#--macos-target=}"

    export MACOS_MIN_VERSION=${TARGET}
    ;;
  --tvos-target=*)
    TARGET="${1#--tvos-target=}"

    export TVOS_MIN_VERSION=${TARGET}
    ;;
  --disable-*)
    DISABLED_ARCH_VARIANT=$(echo $1 | sed -e 's/^--[A-Za-z]*-//g')

    disable_arch_variant "${DISABLED_ARCH_VARIANT}"
    ;;
  --spm)
    export FFMPEG_KIT_SPM_BUILD="1"
    ;;
  *)
    print_unknown_option "$1"
    ;;
  esac
  shift
done

echo -e "INFO: Using iOS min target: ${IOS_MIN_VERSION}, Mac Catalyst min target: ${MAC_CATALYST_MIN_VERSION}, macOS min target: ${MACOS_MIN_VERSION}, tvOS min target: ${TVOS_MIN_VERSION} by Xcode provided at ${XCODE_PATH}\n" 1>>"${BASEDIR}"/build.log 2>&1

# IF HELP DISPLAYED EXIT
if [[ -n ${DISPLAY_HELP} ]]; then
  display_help
  exit 0
fi

echo -e "\nBuilding ffmpeg-kit-next ${BUILD_TYPE_ID}umbrella xcframework\n"
echo -e -n "INFO: Building ffmpeg-kit-next ${BUILD_VERSION} ${BUILD_TYPE_ID}umbrella xcframework: " 1>>"${BASEDIR}"/build.log 2>&1
echo -e "$(date)\n" 1>>"${BASEDIR}"/build.log 2>&1

# PRINT BUILD SUMMARY
print_enabled_architecture_variants
print_enabled_xcframeworks

echo ""

# THIS WILL SAVE ARCHITECTURE VARIANTS TO BE INCLUDED
TARGET_ARCHITECTURE_VARIANT_INDEX_ARRAY=()

# SAVE ARCHITECTURE VARIANTS
for run_arch_variant in {1..8}; do
  if [[ ${ENABLED_ARCHITECTURE_VARIANTS[$run_arch_variant]} -eq 1 ]]; then
    case "$run_arch_variant" in
    1 | 5) ;;
    *)
      TARGET_ARCHITECTURE_VARIANT_INDEX_ARRAY+=("${run_arch_variant}")
      ;;
    esac
  fi
done

# BUILD XCFRAMEWORKS
if [[ -n ${TARGET_ARCHITECTURE_VARIANT_INDEX_ARRAY[0]} ]]; then

  ROOT_UMBRELLA_XCFRAMEWORK_DIRECTORY=${BASEDIR}/prebuilt/$(get_umbrella_xcframework_directory)

  echo -e -n "Creating umbrella xcframeworks under prebuilt: "

  # INITIALIZE TARGET FOLDERS
  initialize_prebuilt_umbrella_xcframework_folders

  for FFMPEG_LIB in "${FFMPEG_LIBS[@]}"; do
    create_umbrella_xcframework "${FFMPEG_LIB}"
  done

  create_umbrella_xcframework "ffmpegkit"

  # CREATE A LOCAL SWIFT PACKAGE MANIFEST WHEN --spm IS ENABLED
  if [[ -n ${FFMPEG_KIT_SPM_BUILD} ]]; then
    create_spm_package "${ROOT_UMBRELLA_XCFRAMEWORK_DIRECTORY}"
  fi

  echo -e -n "INFO: Umbrella xcframeworks created successfully\n\n" 1>>"${BASEDIR}"/build.log 2>&1
  echo -e "ok\n"
fi
