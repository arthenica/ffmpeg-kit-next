#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# shine DECLARES shine_mdct_initialise() WITH AN EMPTY PARAMETER LIST IN
# l3mdct.h BUT DEFINES IT WITH A shine_global_config* PARAMETER IN l3mdct.c.
# THAT IS AN UNPROTOTYPED DECLARATION UP TO C17 AND HARMLESS, BUT IN C23 AN
# EMPTY PARAMETER LIST MEANS (void), SO THE TWO BECOME CONFLICTING TYPES.
# autoconf 2.72 SELECTS C23 ON ITS OWN - IT PROBES THE COMPILER AND BAKES
# -std=gnu23 INTO CC - SO THE ERROR APPEARS EVEN THOUGH NOTHING HERE ASKS FOR
# C23. DECLARE THE PARAMETER SO THE HEADER MATCHES THE DEFINITION.
# THE git checkout KEEPS THE PATCH IDEMPOTENT ACROSS REBUILDS.
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/src/lib/l3mdct.h 1>>"${BASEDIR}"/build.log 2>&1
${SED_INLINE} 's|^void shine_mdct_initialise();|void shine_mdct_initialise(shine_global_config *config);|g' "${BASEDIR}"/src/"${LIB_NAME}"/src/lib/l3mdct.h || return 1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_shine} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-sysroot="${SDK_PATH}" \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
