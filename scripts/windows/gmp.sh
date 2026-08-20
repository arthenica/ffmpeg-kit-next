#!/bin/bash

# gmp HAS NO OPTION TO SKIP ITS MANUAL. IT DOES SHIP doc/gmp.info, BUT git DOES
# NOT PRESERVE TIMESTAMPS, SO AFTER A CLONE gmp.texi IS OFTEN A FEW MILLISECONDS
# NEWER THAN gmp.info AND make REGENERATES THE MANUAL. THAT NEEDS makeinfo,
# WHICH IS NOT ALWAYS INSTALLED (IT IS NOT PART OF A MINIMAL MSYS2 INSTALL).
# POINTING MAKEINFO AT true SKIPS THE REGENERATION AND LEAVES THE LIBRARY
# ITSELF UNAFFECTED.
export MAKEINFO=true

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_gmp} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-assembly \
  --disable-shared \
  --disable-fast-install \
  --disable-maintainer-mode \
  --host="${HOST}" || return 1

# MAKEINFO IS ALSO PASSED ON THE COMMAND LINE SO IT OVERRIDES THE VALUE BAKED
# INTO THE GENERATED Makefiles IN EVERY RECURSIVE SUBMAKE
make -j$(get_cpu_count) MAKEINFO=true || return 1

make install MAKEINFO=true || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_gmp_package_config "6.3.0" || return 1
