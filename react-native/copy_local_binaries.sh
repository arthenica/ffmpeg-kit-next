#!/usr/bin/env bash
#
# copy_local_binaries.sh
#
# Copies the locally-built FFmpegKit native binaries from the repository
# "prebuilt/" directory into this React Native plugin's platform folders so the
# plugin is self-contained and references no remote binary repositories.
#
#   Android : prebuilt/bundle-android-aar-*-maven/  (a local Maven repository)
#               -> android/libs-maven/
#   iOS     : prebuilt/bundle-apple-xcframework-ios-*/*.xcframework
#               -> ios/Frameworks/
#
# These destinations match the plugin build configuration:
#   * android/build.gradle  resolves the AAR via maven { url "$projectDir/libs-maven" }
#   * ffmpeg-kit-react-native.podspec  vendors ios/Frameworks/*.xcframework
#
# Build the binaries first from the repository root, e.g.:
#   ./nix-android.sh -p <profile>
#   ./nix-ios.sh     -p <profile>
#
# Usage:
#   ./copy_local_binaries.sh [android] [ios]
#
# With no platform arguments, every platform that has a matching bundle in
# prebuilt/ is copied. Set FFMPEG_KIT_PREBUILT_DIR to override the location of
# the prebuilt directory.

set -euo pipefail

plugin_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Plugin lives at <repo>/react-native, so prebuilt is one level up.
prebuilt_dir="${FFMPEG_KIT_PREBUILT_DIR:-$(cd "${plugin_dir}/.." && pwd)/prebuilt}"

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
  dest="${plugin_dir}/${dest_subdir}"
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
  echo "${platform}: copied ${count} xcframework(s) from $(basename "${bundle}") -> ${dest_subdir}/"
}

platforms=("$@")
if ((${#platforms[@]} == 0)); then
  platforms=(android ios)
fi

for platform in "${platforms[@]}"; do
  case "${platform}" in
    android) copy_android ;;
    ios)     copy_apple ios 'bundle-apple-xcframework-ios-*' ios/Frameworks ;;
    *)
      echo "error: unknown platform '${platform}' (expected android or ios)" >&2
      exit 1
      ;;
  esac
done

echo "done."
