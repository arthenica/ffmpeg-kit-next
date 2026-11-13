#!/bin/bash

MOLTENVK_SOURCE_DIR="${BASEDIR}/src/${LIB_NAME}"
MOLTENVK_PATCH_DIR=$(get_library_patch_dir "${LIB_NAME}") || return 1
MOLTENVK_EXTERNAL_DIR="${MOLTENVK_SOURCE_DIR}/External"
MOLTENVK_CPM_VERSION="0.40.8"
MOLTENVK_CPM_SOURCE_CACHE="${BUILD_DIR}/cpm-cache"

# CLONE THE DEPENDENCIES FROM THE ARTHENICA FORKS AT THE COMMITS MOLTENVK PINS IN ExternalRevisions, SO THESE SCRIPTS DO NOT
# DUPLICATE ITS PINS. A DIRECTORY THAT ALREADY HOLDS THE RIGHT COMMIT FROM THE RIGHT FORK IS KEPT
for MOLTENVK_DEPENDENCY in cereal SPIRV-Cross SPIRV-Tools SPIRV-Headers Vulkan-Headers; do
  MOLTENVK_DEPENDENCY_REVISION_FILE="${MOLTENVK_SOURCE_DIR}/ExternalRevisions/${MOLTENVK_DEPENDENCY}_repo_revision"
  MOLTENVK_DEPENDENCY_URL="https://github.com/arthenica/${MOLTENVK_DEPENDENCY}"
  MOLTENVK_DEPENDENCY_PATH="${MOLTENVK_EXTERNAL_DIR}/${MOLTENVK_DEPENDENCY}"
  MOLTENVK_DEPENDENCY_COMMIT=$(tr -d '[:space:]' <"${MOLTENVK_DEPENDENCY_REVISION_FILE}")

  if [[ -z ${MOLTENVK_DEPENDENCY_COMMIT} ]]; then
    echo -e "ERROR: Failed to read the ${MOLTENVK_DEPENDENCY} revision from ${MOLTENVK_DEPENDENCY_REVISION_FILE}\n"
    return 1
  fi

  if [[ $(git -C "${MOLTENVK_DEPENDENCY_PATH}" rev-parse HEAD 2>/dev/null) == "${MOLTENVK_DEPENDENCY_COMMIT}" ]] &&
    [[ $(git -C "${MOLTENVK_DEPENDENCY_PATH}" remote get-url origin 2>/dev/null) == "${MOLTENVK_DEPENDENCY_URL}" ]]; then
    echo -e "INFO: ${MOLTENVK_DEPENDENCY_PATH} already holds commit ${MOLTENVK_DEPENDENCY_COMMIT}\n"
    continue
  fi

  rm -rf "${MOLTENVK_DEPENDENCY_PATH}" || return 1

  if [[ $(clone_git_repository_with_commit_id "${MOLTENVK_DEPENDENCY_URL}" "${MOLTENVK_DEPENDENCY_PATH}" "${MOLTENVK_DEPENDENCY_COMMIT}") -ne 0 ]]; then
    echo -e "ERROR: Failed to clone ${MOLTENVK_DEPENDENCY_URL} at ${MOLTENVK_DEPENDENCY_COMMIT} into ${MOLTENVK_DEPENDENCY_PATH}\n"
    return 1
  fi
done

# PATCH THE CMAKE BUILD OF MOLTENVK, WHICH BUILDS ONLY A macOS SHARED LIBRARY OTHERWISE: (a) A STATIC LIBRARY, (b) UIKit
# INSTEAD OF AppKit OUTSIDE macOS AND AN OPTIONAL IOKit. A PATCH THAT NO LONGER APPLIES AFTER A MOLTENVK UPDATE IS THE
# SIGNAL TO CHECK BOTH AGAIN
apply_moltenvk_patch() {
  local PATCH_NAME="$1"
  local PATCH_FILE="${MOLTENVK_PATCH_DIR}/${PATCH_NAME}"

  if git -C "${MOLTENVK_SOURCE_DIR}" apply --check "${PATCH_FILE}" 2>/dev/null; then
    git -C "${MOLTENVK_SOURCE_DIR}" apply "${PATCH_FILE}" || return 1
  elif git -C "${MOLTENVK_SOURCE_DIR}" apply --reverse --check "${PATCH_FILE}" 2>/dev/null; then
    echo -e "INFO: skipping moltenvk ${PATCH_NAME}, already applied\n" 1>>"${BASEDIR}"/build.log 2>&1
  else
    echo -e "ERROR: moltenvk ${PATCH_NAME} does not apply cleanly\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi
}

apply_moltenvk_patch "MoltenVK.static-library.patch" || return 1
apply_moltenvk_patch "Common.apple-platforms.patch" || return 1

# SPV_EXPECT IS AN EXPRESSION MACRO. ITS TRAILING SEMICOLON BREAKS METAL SHADERS THAT USE OpExpectKHR IN CONDITIONS
apply_moltenvk_patch "SPIRV-Cross.expect-expression.patch" || return 1

# THE CMAKE BUILD OF MOLTENVK DOWNLOADS ITS CPM.cmake BOOTSTRAP SCRIPT UNLESS THE CPM CACHE ALREADY HOLDS IT. SEED THE CACHE
# WITH THE VENDORED COPY (THE UPSTREAM RELEASE ASSET), SO THE BUILD DOES NOT TOUCH THE NETWORK. A MOLTENVK UPDATE
# THAT ASKS FOR ANOTHER CPM VERSION WOULD DOWNLOAD IT, SO THAT IS AN ERROR
if ! grep -q "^set(CPM_DOWNLOAD_VERSION ${MOLTENVK_CPM_VERSION})$" "${MOLTENVK_SOURCE_DIR}/cmake/recipes/CPM.cmake"; then
  echo -e "ERROR: ${LIB_NAME} no longer uses CPM ${MOLTENVK_CPM_VERSION}, see ${MOLTENVK_SOURCE_DIR}/cmake/recipes/CPM.cmake\n"
  return 1
fi

mkdir -p "${MOLTENVK_CPM_SOURCE_CACHE}/cpm" || return 1
cp "${MOLTENVK_PATCH_DIR}/CPM_${MOLTENVK_CPM_VERSION}.cmake" "${MOLTENVK_CPM_SOURCE_CACHE}/cpm/CPM_${MOLTENVK_CPM_VERSION}.cmake" || return 1

# THE TARGET: CMAKE_SYSTEM_NAME (Darwin FOR macOS AND MAC CATALYST, iOS, tvOS, visionOS), CMAKE_OSX_SYSROOT (macosx, iphoneos,
# iphonesimulator, appletvos, appletvsimulator, xros, xrsimulator; macosx FOR MAC CATALYST) AND CMAKE_OSX_ARCHITECTURES COME
# FROM get_apple_cmake_system_name (run-apple.sh), get_sdk_name AND get_cmake_osx_architectures. THE FLAGS CARRY -target AND
# THE MINIMUM VERSION, FOR MAC CATALYST <arch>-apple-ios<version>-macabi AND THE iOSSupport FRAMEWORKS (-iframework)
MOLTENVK_CMAKE_OPTIONS=()
case ${ARCH} in
*-mac-catalyst)
  ;;
*)
  MOLTENVK_CMAKE_OPTIONS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=$(get_min_sdk_version)")
  ;;
esac

CPM_SOURCE_CACHE="${MOLTENVK_CPM_SOURCE_CACHE}" cmake -Wno-dev -G Ninja \
  -S "${MOLTENVK_SOURCE_DIR}" \
  -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DSPIRV_SKIP_EXECUTABLES=ON \
  -DSPIRV_SKIP_TESTS=ON \
  "${MOLTENVK_CMAKE_OPTIONS[@]}" \
  -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
  -DCMAKE_OSX_SYSROOT="$(get_sdk_name)" \
  -DCMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_OBJC_COMPILER="$CC" \
  -DCMAKE_OBJCXX_COMPILER="$CXX" \
  -DCMAKE_AR="$(xcrun --sdk $(get_sdk_name) -f ar)" \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_OBJC_FLAGS="${CFLAGS}" \
  -DCMAKE_OBJCXX_FLAGS="${CXXFLAGS}" \
  -DCPM_SOURCE_CACHE="${MOLTENVK_CPM_SOURCE_CACHE}" \
  -DCPM_cereal_SOURCE="${MOLTENVK_EXTERNAL_DIR}/cereal" \
  -DCPM_SPIRV-Cross_SOURCE="${MOLTENVK_EXTERNAL_DIR}/SPIRV-Cross" \
  -DCPM_SPIRV-Tools_SOURCE="${MOLTENVK_EXTERNAL_DIR}/SPIRV-Tools" \
  -DCPM_SPIRV-Headers_SOURCE="${MOLTENVK_EXTERNAL_DIR}/SPIRV-Headers" \
  -DCPM_Vulkan-Headers_SOURCE="${MOLTENVK_EXTERNAL_DIR}/Vulkan-Headers" || return 1

ninja -C "${BUILD_DIR}" -j$(get_cpu_count) || return 1

# PRELINK MOLTENVK AND THE STATIC LIBRARIES IT DEPENDS ON INTO ONE OBJECT, AS THE XCODE BUILD OF MOLTENVK DOES
# (GENERATE_MASTER_OBJECT_FILE), AND ARCHIVE THAT OBJECT ALONE. THE LINKER THEN TAKES ALL OF MOLTENVK OR NOTHING: THE
# OBJECTIVE-C CATEGORY MEMBERS, WHICH NO SYMBOL REFERENCES, ARE NOT DROPPED, AND THE SPIRV-Tools CALLS OF MOLTENVK BIND
# TO ITS OWN SPIRV-Tools, NOT TO THE COPY IN libshaderc_combined.a. THE MEMBERS ARE EXTRACTED FIRST, BECAUSE ld -r READS
# THE TARGET PLATFORM AND MINIMUM VERSION FROM THE OBJECTS (FROM ARCHIVES ALONE IT WOULD NEED -platform_version)
MOLTENVK_PRELINK_DIR="${BUILD_DIR}/prelink"
MOLTENVK_OBJECTS=()
MOLTENVK_ARCHIVE_INDEX=0

rm -rf "${MOLTENVK_PRELINK_DIR}" || return 1

for MOLTENVK_ARCHIVE in \
  MoltenVK/libMoltenVK.a \
  Common/libMoltenVK_Common.a \
  MoltenVKShaderConverter/MoltenVKShaderConverter/libMoltenVK_ShaderConverter.a \
  _deps/spirv-cross-build/libspirv-cross-core.a \
  _deps/spirv-cross-build/libspirv-cross-glsl.a \
  _deps/spirv-cross-build/libspirv-cross-msl.a \
  _deps/spirv-cross-build/libspirv-cross-reflect.a \
  _deps/spirv-tools-build/source/libSPIRV-Tools.a; do
  if [[ ! -f "${BUILD_DIR}/${MOLTENVK_ARCHIVE}" ]]; then
    echo -e "ERROR: ${BUILD_DIR}/${MOLTENVK_ARCHIVE} not found\n"
    return 1
  fi

  # EACH ARCHIVE GOES TO ITS OWN DIRECTORY, MEMBER NAMES REPEAT ACROSS ARCHIVES. A NAME REPEATED INSIDE ONE ARCHIVE WOULD BE
  # OVERWRITTEN ON EXTRACTION, SO THAT IS AN ERROR
  MOLTENVK_ARCHIVE_INDEX=$((MOLTENVK_ARCHIVE_INDEX + 1))
  MOLTENVK_MEMBER_DIR="${MOLTENVK_PRELINK_DIR}/${MOLTENVK_ARCHIVE_INDEX}"
  MOLTENVK_MEMBERS=$("${AR}" -t "${BUILD_DIR}/${MOLTENVK_ARCHIVE}" | grep -v '^__\.SYMDEF') || return 1

  if [[ -n $(echo "${MOLTENVK_MEMBERS}" | sort | uniq -d) ]]; then
    echo -e "ERROR: ${BUILD_DIR}/${MOLTENVK_ARCHIVE} has members with the same name\n"
    return 1
  fi

  mkdir -p "${MOLTENVK_MEMBER_DIR}" || return 1
  (cd "${MOLTENVK_MEMBER_DIR}" && "${AR}" -x "${BUILD_DIR}/${MOLTENVK_ARCHIVE}") || return 1

  while read -r MOLTENVK_MEMBER; do
    MOLTENVK_OBJECTS+=("${MOLTENVK_MEMBER_DIR}/${MOLTENVK_MEMBER}")
  done <<<"${MOLTENVK_MEMBERS}"
done

printf '%s\n' '*spv*' >"${MOLTENVK_PRELINK_DIR}/MoltenVK.unexported" || return 1

"${LD}" -r -arch "$(get_cmake_osx_architectures)" -unexported_symbols_list "${MOLTENVK_PRELINK_DIR}/MoltenVK.unexported" \
  -o "${MOLTENVK_PRELINK_DIR}/MoltenVK.o" "${MOLTENVK_OBJECTS[@]}" || return 1

mkdir -p "${LIB_INSTALL_PREFIX}/lib" "${LIB_INSTALL_PREFIX}/include/MoltenVK" || return 1

"$(xcrun --sdk "$(get_sdk_name)" -f libtool)" -static -o "${LIB_INSTALL_PREFIX}/lib/libMoltenVK.a" "${MOLTENVK_PRELINK_DIR}/MoltenVK.o" || return 1

# THE MOLTENVK HEADERS (mvk_vulkan.h, mvk_config.h, ...). THE VULKAN HEADERS COME FROM vulkan-headers
cp "${MOLTENVK_SOURCE_DIR}"/MoltenVK/include/MoltenVK/*.h "${LIB_INSTALL_PREFIX}/include/MoltenVK" || return 1

# REWRITE THE vulkan.pc OF vulkan-headers, KEEPING ITS VERSION AND HEADERS
VULKAN_VERSION=$(pkg-config --modversion vulkan 2>/dev/null)

if [[ -z ${VULKAN_VERSION} ]]; then
  echo -e "ERROR: Failed to read the version of vulkan.pc in ${INSTALL_PKG_CONFIG_DIR}\n"
  return 1
fi

create_moltenvk_package_config "${VULKAN_VERSION}" || return 1
