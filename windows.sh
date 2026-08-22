#!/bin/bash

# LOAD INITIAL SETTINGS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BASEDIR="${SCRIPT_DIR}"
cd "${BASEDIR}"
export FFMPEG_KIT_BUILD_TYPE="windows"
source "${SCRIPT_DIR}"/scripts/variable.sh
source "${SCRIPT_DIR}"/scripts/function-${FFMPEG_KIT_BUILD_TYPE}.sh
source "${SCRIPT_DIR}"/scripts/help-${FFMPEG_KIT_BUILD_TYPE}.sh
disabled_libraries=()
disabled_libraries_with_deps=()

# SET DEFAULT SETTINGS
enable_default_windows_architectures

echo -e "INFO: Build options: $*\n" 1>>"${BASEDIR}"/build.log 2>&1

# SET DEFAULT BUILD OPTIONS
export GPL_ENABLED="no"
DISPLAY_HELP=""
ENABLE_ALL_LIBRARIES=""
BUILD_TYPE_ID=""
BUILD_VERSION=$(git describe --tags --always 2>>"${BASEDIR}"/build.log)

set_default_min_windows_platform_version

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
  --no-output-redirection)
    no_output_redirection
    ;;
  --no-workspace-cleanup-*)
    NO_WORKSPACE_CLEANUP_LIBRARY="${1#--no-workspace-cleanup-}"

    no_workspace_cleanup_library "${NO_WORKSPACE_CLEANUP_LIBRARY}"
    ;;
  --no-link-time-optimization)
    no_link_time_optimization
    ;;
  --no-ffmpeg-kit-protocols)
    export NO_FFMPEG_KIT_PROTOCOLS="1"
    ;;
  --no-static-mingw-runtime)
    export NO_STATIC_MINGW_RUNTIME="1"
    ;;
  -d | --debug)
    enable_debug
    ;;
  -s | --speed)
    optimize_for_speed
    ;;
  -f | --force)
    export BUILD_FORCE="1"
    ;;
  --jobs=*)
    JOB_COUNT="${1#--jobs=}"
    export BUILD_JOBS="${JOB_COUNT}"
    ;;
  --package-name=*)
    PACKAGE_NAME="${1#--package-name=}"

    export FFMPEG_KIT_PACKAGE_NAME="${PACKAGE_NAME}"
    ;;
  --extra-cflags=*)
    EXTRA_CFLAGS="${1#--extra-cflags=}"
    export EXTRA_CFLAGS="${EXTRA_CFLAGS}"
    ;;
  --extra-cxxflags=*)
    EXTRA_CXXFLAGS="${1#--extra-cxxflags=}"
    export EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS}"
    ;;
  --extra-ldflags=*)
    EXTRA_LDFLAGS="${1#--extra-ldflags=}"
    export EXTRA_LDFLAGS="${EXTRA_LDFLAGS}"
    ;;
  *)
    process_common_build_option "$1" || print_unknown_option "$1"
    ;;
  esac
  shift
done

if [[ -z ${BUILD_VERSION} ]]; then
  echo -e "\n(*) error: Can not run git commands in this folder. See build.log.\n"
  exit 1
fi

# PROCESS ENABLE_ALL_LIBRARIES OPTION AS LAST OPTION
if [[ -n ${ENABLE_ALL_LIBRARIES} ]]; then
  enable_all_libraries
fi

# DISABLE SPECIFIED LIBRARIES AND THEIR DEPENDENCIES
for disabled_library in "${disabled_libraries_with_deps[@]}"; do
  set_library "${disabled_library}" 0
done

# DISABLE SPECIFIED LIBRARIES ONLY
for disabled_library in "${disabled_libraries[@]}"; do
  disable_library "${disabled_library}"
done

# IF HELP DISPLAYED EXIT
if [[ -n ${DISPLAY_HELP} ]]; then
  display_help
  exit 0
fi

echo -e "\nBuilding ffmpeg-kit-next ${BUILD_TYPE_ID}library for Windows\n"
echo -e -n "INFO: Building ffmpeg-kit-next ${BUILD_VERSION} ${BUILD_TYPE_ID}library for Windows: " 1>>"${BASEDIR}"/build.log 2>&1
echo -e "$(date)\n" 1>>"${BASEDIR}"/build.log 2>&1

# PRINT BUILD SUMMARY
print_enabled_architectures
print_enabled_libraries
print_reconfigure_requested_libraries
print_rebuild_requested_libraries
print_redownload_requested_libraries
print_custom_libraries

# VALIDATE GPL FLAGS
for gpl_library in {$LIBRARY_X264,$LIBRARY_X265,$LIBRARY_XVIDCORE,$LIBRARY_LIBVIDSTAB,$LIBRARY_RUBBERBAND}; do
  if [[ ${ENABLED_LIBRARIES[$gpl_library]} -eq 1 ]]; then
    library_name=$(get_library_name ${gpl_library})

    if [ ${GPL_ENABLED} != "yes" ]; then
      echo -e "\n(*) Invalid configuration detected. GPL library ${library_name} enabled without --enable-gpl flag.\n"
      echo -e "\n(*) Invalid configuration detected. GPL library ${library_name} enabled without --enable-gpl flag.\n" 1>>"${BASEDIR}"/build.log 2>&1
      exit 1
    fi
  fi
done

trap fail_operation EXIT
echo -n -e "\nDownloading sources: "
echo -e "INFO: Downloading the source code of ffmpeg and external libraries.\n" 1>>"${BASEDIR}"/build.log 2>&1

# DOWNLOAD GNU CONFIG
download_gnu_config

# DOWNLOAD RAPIDJSON
download_rapidjson

# DOWNLOAD LIBRARY SOURCES
downloaded_library_sources "${ENABLED_LIBRARIES[@]}"

# THIS WILL SAVE ARCHITECTURES TO BUILD
TARGET_ARCH_LIST=()

# BUILD ENABLED LIBRARIES ON ENABLED ARCHITECTURES
for run_arch in {0..12}; do
  if [[ ${ENABLED_ARCHITECTURES[$run_arch]} -eq 1 ]]; then
    export ARCH=$(get_arch_name "$run_arch")
    export FULL_ARCH=$(get_full_arch_name "$run_arch")

    # EXECUTE MAIN BUILD SCRIPT
    . "${SCRIPT_DIR}"/scripts/main-windows.sh "${ENABLED_LIBRARIES[@]}"

    TARGET_ARCH_LIST+=("${FULL_ARCH}")

    # CLEAR FLAGS
    for library in {0..98}; do
      library_name=$(get_library_name "${library}")
      unset "$(echo "OK_${library_name}" | sed "s/\-/\_/g")"
      unset "$(echo "DEPENDENCY_REBUILT_${library_name}" | sed "s/\-/\_/g")"
    done
  fi
done

# BUILD FFMPEG-KIT BUNDLE
if [[ -n ${TARGET_ARCH_LIST[0]} ]]; then

  echo -e -n "\nCreating the bundle under prebuilt: "

  echo -e "DEBUG: Creating the bundle directory\n" 1>>"${BASEDIR}"/build.log 2>&1

  initialize_folder "${BASEDIR}/prebuilt/$(get_bundle_directory)"

  create_windows_bundle

  echo -e "ok\n"
fi
