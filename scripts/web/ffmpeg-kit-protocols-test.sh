#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "ERROR: Usage: $0 <repo-root> <ffmpeg-source-dir>" >&2
  exit 1
fi

BASEDIR="$1"
FFMPEG_SOURCE_DIR="$2"
FFKIT_PROTOCOLS=(ffkitmem ffkitstream)

fail() {
  echo "ERROR: $1" >&2
  exit 1
}

check_file_exists() {
  local file="$1"

  [[ -f "${file}" ]] || fail "Expected file not found: ${file}"
}

check_contains() {
  local file="$1"
  local pattern="$2"
  local description="$3"

  check_file_exists "${file}"

  if ! grep -Fq "${pattern}" "${file}"; then
    fail "Missing ${description} in ${file}"
  fi
}

check_protocol_patch() {
  local protocol="$1"

  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavformat/file.c" \
    "const URLProtocol ff_${protocol}_protocol" \
    "${protocol} URLProtocol"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavformat/protocols.c" \
    "extern const URLProtocol ff_${protocol}_protocol;" \
    "${protocol} protocol declaration"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavformat/hls.c" \
    "${protocol}" \
    "${protocol} HLS allow-list entry"
}

echo -e "INFO: Validating custom ffmpeg-kit protocol source patches\n"

for protocol in "${FFKIT_PROTOCOLS[@]}"; do
  check_protocol_patch "${protocol}"
done

check_contains \
  "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
  "void av_set_ffkitmem_functions" \
  "ffkitmem function setter declaration"

check_contains \
  "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
  "void av_set_ffkitstream_functions" \
  "ffkitstream function setter declaration"
