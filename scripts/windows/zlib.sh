#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

export AR="${AR}"
export RANLIB="${RANLIB}"

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --static || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_zlib_package_config "1.3.2" || return 1
