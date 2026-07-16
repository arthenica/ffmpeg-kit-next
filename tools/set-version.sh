#!/usr/bin/env bash
#
# Updates FFmpegKitNext platform versions.
# Compatible with bash and zsh on macOS and modern Linux distributions.

set -eu

ALL_PLATFORMS="android apple linux flutter react-native"
SELECTED_PLATFORMS=""
NEW_VERSION=""
ANDROID_VERSION_CODE=""
PACKAGE_VERSION_CODE=""

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASEDIR=$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)

usage() {
  cat <<EOF
Usage:
  tools/set-version.sh VERSION [--platform PLATFORM]
  tools/set-version.sh --version VERSION [--platform PLATFORM]

Options:
  -v, --version VERSION              Version to set, for example 6.1.3.
  -p, --platform PLATFORM            Platform to update. May be repeated or comma-separated.
                                     Supported: android, apple, linux, flutter, react-native.
                                     Defaults to all platforms.
  --all                              Update all platforms.
  --android-version-code CODE        Override Android library versionCode.
                                     Default: <android minSdk><zero-padded package code>.
                                     Example: 6.1.2 with minSdk 24 becomes 240612.
  --package-version-code CODE        Override Flutter and React Native Android versionCode.
                                     Default: major + minor + patch. Example: 6.1.2 becomes 612.
  -h, --help                         Show this help.

Examples:
  tools/set-version.sh 6.1.3
  tools/set-version.sh --version 6.1.3 --platform android
  tools/set-version.sh 6.1.3 --platform flutter,react-native
EOF
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

add_platforms() {
  for platform in $(printf '%s\n' "$1" | tr ',' ' '); do
    [ -n "$platform" ] || continue
    case "$platform" in
      all)
        SELECTED_PLATFORMS="$ALL_PLATFORMS"
        ;;
      android | apple | linux | flutter | react-native)
        case " $SELECTED_PLATFORMS " in
          *" $platform "*) ;;
          *) SELECTED_PLATFORMS="${SELECTED_PLATFORMS}${SELECTED_PLATFORMS:+ }${platform}" ;;
        esac
        ;;
      *)
        die "Unsupported platform: $platform"
        ;;
    esac
  done
}

is_selected() {
  case " $SELECTED_PLATFORMS " in
    *" $1 "*) return 0 ;;
    *) return 1 ;;
  esac
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    -v | --version)
      [ "$#" -gt 1 ] || die "$1 requires a value"
      NEW_VERSION=$2
      shift 2
      ;;
    --version=*)
      NEW_VERSION=${1#*=}
      shift
      ;;
    -p | --platform)
      [ "$#" -gt 1 ] || die "$1 requires a value"
      platform_value=$2
      add_platforms "$platform_value"
      shift 2
      ;;
    --platform=*)
      add_platforms "${1#*=}"
      shift
      ;;
    --all)
      SELECTED_PLATFORMS="$ALL_PLATFORMS"
      shift
      ;;
    --android-version-code)
      [ "$#" -gt 1 ] || die "$1 requires a value"
      ANDROID_VERSION_CODE=$2
      shift 2
      ;;
    --android-version-code=*)
      ANDROID_VERSION_CODE=${1#*=}
      shift
      ;;
    --package-version-code)
      [ "$#" -gt 1 ] || die "$1 requires a value"
      PACKAGE_VERSION_CODE=$2
      shift 2
      ;;
    --package-version-code=*)
      PACKAGE_VERSION_CODE=${1#*=}
      shift
      ;;
    -*)
      die "Unknown option: $1"
      ;;
    *)
      [ -z "$NEW_VERSION" ] || die "Version specified more than once"
      NEW_VERSION=$1
      shift
      ;;
  esac
done

[ -n "$NEW_VERSION" ] || die "Missing version"
[ -n "$SELECTED_PLATFORMS" ] || SELECTED_PLATFORMS="$ALL_PLATFORMS"

case "$NEW_VERSION" in
  *[!0-9.]* | .* | *. | *..*)
    die "Version must be numeric dot-separated, for example 6.1.3"
    ;;
esac

VERSION_PART_COUNT=$(printf '%s\n' "$NEW_VERSION" | awk -F. '{ print NF }')
[ "$VERSION_PART_COUNT" -eq 3 ] || die "Version must have exactly three numeric parts, for example 6.1.3"

MAJOR=$(printf '%s\n' "$NEW_VERSION" | awk -F. '{ print $1 }')
MINOR=$(printf '%s\n' "$NEW_VERSION" | awk -F. '{ print $2 }')
PATCH=$(printf '%s\n' "$NEW_VERSION" | awk -F. '{ print $3 }')
[ -n "$MAJOR" ] && [ -n "$MINOR" ] && [ -n "$PATCH" ] || die "Version contains an empty numeric part"

case "$MAJOR$MINOR$PATCH" in
  *[!0-9]*)
    die "Version must contain only numeric parts"
    ;;
esac

if [ -z "$PACKAGE_VERSION_CODE" ]; then
  PACKAGE_VERSION_CODE="${MAJOR}${MINOR}${PATCH}"
fi

case "$PACKAGE_VERSION_CODE" in
  '' | *[!0-9]*)
    die "Package versionCode must be numeric"
    ;;
esac

if [ -z "$ANDROID_VERSION_CODE" ]; then
  ANDROID_BUILD_GRADLE="${BASEDIR}/android/ffmpeg-kit-next-android-lib/build.gradle"
  [ -f "$ANDROID_BUILD_GRADLE" ] || die "Missing file: $ANDROID_BUILD_GRADLE"
  ANDROID_MIN_SDK=$(awk '$1 == "minSdk" { print $2; exit }' "$ANDROID_BUILD_GRADLE")
  [ -n "$ANDROID_MIN_SDK" ] || die "Could not read Android minSdk from $ANDROID_BUILD_GRADLE"
  case "$ANDROID_MIN_SDK" in
    *[!0-9]*)
      die "Android minSdk is not numeric: $ANDROID_MIN_SDK"
      ;;
  esac
  ANDROID_VERSION_CODE=$(awk -v min_sdk="$ANDROID_MIN_SDK" -v package_code="$PACKAGE_VERSION_CODE" 'BEGIN { printf "%d%04d", min_sdk, package_code }')
fi

case "$ANDROID_VERSION_CODE" in
  '' | *[!0-9]*)
    die "Android versionCode must be numeric"
    ;;
esac

extract_first() {
  file=$1
  script=$2
  [ -f "$file" ] || die "Missing file: $file"
  value=$(sed -n "$script" "$file" | sed -n '1p')
  [ -n "$value" ] || die "Could not extract value from $file"
  printf '%s\n' "$value"
}

extract_android_version() {
  extract_first "${BASEDIR}/android/ffmpeg-kit-next-android-lib/build.gradle" 's/^[[:space:]]*versionName[[:space:]]*"\([^"]*\)".*/\1/p'
}

extract_android_version_code() {
  extract_first "${BASEDIR}/android/ffmpeg-kit-next-android-lib/build.gradle" 's/^[[:space:]]*versionCode[[:space:]]*\([0-9][0-9]*\).*/\1/p'
}

extract_apple_version() {
  extract_first "${BASEDIR}/apple/src/FFmpegKitConfig.m" 's/.*NSString \*const FFmpegKitVersion = @"\([^"]*\)";.*/\1/p'
}

extract_linux_version() {
  extract_first "${BASEDIR}/linux/src/FFmpegKitConfig.h" 's/.*FFmpegKitVersion = "\([^"]*\)";.*/\1/p'
}

extract_flutter_version() {
  extract_first "${BASEDIR}/flutter/flutter/pubspec.yaml" 's/^version:[[:space:]]*\([^[:space:]]*\).*/\1/p'
}

extract_flutter_version_code() {
  extract_first "${BASEDIR}/flutter/flutter/android/build.gradle" 's/^[[:space:]]*versionCode[[:space:]]*\([0-9][0-9]*\).*/\1/p'
}

extract_react_native_version() {
  extract_first "${BASEDIR}/react-native/package.json" 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)",.*/\1/p'
}

extract_react_native_version_code() {
  extract_first "${BASEDIR}/react-native/android/build.gradle" 's/^[[:space:]]*versionCode[[:space:]]*\([0-9][0-9]*\).*/\1/p'
}

replace_in_file() {
  file=$1
  description=$2
  perl_script=$3

  [ -f "$file" ] || die "Missing file: $file"
  if ! env NEW_VERSION="$NEW_VERSION" ANDROID_VERSION_CODE="$ANDROID_VERSION_CODE" PACKAGE_VERSION_CODE="$PACKAGE_VERSION_CODE" \
    perl -0pi -e "$perl_script" "$file"; then
    die "Could not update $description in $file"
  fi
}

update_doxyfile() {
  replace_in_file "$1" "Doxyfile PROJECT_NUMBER" 'BEGIN { $count = 0 } $count += s/^(PROJECT_NUMBER[ \t]*=)[ \t]*[^\r\n]*/${1} $ENV{NEW_VERSION}/mg; END { exit($count ? 0 : 3) }'
}

update_configure_ac() {
  replace_in_file "$1" "configure.ac version" 'BEGIN { $count = 0 } $count += s/^(# ffmpeg-kit-next )[^\r\n]+( configure\.ac)$/${1}$ENV{NEW_VERSION}${2}/mg; $count += s/(AC_INIT\(\[ffmpeg-kit-next\], \[)[^]]+(\], \[https:\/\/github\.com\/arthenica\/ffmpeg-kit-next\/issues\/new\]\))/${1}$ENV{NEW_VERSION}${2}/mg; END { exit($count >= 2 ? 0 : 3) }'
}

update_android() {
  update_doxyfile "${BASEDIR}/android/ffmpeg-kit-next-android-lib/Doxyfile"
  replace_in_file "${BASEDIR}/android/ffmpeg-kit-next-android-lib/build.gradle" "Android Gradle version" 'BEGIN { $count = 0 } $count += s/(versionCode[ \t]+)[0-9]+/${1}$ENV{ANDROID_VERSION_CODE}/g; $count += s/(versionName[ \t]+")[^"]+(")/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count >= 2 ? 0 : 3) }'
  replace_in_file "${BASEDIR}/android/ffmpeg-kit-next-android-lib/src/main/cpp/ffmpegkit.h" "Android native version" 'BEGIN { $count = 0 } $count += s/(#define[ \t]+FFMPEG_KIT_VERSION[ \t]+")[^"]+(")/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/android/ffmpeg-kit-next-android-lib/src/main/kotlin/com/arthenica/ffmpegkit/NativeLoader.kt" "Android test loader version" 'BEGIN { $count = 0 } $count += s/(val[ \t]+version[ \t]*=[ \t]*")[^"]+(")/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
}

update_apple() {
  update_doxyfile "${BASEDIR}/apple/Doxyfile"
  update_configure_ac "${BASEDIR}/apple/configure.ac"
  replace_in_file "${BASEDIR}/apple/src/FFmpegKitConfig.m" "Apple FFmpegKitVersion" 'BEGIN { $count = 0 } $count += s/(NSString \*const FFmpegKitVersion = @")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
}

update_linux() {
  update_doxyfile "${BASEDIR}/linux/Doxyfile"
  update_configure_ac "${BASEDIR}/linux/configure.ac"
  replace_in_file "${BASEDIR}/linux/src/FFmpegKitConfig.h" "Linux FFmpegKitVersion" 'BEGIN { $count = 0 } $count += s/(static constexpr const char \*FFmpegKitVersion = ")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
}

update_flutter() {
  replace_in_file "${BASEDIR}/flutter/flutter/pubspec.yaml" "Flutter pubspec version" 'BEGIN { $count = 0 } $count += s/^(version:[ \t]*)[^\r\n]+/${1}$ENV{NEW_VERSION}/mg; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/lib/src/ffmpeg_kit_factory.dart" "Flutter Dart version" 'BEGIN { $count = 0 } $count += s/(static String getVersion\(\) => ")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/android/build.gradle" "Flutter Android version" 'BEGIN { $count = 0 } $count += s/(versionCode[ \t]+)[0-9]+/${1}$ENV{PACKAGE_VERSION_CODE}/g; $count += s/(versionName[ \t]+")[^"]+(")/${1}$ENV{NEW_VERSION}${2}/g; $count += s/(implementation '\''com\.arthenica:ffmpeg-kit-next:)[^'\'']+('\''\s*)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count >= 3 ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/android/src/main/java/com/arthenica/ffmpegkit/flutter/FFmpegKitFlutterPlugin.java" "Flutter Android plugin library version" 'BEGIN { $count = 0 } $count += s/(public static final String LIBRARY_VERSION[ \t]*=[ \t]*")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/ios/ffmpeg_kit_next_flutter.podspec" "Flutter iOS podspec version" 'BEGIN { $count = 0 } $count += s/(s\.version[ \t]*=[ \t]*'\''\s*)[^'\'']+('\''\s*)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/ios/ffmpeg_kit_next_flutter/Sources/ffmpeg_kit_next_flutter/FFmpegKitFlutterPlugin.m" "Flutter iOS plugin library version" 'BEGIN { $count = 0 } $count += s/(static NSString \*const LIBRARY_VERSION[ \t]*=[ \t]*@")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/macos/ffmpeg_kit_next_flutter.podspec" "Flutter macOS podspec version" 'BEGIN { $count = 0 } $count += s/(s\.version[ \t]*=[ \t]*'\''\s*)[^'\'']+('\''\s*)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/flutter/flutter/macos/ffmpeg_kit_next_flutter/Sources/ffmpeg_kit_next_flutter/FFmpegKitFlutterPlugin.m" "Flutter macOS plugin library version" 'BEGIN { $count = 0 } $count += s/(static NSString \*const LIBRARY_VERSION[ \t]*=[ \t]*@")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
}

update_react_native() {
  replace_in_file "${BASEDIR}/react-native/package.json" "React Native package version" 'BEGIN { $count = 0 } $count += s/("version"[ \t]*:[ \t]*")[^"]+(",)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/react-native/android/build.gradle" "React Native Android version" 'BEGIN { $count = 0 } $count += s/(versionCode[ \t]+)[0-9]+/${1}$ENV{PACKAGE_VERSION_CODE}/g; $count += s/(versionName[ \t]+")[^"]+(")/${1}$ENV{NEW_VERSION}${2}/g; $count += s/(implementation '\''com\.arthenica:ffmpeg-kit-next:)[^'\'']+('\''\s*)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count >= 3 ? 0 : 3) }'
  replace_in_file "${BASEDIR}/react-native/android/src/main/java/com/arthenica/ffmpegkit/reactnative/FFmpegKitReactNativeModule.java" "React Native Android module library version" 'BEGIN { $count = 0 } $count += s/(public static final String LIBRARY_VERSION[ \t]*=[ \t]*")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/react-native/src/index.js" "React Native JavaScript version" 'BEGIN { $count = 0 } $count += s/(static getVersion\(\)[ \t\r\n]*\{[ \t\r\n]*return ")[^"]+(";[ \t\r\n]*\})/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
  replace_in_file "${BASEDIR}/react-native/ios/FFmpegKitReactNativeModule.mm" "React Native iOS module library version" 'BEGIN { $count = 0 } $count += s/(static NSString \*const LIBRARY_VERSION[ \t]*=[ \t]*@")[^"]+(";)/${1}$ENV{NEW_VERSION}${2}/g; END { exit($count ? 0 : 3) }'
}

printf 'Updating version to %s\n' "$NEW_VERSION"
printf 'Selected platforms: %s\n' "$SELECTED_PLATFORMS"
printf '\n'

if is_selected android; then
  OLD_ANDROID_VERSION=$(extract_android_version)
  OLD_ANDROID_VERSION_CODE=$(extract_android_version_code)
  update_android
  printf '%-13s version     %s -> %s\n' "android" "$OLD_ANDROID_VERSION" "$(extract_android_version)"
  printf '%-13s versionCode %s -> %s\n' "android" "$OLD_ANDROID_VERSION_CODE" "$(extract_android_version_code)"
fi

if is_selected apple; then
  OLD_APPLE_VERSION=$(extract_apple_version)
  update_apple
  printf '%-13s version     %s -> %s\n' "apple" "$OLD_APPLE_VERSION" "$(extract_apple_version)"
fi

if is_selected linux; then
  OLD_LINUX_VERSION=$(extract_linux_version)
  update_linux
  printf '%-13s version     %s -> %s\n' "linux" "$OLD_LINUX_VERSION" "$(extract_linux_version)"
fi

if is_selected flutter; then
  OLD_FLUTTER_VERSION=$(extract_flutter_version)
  OLD_FLUTTER_VERSION_CODE=$(extract_flutter_version_code)
  update_flutter
  printf '%-13s version     %s -> %s\n' "flutter" "$OLD_FLUTTER_VERSION" "$(extract_flutter_version)"
  printf '%-13s versionCode %s -> %s\n' "flutter" "$OLD_FLUTTER_VERSION_CODE" "$(extract_flutter_version_code)"
fi

if is_selected react-native; then
  OLD_REACT_NATIVE_VERSION=$(extract_react_native_version)
  OLD_REACT_NATIVE_VERSION_CODE=$(extract_react_native_version_code)
  update_react_native
  printf '%-13s version     %s -> %s\n' "react-native" "$OLD_REACT_NATIVE_VERSION" "$(extract_react_native_version)"
  printf '%-13s versionCode %s -> %s\n' "react-native" "$OLD_REACT_NATIVE_VERSION_CODE" "$(extract_react_native_version_code)"
fi
