#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "ERROR: Usage: $0 <repo-root> <ffmpeg-source-dir>" >&2
  exit 1
fi

BASEDIR="$1"
FFMPEG_SOURCE_DIR="$2"
PLATFORM_DIR="linux"
FFKIT_PROTOCOLS=(ffkitmem ffkitstream)
HOST_TEST_CFLAGS=()
TEST_SOURCE="${BASEDIR}/tools/protocols/test/ffkit_protocol_id_test.c"
BUILD_DIR="${BASEDIR}/.tmp/ffkitprotocols/${PLATFORM_DIR}/tests"
HOST_CC="${HOST_CC:-cc}"

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

has_protocol() {
  local wanted="$1"
  local protocol

  for protocol in "${FFKIT_PROTOCOLS[@]}"; do
    if [[ "${protocol}" == "${wanted}" ]]; then
      return 0
    fi
  done

  return 1
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

if has_protocol "ffkitmem" || has_protocol "ffkitstream"; then
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavformat/file.c" \
    "static int ffkit_parse_protocol_id" \
    "ffkit protocol id parser"
fi

if has_protocol "ffkitsaf"; then
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavformat/file.c" \
    "static int saf_open" \
    "ffkitsaf open implementation"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
    "void av_set_saf_open" \
    "ffkitsaf open setter declaration"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
    "void av_set_saf_close" \
    "ffkitsaf close setter declaration"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.c" \
    "saf_open_function _saf_open_function" \
    "ffkitsaf open callback storage"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.c" \
    "saf_close_function _saf_close_function" \
    "ffkitsaf close callback storage"
fi

if has_protocol "ffkitmem"; then
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
    "void av_set_ffkitmem_functions" \
    "ffkitmem function setter declaration"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.c" \
    "ffkit_protocol_open_function _ffkitmem_open_function" \
    "ffkitmem callback storage"
fi

if has_protocol "ffkitstream"; then
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.h" \
    "void av_set_ffkitstream_functions" \
    "ffkitstream function setter declaration"
  check_contains \
    "${FFMPEG_SOURCE_DIR}/libavutil/file.c" \
    "ffkit_protocol_open_function _ffkitstream_open_function" \
    "ffkitstream callback storage"
fi

echo -e "INFO: Running host-only ffmpeg-kit protocol parser/runtime tests\n"

check_file_exists "${TEST_SOURCE}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

HOST_COMPILE_ARGS=(
  -std=c99
  -Wall
  -Wextra
  -Werror
)

if ((${#HOST_TEST_CFLAGS[@]} > 0)); then
  HOST_COMPILE_ARGS+=("${HOST_TEST_CFLAGS[@]}")
fi

HOST_COMPILE_ARGS+=(
  -I"${BASEDIR}/src/ffmpeg"
  "${TEST_SOURCE}"
  -o "${BUILD_DIR}/ffkit_protocol_id_test"
)

"${HOST_CC}" "${HOST_COMPILE_ARGS[@]}"

"${BUILD_DIR}/ffkit_protocol_id_test"
