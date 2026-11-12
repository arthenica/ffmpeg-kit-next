#!/bin/bash

VULKAN_HEADERS_SOURCE_DIR="${BASEDIR}/src/${LIB_NAME}"
VULKAN_CORE_HEADER="${VULKAN_HEADERS_SOURCE_DIR}/include/vulkan/vulkan_core.h"

# INSTALL THE HEADERS
mkdir -p "${LIB_INSTALL_PREFIX}/include" || return 1
cp -R "${VULKAN_HEADERS_SOURCE_DIR}/include/." "${LIB_INSTALL_PREFIX}/include" || return 1

# READ THE VERSION FROM THE HEADERS, e.g. VK_MAKE_API_VERSION(0, 1, 4, VK_HEADER_VERSION) AND VK_HEADER_VERSION 363 -> 1.4.363
VULKAN_API_VERSION=$(sed -n 's/^#define VK_HEADER_VERSION_COMPLETE VK_MAKE_API_VERSION(0, \([0-9]*\), \([0-9]*\), VK_HEADER_VERSION)$/\1.\2/p' "${VULKAN_CORE_HEADER}")
VULKAN_HEADER_VERSION=$(sed -n 's/^#define VK_HEADER_VERSION \([0-9]*\)$/\1/p' "${VULKAN_CORE_HEADER}")

if [[ -z ${VULKAN_API_VERSION} ]] || [[ -z ${VULKAN_HEADER_VERSION} ]]; then
  echo -e "ERROR: Failed to read the Vulkan header version from ${VULKAN_CORE_HEADER}\n"
  return 1
fi

# CREATE PACKAGE CONFIG MANUALLY
create_vulkan_package_config "${VULKAN_API_VERSION}.${VULKAN_HEADER_VERSION}" || return 1
