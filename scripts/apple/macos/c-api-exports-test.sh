#!/usr/bin/env bash
#
# Checks that the C API header of one macOS build declares exactly the functions
# its libffmpegkit exports: nothing declared but missing, nothing exported but
# undeclared.
#
#   scripts/apple/macos/c-api-exports-test.sh <ffmpeg-kit-directory>
#
#   <ffmpeg-kit-directory>  where ffmpeg-kit is installed for one architecture,
#                           prebuilt/apple-macos-<architecture>-<min version>/ffmpeg-kit.
#                           It has to hold lib/libffmpegkit.dylib and
#                           include/ffmpegkit_c.h.
#
# Nothing is run. The header is read and the symbol table of the library is
# listed, so the build of either architecture is checked the same way on either
# kind of Mac, with no Rosetta. macos.sh runs it for each architecture right
# after ffmpeg-kit has been installed.
#
# Exit status: 0 they match, 1 they differ, 2 the build cannot be checked.

set -euo pipefail

DIRECTORY="${1:-}"

if [[ -z "${DIRECTORY}" ]]; then
  echo "usage: $0 <ffmpeg-kit-directory>" >&2
  exit 2
fi

LIBRARY="${DIRECTORY}/lib/libffmpegkit.dylib"
HEADER="${DIRECTORY}/include/ffmpegkit_c.h"

for required in "${LIBRARY}" "${HEADER}"; do
  if [[ ! -e "${required}" ]]; then
    echo "cannot check the C API of ${DIRECTORY}: ${required} does not exist" >&2
    exit 2
  fi
done

# A build holds one architecture. nm would list the symbols of every slice of a
# universal library, one section after the other.
ARCHITECTURES="$(lipo -archs "${LIBRARY}")"
if [[ "${ARCHITECTURES}" == *" "* ]]; then
  echo "cannot check the C API of ${DIRECTORY}: ${LIBRARY} has more than one architecture (${ARCHITECTURES})" >&2
  exit 2
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/ffk-c-api.XXXXXX")"
trap 'rm -rf "${WORK}"' EXIT

# What the header declares: every ffk_ name that is followed by a parenthesis,
# once the comments, which mention functions, are gone. Function pointer types
# have the parenthesis in front of the name and do not count.
perl -0pe 's{/\*.*?\*/}{}gs; s{//[^\n]*}{}g' "${HEADER}" |
  { grep -oE '\bffk_[a-z0-9_]+[[:space:]]*\(' || true; } | tr -d ' (' | sort -u >"${WORK}/declared.txt"

# What the library exports: the defined external symbols, without the underscore
# that C adds in front of every name.
nm -gU "${LIBRARY}" | awk '{ print $NF }' | sed 's/^_//' | { grep '^ffk_' || true; } | sort -u >"${WORK}/exported.txt"

if [[ ! -s "${WORK}/declared.txt" ]]; then
  # The C API is part of every macOS build. An empty header would otherwise match
  # a library that has none.
  echo "${HEADER} declares no ffk_ function" >&2
  exit 1
fi

MISSING="$(comm -23 "${WORK}/declared.txt" "${WORK}/exported.txt")"
UNDECLARED="$(comm -13 "${WORK}/declared.txt" "${WORK}/exported.txt")"

STATUS=0
if [[ -n "${MISSING}" ]]; then
  echo "ERROR: declared in ${HEADER} but not exported by ${LIBRARY}:"
  echo "${MISSING}" | sed 's/^/  /'
  STATUS=1
fi
if [[ -n "${UNDECLARED}" ]]; then
  echo "ERROR: exported by ${LIBRARY} but not declared in ${HEADER}:"
  echo "${UNDECLARED}" | sed 's/^/  /'
  STATUS=1
fi
if [[ "${STATUS}" -eq 0 ]]; then
  echo "INFO: C API of ${DIRECTORY} (${ARCHITECTURES}): $(wc -l <"${WORK}/declared.txt" | tr -d ' ') functions declared, all exported, none undeclared"
fi

exit "${STATUS}"
