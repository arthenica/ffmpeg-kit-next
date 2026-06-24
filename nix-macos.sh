#!/usr/bin/env bash

set -euo pipefail

usage() {
  set +u
  export BASEDIR="${script_dir}"
  export FFMPEG_KIT_BUILD_TYPE="${platform}"
  export FFMPEG_KIT_NIX_HELP="1"
  source "${script_dir}/scripts/function.sh"
  source "${script_dir}/scripts/help-${platform}.sh"
  display_help
}

list_profiles() {
  cd "${script_dir}"
  export NIX_USER_CONF_FILES="${script_dir}/nix.conf"

  local current_system
  current_system="$(nix eval --impure --raw --expr 'builtins.currentSystem')"

  nix eval --raw ".#devShells.${current_system}" \
    --apply 'profiles: builtins.concatStringsSep "\n" (builtins.attrNames profiles)'
  printf '\n'
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
wrapper_name="$(basename "${BASH_SOURCE[0]}")"
platform="${wrapper_name#nix-}"
platform="${platform%.sh}"
start_script="./scripts/start-${platform}.sh"
help_command="./${wrapper_name}"
profile=""
build_args=()

while (($# > 0)); do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    --list-profiles)
      list_profiles
      exit 0
      ;;
    -p | --profile)
      if (($# < 2)); then
        echo "error: $1 requires a profile argument." >&2
        usage >&2
        exit 1
      fi
      profile="$2"
      shift 2
      ;;
    --profile=*)
      profile="${1#--profile=}"
      shift
      ;;
    --)
      shift
      build_args+=("$@")
      break
      ;;
    *)
      build_args+=("$1")
      shift
      ;;
  esac
done

if [[ -z "${profile}" ]]; then
  echo "error: a nix profile is required. pass it with -p or --profile." >&2
  usage >&2
  exit 1
fi

case "${profile}" in
  .* | /* | git+* | github:* | path:* | flake:*)
    nix_profile="${profile}"
    ;;
  \#*)
    nix_profile=".${profile}"
    ;;
  *)
    nix_profile=".#${profile}"
    ;;
esac

cd "${script_dir}"
export NIX_USER_CONF_FILES="${script_dir}/nix.conf"
export FFMPEG_KIT_HELP_COMMAND="${help_command}"

set +u
exec nix develop "${nix_profile}" -c "${start_script}" "${build_args[@]}"
