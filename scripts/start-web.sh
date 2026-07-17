#!/bin/bash

# LOAD INITIAL SETTINGS
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BASEDIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${BASEDIR}"
export FFMPEG_KIT_BUILD_TYPE="web"
source "${SCRIPT_DIR}"/variable.sh
source "${SCRIPT_DIR}"/function-${FFMPEG_KIT_BUILD_TYPE}.sh
source "${SCRIPT_DIR}"/help-${FFMPEG_KIT_BUILD_TYPE}.sh
disabled_libraries=()

# SET DEFAULT SETTINGS
enable_default_web_architectures

echo -e "INFO: Build options: $*\n" 1>>"${BASEDIR}"/build.log 2>&1

# SET DEFAULT BUILD OPTIONS
export GPL_ENABLED="no"
DISPLAY_HELP=""
BUILD_FULL=""
BUILD_TYPE_ID=""
BUILD_VERSION=$(git describe --tags --always 2>>"${BASEDIR}"/build.log)
export FFMPEG_KIT_WEB_PTHREADS="${FFMPEG_KIT_WEB_PTHREADS:-1}"
export FFMPEG_KIT_WEB_RELAXED_SIMD="${FFMPEG_KIT_WEB_RELAXED_SIMD:-0}"

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
  --skip-*)
    SKIP_LIBRARY="${1#--skip-}"

    skip_library "${SKIP_LIBRARY}"
    ;;
  --no-output-redirection)
    no_output_redirection
    ;;
  --no-ffmpeg-kit-protocols)
    export NO_FFMPEG_KIT_PROTOCOLS="1"
    ;;
  --no-workspace-cleanup-*)
    NO_WORKSPACE_CLEANUP_LIBRARY="${1#--no-workspace-cleanup-}"

    no_workspace_cleanup_library "${NO_WORKSPACE_CLEANUP_LIBRARY}"
    ;;
  --no-link-time-optimization)
    no_link_time_optimization
    ;;
  --enable-pthreads)
    export FFMPEG_KIT_WEB_PTHREADS="1"
    ;;
  --disable-pthreads)
    export FFMPEG_KIT_WEB_PTHREADS="0"
    ;;
  --enable-relaxed-simd)
    export FFMPEG_KIT_WEB_RELAXED_SIMD="1"
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
  --reconf-*)
    CONF_LIBRARY="${1#--reconf-}"

    reconf_library "${CONF_LIBRARY}"
    ;;
  --rebuild-*)
    BUILD_LIBRARY="${1#--rebuild-}"

    rebuild_library "${BUILD_LIBRARY}"
    ;;
  --redownload-*)
    DOWNLOAD_LIBRARY="${1#--redownload-}"

    redownload_library "${DOWNLOAD_LIBRARY}"
    ;;
  --full)
    BUILD_FULL="1"
    ;;
  --enable-gpl)
    export GPL_ENABLED="yes"
    ;;
  --enable-custom-library-*)
    CUSTOM_LIBRARY_OPTION_KEY="${1#--enable-custom-}"
    CUSTOM_LIBRARY_OPTION_KEY="${CUSTOM_LIBRARY_OPTION_KEY%%=*}"
    CUSTOM_LIBRARY_OPTION_VALUE="${1##*=}"

    echo -e "INFO: Custom library options detected: ${CUSTOM_LIBRARY_OPTION_KEY} ${CUSTOM_LIBRARY_OPTION_VALUE}\n" 1>>"${BASEDIR}"/build.log 2>&1

    generate_custom_library_environment_variables "${CUSTOM_LIBRARY_OPTION_KEY}" "${CUSTOM_LIBRARY_OPTION_VALUE}"
    ;;
  --enable-*)
    ENABLED_LIBRARY="${1#--enable-}"

    enable_library "${ENABLED_LIBRARY}"
    ;;
  --disable-lib-*)
    DISABLED_LIB="${1#--disable-lib-}"

    disabled_libraries+=("${DISABLED_LIB}")
    ;;
  --disable-*)
    DISABLED_ARCH="${1#--disable-}"

    disable_arch "${DISABLED_ARCH}"
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
  --version-*)
    CUSTOM_VERSION_KEY="${1#--version-}"
    CUSTOM_VERSION_KEY="${CUSTOM_VERSION_KEY%%=*}"
    CUSTOM_VERSION_VALUE="${1##*=}"

    echo -e "INFO: Custom version detected: ${CUSTOM_VERSION_KEY} ${CUSTOM_VERSION_VALUE}\n" 1>>"${BASEDIR}"/build.log 2>&1

    generate_custom_version_environment_variables "${CUSTOM_VERSION_KEY}" "${CUSTOM_VERSION_VALUE}"
    ;;
  *)
    print_unknown_option "$1"
    ;;
  esac
  shift
done

if [[ -z ${BUILD_VERSION} ]]; then
  echo -e "\n(*) error: Can not run git commands in this folder. See build.log.\n"
  exit 1
fi

# PROCESS FULL OPTION AS LAST OPTION
if [[ -n ${BUILD_FULL} ]]; then
  for library in {0..98}; do
    if [ "${GPL_ENABLED}" == "yes" ]; then
      enable_library "$(get_library_name $library)" 1
    else
      if [[ $(is_gpl_licensed $library) -eq 1 ]]; then
        enable_library "$(get_library_name $library)" 1
      fi
    fi
  done
fi

# DISABLE SPECIFIED LIBRARIES
for disabled_library in ${disabled_libraries[@]}; do
  set_library "${disabled_library}" 0
done

# IF HELP DISPLAYED EXIT
if [[ -n ${DISPLAY_HELP} ]]; then
  display_help
  exit 0
fi

echo -e "\nBuilding ffmpeg-kit-next ${BUILD_TYPE_ID}library for WebAssembly\n"
echo -e -n "INFO: Building ffmpeg-kit-next ${BUILD_VERSION} ${BUILD_TYPE_ID}library for WebAssembly: " 1>>"${BASEDIR}"/build.log 2>&1
echo -e "$(date)\n" 1>>"${BASEDIR}"/build.log 2>&1

# PRINT BUILD SUMMARY
print_enabled_architectures
print_enabled_libraries
print_reconfigure_requested_libraries
print_rebuild_requested_libraries
print_redownload_requested_libraries
print_custom_libraries

# VALIDATE GPL FLAGS
for gpl_library in {$LIBRARY_X264,$LIBRARY_XVIDCORE,$LIBRARY_X265,$LIBRARY_LIBVIDSTAB,$LIBRARY_RUBBERBAND}; do
  if [[ ${ENABLED_LIBRARIES[$gpl_library]} -eq 1 ]]; then
    library_name=$(get_library_name ${gpl_library})

    if [ ${GPL_ENABLED} != "yes" ]; then
      echo -e "\n(*) Invalid configuration detected. GPL library ${library_name} enabled without --enable-gpl flag.\n"
      echo -e "\n(*) Invalid configuration detected. GPL library ${library_name} enabled without --enable-gpl flag.\n" 1>>"${BASEDIR}"/build.log 2>&1
      exit 1
    fi
  fi
done

# VALIDATE PTHREAD FLAGS
if [[ ${FFMPEG_KIT_WEB_PTHREADS} != "1" && ${SKIP_ffmpeg_kit:-0} != "1" ]]; then
  echo -e "\n(*) The current web ffmpeg-kit wrapper build requires Emscripten pthreads. Use --enable-pthreads or add --skip-ffmpeg-kit for an FFmpeg-core-only build.\n"
  exit 1
fi

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
for run_arch in ${ARCH_WASM32}; do
  if [[ ${ENABLED_ARCHITECTURES[$run_arch]} -eq 1 ]]; then
    export ARCH=$(get_arch_name "$run_arch")
    export FULL_ARCH=$(get_full_arch_name "$run_arch")

    # EXECUTE MAIN BUILD SCRIPT
    . "${SCRIPT_DIR}"/main-web.sh "${ENABLED_LIBRARIES[@]}" || exit 1

    TARGET_ARCH_LIST+=("${FULL_ARCH}")

    # CLEAR FLAGS
    for library in {0..97}; do
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

  initialize_folder "${BASEDIR}/prebuilt/$(get_bundle_directory)" || exit 1

  create_web_bundle || exit 1

  echo -e "ok\n"
fi
