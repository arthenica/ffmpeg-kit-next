# ffmpeg-kit-next 9.0.0 configure.ac (windows)

AC_INIT([ffmpeg-kit-next], [9.0.0], [https://github.com/arthenica/ffmpeg-kit-next/issues/new])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ffmpegkit_c.cpp])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_MAINTAINER_MODE

# Check for os version
VERSION_OS=$(uname -s)
AC_DEFINE_UNQUOTED(VERSION_OS, "$VERSION_OS", [os version])

# Check host so we can infer target CPU (and assembly optimizations)
AC_CANONICAL_HOST

# Check for processor characteristics
AC_C_BIGENDIAN(AC_DEFINE([HIGHFIRST], [1], [big-endian machine]))

# Checks for programs.
cflags_bckup="$CFLAGS"
AC_PROG_CC(clang gcc)
AC_LANG([C++])
AC_PROG_CXX(clang++ g++)
AX_CXX_COMPILE_STDCXX(11, noext)
AM_PROG_AR
CFLAGS="$cflags_bckup"

FFMPEG_LIBS="-lavcodec -lavfilter -lavformat -lavutil -lswscale -lswresample"
AC_SUBST(FFMPEG_LIBS)

# CURRENT INTERFACE:REVISION:AGE
VERSION_INFO="12:0:0"
AC_SUBST(VERSION_INFO)

LT_INIT([win32-dll])

# Link the MinGW toolchain runtime - libc++, libunwind and libwinpthread - into
# the dll instead of leaving it as a runtime dependency.
#
# Without this libffmpegkit imports libc++.dll, libunwind.dll and
# libwinpthread-1.dll, which only exist inside an MSYS2 installation. The
# consumers this port targets build with MSVC and have no MinGW toolchain at
# all, so a dll that needs them loads on the build machine and fails everywhere
# else with STATUS_DLL_NOT_FOUND.
#
# A private, statically linked libc++ is safe here because libffmpegkit exports
# only the flat C API in ffmpegkit_c.h: no C++ object, allocation or exception
# crosses the dll boundary, so nothing can bind to a different libc++ that the
# consumer happens to ship.
AC_ARG_ENABLE([static-mingw-runtime],
  [AS_HELP_STRING([--disable-static-mingw-runtime],
    [link libc++, libunwind and libwinpthread as dlls instead of statically @<:@default=static@:>@])],
  [], [enable_static_mingw_runtime=yes])

# Windows system libraries needed to build the shared library:
#   - winpthread provides the pthread API used by the C++ layer
#   - bcrypt provides the CNG primitives used for secure random data
WINDOWS_LIBS="-lbcrypt"

if test "x$enable_static_mingw_runtime" = "xyes"; then

  # Each archive is resolved through the compiler rather than hardcoded, so the
  # right one is picked up whichever MSYS2 environment is in use.
  #
  # They are named as -Wl,<path> and land in LIBADD, which libtool emits after
  # the object files and before its own runtime list. Both details matter. A
  # bare path is silently dropped, because libtool treats it as a library it
  # owns and relocates it off the link line; and an archive that precedes the
  # objects resolves nothing, because the archive scan is single pass.
  FFMPEGKIT_STATIC_RUNTIME=""
  for ffmpegkit_runtime_archive in libc++.a libunwind.a; do
    ffmpegkit_archive_path=`$CXX -print-file-name=$ffmpegkit_runtime_archive 2>/dev/null`
    AS_IF([test -f "$ffmpegkit_archive_path"],
      [FFMPEGKIT_STATIC_RUNTIME="$FFMPEGKIT_STATIC_RUNTIME -Wl,$ffmpegkit_archive_path"],
      [AC_MSG_WARN([$ffmpegkit_runtime_archive not found, its dll stays a runtime dependency])])
  done

  # winpthread additionally needs --whole-archive. libc++ itself calls
  # pthread_create and is linked ahead of it, so a demand driven scan reads
  # winpthread, finds nothing pending yet and discards it - the link then fails
  # on undefined pthread_*. Pulling every member in unconditionally means the
  # definitions are present no matter who asks for them later.
  ffmpegkit_winpthread=`$CXX -print-file-name=libwinpthread.a 2>/dev/null`
  AS_IF([test -f "$ffmpegkit_winpthread"],
    [WINDOWS_LIBS="-Wl,--whole-archive,$ffmpegkit_winpthread,--no-whole-archive $WINDOWS_LIBS"],
    [AC_MSG_WARN([libwinpthread.a not found, libwinpthread-1.dll stays a runtime dependency])
     WINDOWS_LIBS="-lpthread $WINDOWS_LIBS"])

  WINDOWS_LIBS="$FFMPEGKIT_STATIC_RUNTIME $WINDOWS_LIBS"

  # libtool links the shared library with -nostdlib and supplies the C++ runtime
  # itself, from the postdeps list it detected while configuring. That list names
  # -lc++ and -lunwind, which resolve to the import libraries and put the dll
  # dependency straight back. Because -nostdlib suppresses the driver's own
  # stdlib handling, -static-libstdc++ never reaches it and is reported as an
  # unused argument, so taking the shared ones out of postdeps here is what
  # actually decides it. The static archives are added through WINDOWS_LIBS
  # above; they cannot be substituted into postdeps, because libtool drops the
  # -Wl, form out of it and relocates a bare path.
  postdeps_CXX=`echo " $postdeps_CXX " | sed -e 's/ -lc++ / /g' -e 's/ -lunwind / /g'`

else
  WINDOWS_LIBS="-lpthread $WINDOWS_LIBS"
fi

AC_SUBST(WINDOWS_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([libavformat/avformat.h libavcodec/avcodec.h libavfilter/avfilter.h libavutil/avutil.h libswresample/swresample.h], [], [
  AC_MSG_ERROR([unable to find ffmpeg headers])
])
AC_CHECK_HEADERS([rapidjson/document.h], [], [
  AC_MSG_ERROR([unable to find rapidjson headers. rapidjson is downloaded by windows.sh and passed via CXXFLAGS])
])
AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([floor memmove memset select strchr strcspn strerror strrchr strstr strtol malloc strcpy strlen vsnprintf])

AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
