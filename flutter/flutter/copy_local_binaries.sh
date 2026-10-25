#!/usr/bin/env bash
#
# copy_local_binaries.sh
#
# Copies the locally-built FFmpegKit native binaries from the repository
# "prebuilt/" directory into this Flutter plugin's platform folders so the
# plugin is self-contained and references no remote binary repositories.
#
#   Android : prebuilt/bundle-android-aar-*-maven/  (a local Maven repository)
#               -> android/libs-maven/
#   iOS     : prebuilt/bundle-apple-xcframework-ios-*/*.xcframework
#               -> ios/ffmpeg_kit_next_flutter/Frameworks/
#   macOS   : prebuilt/bundle-apple-xcframework-macos-*/*.xcframework
#               -> macos/ffmpeg_kit_next_flutter/Frameworks/
#   Linux   : prebuilt/bundle-linux/ffmpeg-kit-next/{include,lib,licenses}
#               -> linux/Frameworks/ffmpeg-kit-next/
#   Web     : prebuilt/bundle-web-wasm32/ffmpeg-kit-next/{dist,lib,LICENSE,SOURCE,licenses}
#               -> web/ffmpeg-kit-next/ (web-only plugin assets; README.md placeholders are kept)
#
# Build the binaries first from the repository root, e.g.:
#   ./nix-android.sh -p <profile>
#   ./nix-ios.sh     -p <profile>
#   ./nix-macos.sh   -p <profile>
#   ./nix-linux.sh   -p <profile>
#   ./nix-web.sh     -p <profile>
#
# Usage:
#   ./copy_local_binaries.sh [android] [ios] [macos] [linux] [windows] [web]
#
# With no platform arguments, every platform that has a matching bundle in
# prebuilt/ is copied. Set FFMPEG_KIT_PREBUILT_DIR to override the location of
# the prebuilt directory.

set -euo pipefail

plugin_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Plugin lives at <repo>/flutter/flutter, so prebuilt is two levels up.
prebuilt_dir="${FFMPEG_KIT_PREBUILT_DIR:-$(cd "${plugin_dir}/../.." && pwd)/prebuilt}"

if [[ ! -d "${prebuilt_dir}" ]]; then
  echo "error: prebuilt directory not found: ${prebuilt_dir}" >&2
  echo "       build the native binaries first (e.g. ./nix-android.sh -p <profile>)." >&2
  exit 1
fi

# Resolve a single bundle directory from a glob pattern. Prints the path on
# stdout, or returns non-zero if nothing matches.
resolve_bundle() {
  local pattern="$1"
  local matches=()
  while IFS= read -r -d '' dir; do
    matches+=("${dir}")
  done < <(find "${prebuilt_dir}" -maxdepth 1 -type d -name "${pattern}" -print0 | sort -z)

  if ((${#matches[@]} == 0)); then
    return 1
  fi
  # bash 3.2 (macOS default) has no negative array subscripts; index explicitly.
  local last=$((${#matches[@]} - 1))
  if ((${#matches[@]} > 1)); then
    echo "warning: multiple bundles match '${pattern}', using $(basename "${matches[$last]}")" >&2
  fi
  printf '%s\n' "${matches[$last]}"
}

copy_android() {
  local bundle dest
  if ! bundle="$(resolve_bundle 'bundle-android-aar-*')"; then
    echo "skip android: no bundle-android-aar-* found in ${prebuilt_dir}" >&2
    return 0
  fi
  # The bundle is a local Maven repository (com/arthenica/ffmpeg-kit-next/<version>/).
  if [[ ! -d "${bundle}/com/arthenica/ffmpeg-kit-next" ]]; then
    echo "error: no ffmpeg-kit-next Maven artifact found in: ${bundle}" >&2
    return 1
  fi
  dest="${plugin_dir}/android/libs-maven"
  rm -rf "${dest}"
  mkdir -p "${dest}"
  cp -R "${bundle}/." "${dest}/"
  echo "android: copied Maven repo $(basename "${bundle}") -> android/libs-maven/"
}

copy_apple() {
  local platform="$1" pattern="$2" dest_subdir="$3"
  local bundle dest
  if ! bundle="$(resolve_bundle "${pattern}")"; then
    echo "skip ${platform}: no ${pattern} found in ${prebuilt_dir}" >&2
    return 0
  fi
  dest="${plugin_dir}/${dest_subdir}/ffmpeg_kit_next_flutter/Frameworks"
  rm -rf "${dest}"
  mkdir -p "${dest}"
  local count=0
  while IFS= read -r -d '' fw; do
    cp -R "${fw}" "${dest}/"
    count=$((count + 1))
  done < <(find "${bundle}" -maxdepth 1 -type d -name '*.xcframework' -print0)
  if ((count == 0)); then
    echo "error: no .xcframework found in ${bundle}" >&2
    return 1
  fi
  echo "${platform}: copied ${count} xcframework(s) from $(basename "${bundle}") -> ${dest_subdir}/ffmpeg_kit_next_flutter/Frameworks/"
}

copy_linux() {
  local bundle src dest
  bundle="${prebuilt_dir}/bundle-linux"
  if [[ ! -d "${bundle}" ]]; then
    echo "skip linux: no bundle-linux found in ${prebuilt_dir}" >&2
    return 0
  fi
  src="${bundle}/ffmpeg-kit-next"
  if [[ ! -f "${src}/lib/libffmpegkit.so" ]]; then
    echo "error: libffmpegkit.so not found in: ${src}/lib" >&2
    return 1
  fi
  if [[ ! -d "${src}/licenses" ]]; then
    echo "error: licence texts not found in: ${src}/licenses" >&2
    return 1
  fi
  dest="${plugin_dir}/linux/Frameworks/ffmpeg-kit-next"
  rm -rf "${dest}"
  mkdir -p "${dest}"
  cp -R "${src}/include" "${dest}/include"
  cp -R "${src}/lib" "${dest}/lib"
  cp -R "${src}/licenses" "${dest}/licenses"
  echo "linux: copied bundle-linux -> linux/Frameworks/ffmpeg-kit-next/"
}

copy_windows() {
  local src dest
  src="${prebuilt_dir}/bundle-windows/ffmpeg-kit-next"
  dest="${plugin_dir}/windows/Frameworks/ffmpeg-kit-next"
  if [[ ! -d "${src}" ]]; then
    echo "skip windows: no bundle-windows/ffmpeg-kit-next found in ${prebuilt_dir}" >&2
    return 0
  fi
  if [[ ! -f "${src}/include/ffmpegkit_c.h" || ! -f "${src}/lib/ffmpegkit.lib" || ! -d "${src}/bin" ||
        ! -d "${src}/licenses" ]]; then
    echo "error: incomplete Windows bundle: ${src}" >&2
    return 1
  fi
  rm -rf "${dest}"
  mkdir -p "${dest}"
  cp -R "${src}/." "${dest}/"
  echo "windows: copied bundle-windows -> windows/Frameworks/ffmpeg-kit-next/"
}

copy_web() {
  local src dest folder
  src="${prebuilt_dir}/bundle-web-wasm32/ffmpeg-kit-next"
  dest="${plugin_dir}/web/ffmpeg-kit-next"
  if [[ ! -d "${src}" ]]; then
    echo "skip web: no bundle-web-wasm32/ffmpeg-kit-next found in ${prebuilt_dir}" >&2
    return 0
  fi
  if [[ ! -f "${src}/dist/index.js" || ! -f "${src}/dist/ffmpegkit.worker.js" || ! -f "${src}/lib/libffmpegkit.wasm" ||
        ! -f "${src}/LICENSE" || ! -f "${src}/SOURCE" || ! -d "${src}/licenses" ]]; then
    echo "error: incomplete web bundle: ${src}" >&2
    return 1
  fi
  # Remove the previous copy but keep the placeholders that the asset entries need.
  for folder in dist/src dist lib licenses; do
    mkdir -p "${dest}/${folder}"
    find "${dest}/${folder}" -mindepth 1 -maxdepth 1 ! -name README.md ! -name src -exec rm -rf {} +
  done
  rm -f "${dest}/LICENSE" "${dest}/SOURCE"
  cp -R "${src}/dist/." "${dest}/dist/"
  cp -R "${src}/lib/." "${dest}/lib/"
  # The licence texts ship with the binaries (web-only assets, like dist/ and lib/).
  cp "${src}/LICENSE" "${src}/SOURCE" "${dest}/"
  cp -R "${src}/licenses/." "${dest}/licenses/"
  echo "web: copied bundle-web-wasm32 -> web/ffmpeg-kit-next/"
}

platforms=("$@")
if ((${#platforms[@]} == 0)); then
  platforms=(android ios macos linux windows web)
fi

for platform in "${platforms[@]}"; do
  case "${platform}" in
    android) copy_android ;;
    ios)     copy_apple ios   'bundle-apple-xcframework-ios-*'   ios ;;
    macos)   copy_apple macos 'bundle-apple-xcframework-macos-*' macos ;;
    linux)   copy_linux ;;
    windows) copy_windows ;;
    web)     copy_web ;;
    *)
      echo "error: unknown platform '${platform}' (expected android, ios, macos, linux, windows or web)" >&2
      exit 1
      ;;
  esac
done

echo "done."
