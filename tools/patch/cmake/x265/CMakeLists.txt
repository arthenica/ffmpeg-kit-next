# vim: syntax=cmake
if(NOT CMAKE_BUILD_TYPE)
    # default to Release build for GCC builds
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()
message(STATUS "cmake version ${CMAKE_VERSION}")

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW) # MACOSX_RPATH
endif()

cmake_minimum_required (VERSION 2.8.8...3.10) # OBJECT libraries require 2.8.8

if (POLICY CMP0075)
    cmake_policy(SET CMP0075 NEW) # CMAKE_REQUIRED_LIBRARIES warning
endif()


project (x265)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCXXCompilerFlag)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

option(FPROFILE_GENERATE "Compile executable to generate usage data" OFF)
option(FPROFILE_USE "Compile executable using generated usage data" OFF)
option(NATIVE_BUILD "Target the build CPU" OFF)
option(STATIC_LINK_CRT "Statically link C and C++ runtimes for release builds" OFF)
mark_as_advanced(FPROFILE_USE FPROFILE_GENERATE NATIVE_BUILD)
# X265_BUILD must be incremented each time the public API is changed
set(X265_BUILD 216)
configure_file("${PROJECT_SOURCE_DIR}/x265.def.in"
               "${PROJECT_BINARY_DIR}/x265.def")
configure_file("${PROJECT_SOURCE_DIR}/x265_config.h.in"
               "${PROJECT_BINARY_DIR}/x265_config.h")

SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

# System architecture detection
if (APPLE AND CMAKE_OSX_ARCHITECTURES)
    string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" SYSPROC)
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSPROC)
endif()
set(X86_ALIASES x86 i386 i686 x86_64 amd64)
set(ARM_ALIASES armv6l armv7l armv7 armv7s)
set(ARM64_ALIASES arm64 arm64e aarch64)
set(RISCV64_ALIASES riscv64)
list(FIND X86_ALIASES "${SYSPROC}" X86MATCH)
list(FIND ARM_ALIASES "${SYSPROC}" ARMMATCH)
list(FIND ARM64_ALIASES "${SYSPROC}" ARM64MATCH)
set(POWER_ALIASES powerpc64 powerpc64le ppc64 ppc64le)
list(FIND POWER_ALIASES "${SYSPROC}" POWERMATCH)
list(FIND RISCV64_ALIASES "${SYSPROC}" RISCV64MATCH)
if(X86MATCH GREATER "-1")
    set(X86 1)
    add_definitions(-DX265_ARCH_X86=1)
    if(CMAKE_CXX_FLAGS STREQUAL "-m32")
        message(STATUS "Detected x86 target processor")
    elseif("${CMAKE_SIZEOF_VOID_P}" MATCHES 8)
        set(X64 1)
        add_definitions(-DX86_64=1)
        message(STATUS "Detected x86_64 target processor")
    endif()
elseif(POWERMATCH GREATER "-1")
    message(STATUS "Detected POWER target processor")
    set(POWER 1)
    add_definitions(-DX265_ARCH_POWER=1)
    if("${CMAKE_SIZEOF_VOID_P}" MATCHES 8)
        set(PPC64 1)
        add_definitions(-DPPC64=1)
        message(STATUS "Detected POWER PPC64 target processor")
    endif()
elseif(ARMMATCH GREATER "-1")
    if(CROSS_COMPILE_ARM)
        message(STATUS "Cross compiling for ARM arch")
    else()
        set(CROSS_COMPILE_ARM 0)
    endif()
	message(STATUS "Detected ARM target processor")
    set(ARM 1)
    add_definitions(-DX265_ARCH_ARM=1)
elseif(ARM64MATCH GREATER "-1")
    message(STATUS "Detected ARM64 target processor")
    set(ARM64 1)

    option(AARCH64_WARNINGS_AS_ERRORS "Build with -Werror for AArch64 Intrinsics files" OFF)

    option(AARCH64_RUNTIME_CPU_DETECT "Enable AArch64 run-time CPU feature detection" ON)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin|Windows")
        set(AARCH64_RUNTIME_CPU_DETECT OFF CACHE BOOL "" FORCE)
        message(STATUS "Run-time CPU feature detection unsupported on this platform")
    endif()

    # Options for manually enabling/disabling AArch64 SIMD extensions.
    option(ENABLE_NEON "Enable Neon" ON)
    option(ENABLE_NEON_DOTPROD "Enable Neon DotProd" ON)
    option(ENABLE_NEON_I8MM "Enable Neon I8MM" ON)
    option(ENABLE_SVE "Enable SVE" ON)
    option(ENABLE_SVE2 "Enable SVE2" ON)
    option(ENABLE_SVE2_BITPERM "Enable SVE2 BitPerm" ON)

    # Compiler flags for AArch64 extensions.
    set(AARCH64_NEON_FLAG "-march=armv8-a")
    # Neon DotProd is mandatory from Armv8.4.
    set(AARCH64_NEON_DOTPROD_FLAG "-march=armv8.2-a+dotprod")
    # Neon I8MM is mandatory from Armv8.6.
    set(AARCH64_NEON_I8MM_FLAG "-march=armv8.2-a+dotprod+i8mm")
    set(AARCH64_SVE_FLAG "-march=armv8.2-a+dotprod+i8mm+sve")
    # SVE2 is only available from Armv9.0, and armv9-a implies +dotprod and +sve.
    set(AARCH64_SVE2_FLAG "-march=armv9-a+i8mm+sve2")
    # SVE2 BitPerm implies +dotprod, +sve, and +sve2.
    set(AARCH64_SVE2_BITPERM_FLAG "-march=armv9-a+i8mm+sve2-bitperm")
elseif(RISCV64MATCH GREATER "-1")
    message(STATUS "Detected RISCV64 target processor")
    set(RISCV64 1)

    option(RISCV64_RUNTIME_CPU_DETECT "Enable RISCV64 run-time CPU feature detection" ON)

    option(ENABLE_RVV "Enable RVV" ON)

    # Compiler flags only for riscv64 intrinsic file.
    set(RISCV64_INTRINSIC_FLAG "-march=rv64gcv")
else()
    message(STATUS "CMAKE_SYSTEM_PROCESSOR value `${CMAKE_SYSTEM_PROCESSOR}` is unknown")
    message(STATUS "Please add this value near ${CMAKE_CURRENT_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE}")
endif()

if(X64)
    option(ENABLE_CET "Enable Control-flow Enforcement" OFF)
    if(ENABLE_CET)
        add_definitions(-DENABLE_CET=1)
        list(APPEND ASM_FLAGS "-DENABLE_CET=1")
    else()
        add_definitions(-DENABLE_CET=0)
        list(APPEND ASM_FLAGS "-DENABLE_CET=0")
    endif()
endif(X64)

if(UNIX)
    list(APPEND PLATFORM_LIBS pthread)
    find_library(LIBRT rt)
    if(LIBRT)
        list(APPEND PLATFORM_LIBS rt)
    endif()
    mark_as_advanced(LIBRT)
    find_library(LIBDL dl)
    if(LIBDL)
        list(APPEND PLATFORM_LIBS dl)
    endif()
    option(ENABLE_LIBNUMA "Enable libnuma usage (Linux only)" ON)
    if(ENABLE_LIBNUMA)
        find_package(Numa)
        if(NUMA_FOUND)
            link_directories(${NUMA_LIBRARY_DIR})
            list(APPEND CMAKE_REQUIRED_LIBRARIES numa)
            list(APPEND CMAKE_REQUIRED_INCLUDES ${NUMA_INCLUDE_DIR})
            list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-L${NUMA_LIBRARY_DIR}")
            check_symbol_exists(numa_node_of_cpu numa.h NUMA_V2)
            if(NUMA_V2)
                add_definitions(-DHAVE_LIBNUMA)
                message(STATUS "libnuma found, building with support for NUMA nodes")
                list(APPEND PLATFORM_LIBS numa)
                include_directories(${NUMA_INCLUDE_DIR})
            endif()
        endif()
        mark_as_advanced(NUMA_FOUND)
    endif(ENABLE_LIBNUMA)
    option(NO_ATOMICS "Use a slow mutex to replace atomics" OFF)
    if(NO_ATOMICS)
        add_definitions(-DNO_ATOMICS=1)
    endif(NO_ATOMICS)
    find_library(VMAF vmaf)
    option(ENABLE_LIBVMAF "Enable VMAF" OFF)
    if(ENABLE_LIBVMAF)
        add_definitions(-DENABLE_LIBVMAF)
    endif()
    if(X64)
        option(ENABLE_CET "Enable Control-flow Enforcement" OFF)
        if(ENABLE_CET)
            add_definitions(-DENABLE_CET)
            list(APPEND ASM_FLAGS "-DENABLE_CET=1")
        endif()
    endif(X64)
endif(UNIX)

if((X64 AND NOT WIN32) OR ARM64 OR PPC64 OR RISCV64)
    option(ENABLE_PIC "Enable Position Independent Code" ON)
else()
    option(ENABLE_PIC "Enable Position Independent Code" OFF)
endif()

# Compiler detection
if(CMAKE_GENERATOR STREQUAL "Xcode")
  set(XCODE 1)
endif()
if(APPLE)
  add_definitions(-DMACOS=1)
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    set(CLANG 1)
endif()
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
    set(INTEL_CXX 1)
endif()
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(GCC 1)
endif()

if(INTEL_CXX AND WIN32)
    # treat icl roughly like MSVC
    set(MSVC 1)
endif()
if(MSVC)
    if(STATIC_LINK_CRT)
        set(CompilerFlags CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
        foreach(CompilerFlag ${CompilerFlags})
            string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
        endforeach()
    endif(STATIC_LINK_CRT)
    add_definitions(/W4)  # Full warnings
    add_definitions(/Ob2) # always inline
    add_definitions(/MP)  # multithreaded build

    # disable Microsofts suggestions for proprietary secure APIs
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)

    check_include_files(stdint.h HAVE_STDINT_H)
    if(NOT HAVE_STDINT_H)
        include_directories(compat/msvc)
    endif()
endif(MSVC)

check_include_files(inttypes.h HAVE_INT_TYPES_H)
if(HAVE_INT_TYPES_H)
    add_definitions(-DHAVE_INT_TYPES_H=1)
endif()

if(INTEL_CXX AND UNIX)
    set(GCC 1) # treat icpc roughly like gcc
elseif(CLANG)
    set(GCC 1) # treat clang roughly like gcc
elseif(CMAKE_COMPILER_IS_GNUCXX)
    set(GCC 1)
endif()

if(CC STREQUAL "xlc")
    message(STATUS "Use XLC compiler")
    set(XLC 1)
    set(GCC 0)
    #set(CMAKE_C_COMPILER "/usr/bin/xlc")
    #set(CMAKE_CXX_COMPILER "/usr/bin/xlc++")
    add_definitions(-D__XLC__=1)
    add_definitions(-O3 -qstrict -qhot -qaltivec)
    add_definitions(-qinline=level=10 -qpath=IL:/data/video_files/latest.tpo/)
endif()
# this option is to enable the inclusion of dynamic HDR10 library to the libx265 compilation
option(ENABLE_HDR10_PLUS "Enable dynamic HDR10 compilation" OFF)
if(MSVC AND (MSVC_VERSION LESS 1800) AND ENABLE_HDR10_PLUS)
    message(FATAL_ERROR "MSVC version 12.0 or above required to support hdr10plus")
endif()
if(WIN32 AND (MSVC_VERSION GREATER 1800))
    if(CMAKE_VERSION VERSION_LESS 3.7)
        message(FATAL_ERROR "cmake version not compatible for VS 2017. Update the cmake to versions 3.7 or above")
    endif()
endif()
if(GCC)
    add_definitions(-Wall -Wextra -Wshadow)
    add_definitions(-D__STDC_LIMIT_MACROS=1)
    if(NOT INTEL_CXX AND NOT CLANG AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
        add_definitions(-Wno-class-memaccess)
    endif()
    if(ENABLE_HDR10_PLUS)
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8")
            message(FATAL_ERROR "gcc version above 4.8 required to support hdr10plus")
        endif()
        add_definitions(-std=gnu++11)
    else()
        add_definitions(-std=gnu++98)
    endif()
    if(ENABLE_PIC)
         add_definitions(-fPIC)
    endif(ENABLE_PIC)
    if(NATIVE_BUILD)
        if(INTEL_CXX)
            add_definitions(-xhost)
        else()
            add_definitions(-march=native)
        endif()
    elseif(X86 AND NOT X64)
        string(FIND "${CMAKE_CXX_FLAGS}" "-march" marchPos)
        if(marchPos LESS "0")
            add_definitions(-march=i686)
            if(WIN32 AND NOT INTEL_CXX AND NOT CLANG AND
               CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
                add_definitions(-mpreferred-stack-boundary=2)
            endif()
        endif()
    endif()
    if(ARM AND CROSS_COMPILE_ARM)
        message(STATUS "cross compile arm")
        separate_arguments(AS_ARGUMENTS WINDOWS_COMMAND $ENV{AS_ARGUMENTS})
        separate_arguments(ARM_ARGS WINDOWS_COMMAND $ENV{ASM_FLAGS})
        # ALL IOS ARM CPUS HAVE NEON
        add_definitions(-DHAVE_NEON)
    elseif(ARM)
        find_package(Neon)
        if(CPU_HAS_NEON)
            set(ARM_ARGS -mcpu=native -mfloat-abi=hard -mfpu=neon -marm -fPIC)
            add_definitions(-DHAVE_NEON)
        else()
            set(ARM_ARGS -mcpu=native -mfloat-abi=hard -mfpu=vfp -marm)
        endif()
    endif()
    if(ARM64)
        set(CPU_HAS_NEON 1)
        add_definitions(-DX265_ARCH_ARM64=1)

        if (AARCH64_RUNTIME_CPU_DETECT)
            add_definitions(-DAARCH64_RUNTIME_CPU_DETECT=1)
            message(STATUS "Configuring build for run-time CPU feature detection")
        endif()

        if(AARCH64_RUNTIME_CPU_DETECT OR CROSS_COMPILE_ARM64)
            # Add all extensions when compiling for run-time CPU feature detection or cross compiling.
            set(CPU_HAS_NEON_DOTPROD 1)
            set(CPU_HAS_NEON_I8MM 1)
            set(CPU_HAS_SVE 1)
            set(CPU_HAS_SVE2 1)
            set(CPU_HAS_SVE2_BITPERM 1)
        else()
            if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
                find_package(NEON_DOTPROD)
                find_package(NEON_I8MM)
                find_package(SVE)
                find_package(SVE2)
                find_package(SVE2_BITPERM)
            else()
                message(STATUS "Compile-time CPU feature detection unsupported on this platform")
            endif()
        endif()

        if(ENABLE_SVE OR ENABLE_SVE2)
            set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
            string(APPEND CMAKE_REQUIRED_FLAGS " ${AARCH64_SVE_FLAG}")
            set(OLD_CMAKE_TRY_COMPILE_TARGET_TYPE ${CMAKE_TRY_COMPILE_TARGET_TYPE})
            set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
            # When compilation target is a STATIC_LIBRARY, the LINK_OPTIONS are
            # passed to the archiver, so we must backup, clear and restore these.
            # https://gitlab.kitware.com/cmake/cmake/-/issues/23454
            set(OLD_CMAKE_REQUIRED_LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS})
            set(CMAKE_REQUIRED_LINK_OPTIONS "")

            # Check whether the compiler can compile SVE functions that require
            # backup/restore of SVE registers according to AAPCS.
            # https://github.com/llvm/llvm-project/issues/80009.
            set(SVE_COMPILATION_TEST "
#include <arm_sve.h>
void other();
svfloat32_t func(svfloat32_t a) {
  other();
  return a;
}
int main() { return 0; }")

            check_c_source_compiles("${SVE_COMPILATION_TEST}" SVE_COMPILATION_C_TEST_COMPILED)
            check_cxx_source_compiles("${SVE_COMPILATION_TEST}" SVE_COMPILATION_CXX_TEST_COMPILED)

            # Check if arm_neon_sve_bridge.h is available.
            set(SVE_HEADER_TEST "
#ifndef __ARM_NEON_SVE_BRIDGE
#error 1
#endif
#include <arm_sve.h>
#include <arm_neon_sve_bridge.h>
int main() { return 0; }")
            check_c_source_compiles("${SVE_HEADER_TEST}" SVE_HEADER_C_TEST_COMPILED)
            check_cxx_source_compiles("${SVE_HEADER_TEST}" SVE_HEADER_CXX_TEST_COMPILED)

            set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
            set(CMAKE_TRY_COMPILE_TARGET_TYPE ${OLD_CMAKE_TRY_COMPILE_TARGET_TYPE})
            set(CMAKE_REQUIRED_LINK_OPTIONS ${OLD_CMAKE_REQUIRED_LINK_OPTIONS})
            if (SVE_COMPILATION_C_TEST_COMPILED AND SVE_COMPILATION_CXX_TEST_COMPILED)
                if (SVE_HEADER_C_TEST_COMPILED AND SVE_HEADER_CXX_TEST_COMPILED)
                    add_definitions(-DHAVE_SVE_BRIDGE=1)
                    set(HAVE_SVE_BRIDGE 1)
                endif()
            else()
                set(ENABLE_SVE OFF CACHE BOOL "" FORCE)
                set(ENABLE_SVE2 OFF CACHE BOOL "" FORCE)
                message(STATUS "Disabling SVE and SVE2")
            endif()
        endif()

        # Impose constraint that disabling one extension disables all 'higher order' ones.
        if(NOT ENABLE_NEON)
            message(STATUS "Disabling Neon")
            set(CPU_HAS_NEON 0)
            set(ENABLE_NEON_DOTPROD 0)
        endif()
        if(NOT ENABLE_NEON_DOTPROD)
            message(STATUS "Disabling Neon DotProd")
            set(CPU_HAS_NEON_DOTPROD 0)
            set(ENABLE_NEON_I8MM 0)
        endif()
        if(NOT ENABLE_NEON_I8MM)
            message(STATUS "Disabling Neon I8MM")
            set(CPU_HAS_NEON_I8MM 0)
            set(ENABLE_SVE 0)
        endif()
        if(NOT ENABLE_SVE)
            message(STATUS "Disabling SVE")
            set(CPU_HAS_SVE 0)
            set(ENABLE_SVE2 0)
        endif()
        if(NOT ENABLE_SVE2)
            message(STATUS "Disabling SVE2")
            set(CPU_HAS_SVE2 0)
            set(ENABLE_SVE2_BITPERM 0)
        endif()
        if(NOT ENABLE_SVE2_BITPERM)
            message(STATUS "Disabling SVE2 BitPerm")
            set(CPU_HAS_SVE2_BITPERM 0)
        endif()

        if(CPU_HAS_NEON)
            message(STATUS "Found Neon")
            add_definitions(-DHAVE_NEON=1)
        endif()
        if(CPU_HAS_NEON_DOTPROD)
            message(STATUS "Found Neon DotProd")
            add_definitions(-DHAVE_NEON_DOTPROD=1)
        endif()
        if(CPU_HAS_NEON_I8MM)
            message(STATUS "Found Neon I8MM")
            add_definitions(-DHAVE_NEON_I8MM=1)
        endif()
        if(CPU_HAS_SVE)
            message(STATUS "Found SVE")
            add_definitions(-DHAVE_SVE=1)
        endif()
        if(CPU_HAS_SVE2)
            message(STATUS "Found SVE2")
            add_definitions(-DHAVE_SVE2=1)
        endif()
        if(CPU_HAS_SVE2_BITPERM)
            message(STATUS "Found SVE2 BitPerm")
            add_definitions(-DHAVE_SVE2_BITPERM=1)
        endif()
        set(ARM_ARGS -O3)
        # Do not allow implicit vector type conversions in Clang builds (this
        # is already the default in GCC builds).
        check_cxx_compiler_flag(-flax-vector-conversions=none CC_HAS_FLAX_VEC_CONV_NONE)
        if(CC_HAS_FLAX_VEC_CONV_NONE)
            set(ARM_ARGS ${ARM_ARGS} -flax-vector-conversions=none)
        endif()
    endif()
    if(RISCV64)
        add_definitions(-DX265_ARCH_RISCV64=1)

        if (RISCV64_RUNTIME_CPU_DETECT)
            add_definitions(-DRISCV64_RUNTIME_CPU_DETECT=1)
            message(STATUS "Configuring build for run-time CPU feature detection")
        endif()

        if(RISCV64_RUNTIME_CPU_DETECT OR CROSS_COMPILE_RISCV64)
            # Add all extensions when compiling for run-time CPU feature detection or cross compiling.
            set(CPU_HAS_RVV 1)
        else()
            if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
                find_package(RVV)
            else()
                message(STATUS "Compile-time CPU feature detection unsupported on this platform")
            endif()
        endif()

        if(ENABLE_RVV)
            set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
            set(OLD_CMAKE_TRY_COMPILE_TARGET_TYPE ${CMAKE_TRY_COMPILE_TARGET_TYPE})
            set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
            # When compilation target is a STATIC_LIBRARY, the LINK_OPTIONS are
            # passed to the archiver, so we must backup, clear and restore these.
            # https://gitlab.kitware.com/cmake/cmake/-/issues/23454
            set(OLD_CMAKE_REQUIRED_LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS})
            set(CMAKE_REQUIRED_LINK_OPTIONS "")

            # Check whether the compiler can compile RVV functions that require
            # backup/restore of RVV registers according to AAPCS.
            # https://github.com/llvm/llvm-project/issues/80009.
            set(RVV_COMPILATION_TEST [[
int main() {
  asm volatile(".option arch, +v; vsetvli t0, a0, e32, m1, ta, ma");
  return 0;
}]])

            check_c_source_compiles("${RVV_COMPILATION_TEST}" RVV_COMPILATION_C_TEST_COMPILED)
            check_cxx_source_compiles("${RVV_COMPILATION_TEST}" RVV_COMPILATION_CXX_TEST_COMPILED)

            set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
            set(CMAKE_TRY_COMPILE_TARGET_TYPE ${OLD_CMAKE_TRY_COMPILE_TARGET_TYPE})
            set(CMAKE_REQUIRED_LINK_OPTIONS ${OLD_CMAKE_REQUIRED_LINK_OPTIONS})
            if (NOT(RVV_COMPILATION_C_TEST_COMPILED AND RVV_COMPILATION_CXX_TEST_COMPILED))
                set(ENABLE_RVV OFF CACHE BOOL "" FORCE)
                message(STATUS "Disabling RVV")
            endif()
        endif()

        if(NOT ENABLE_RVV)
            message(STATUS "Disabling RVV")
            set(CPU_HAS_RVV 0)
        endif()

        set(CPU_HAS_RVV_INTRINSIC 0)
        if(CPU_HAS_RVV)
            message(STATUS "Found RVV")
            add_definitions(-DHAVE_RVV=1)

            set(RVV_INTRINSIC_TEST [[
#include <riscv_vector.h>
#include <stdint.h>
int main() {
    size_t vl = __riscv_vsetvl_e32m1(10);
    return 0;
}]])
            set(CMAKE_REQUIRED_FLAGS ${RISCV64_INTRINSIC_FLAG})
            check_c_source_compiles("${RVV_INTRINSIC_TEST}" RVV_INTRINSIC_C_TEST_COMPILED)
            check_cxx_source_compiles("${RVV_INTRINSIC_TEST}" RVV_INTRINSIC_CXX_TEST_COMPILED)
            if (RVV_INTRINSIC_C_TEST_COMPILED AND RVV_INTRINSIC_CXX_TEST_COMPILED)
                set(CPU_HAS_RVV_INTRINSIC 1)
                add_definitions(-DHAVE_RVV_INTRINSIC=1)
                message(STATUS "Found RVV intrinsic")
            else()
                message(STATUS "Disabling RVV intrinsic")
            endif()
        endif()

        set(RISCV64_ARGS -O2)
    endif()
    if(ENABLE_PIC)
        list(APPEND ARM_ARGS -DPIC -fPIC)
    endif()
    add_definitions(${ARM_ARGS})
    if(FPROFILE_GENERATE)
        if(INTEL_CXX)
            add_definitions(-prof-gen -prof-dir="${CMAKE_CURRENT_BINARY_DIR}")
            list(APPEND LINKER_OPTIONS "-prof-gen")
        else()
            check_cxx_compiler_flag(-fprofile-generate CC_HAS_PROFILE_GENERATE)
            if(CC_HAS_PROFILE_GENERATE)
                add_definitions(-fprofile-generate)
                list(APPEND LINKER_OPTIONS "-fprofile-generate")
            endif(CC_HAS_PROFILE_GENERATE)
        endif(INTEL_CXX)
    endif(FPROFILE_GENERATE)
    if(FPROFILE_USE)
        if(INTEL_CXX)
            add_definitions(-prof-use -prof-dir="${CMAKE_CURRENT_BINARY_DIR}")
            list(APPEND LINKER_OPTIONS "-prof-use")
        else()
            check_cxx_compiler_flag(-fprofile-use CC_HAS_PROFILE_USE)
            check_cxx_compiler_flag(-fprofile-correction CC_HAS_PROFILE_CORRECTION)
            check_cxx_compiler_flag(-Wno-error=coverage-mismatch CC_HAS_COVMISMATCH)
            if(CC_HAS_PROFILE_USE)
                add_definitions(-fprofile-use)
                list(APPEND LINKER_OPTIONS "-fprofile-use")
            endif(CC_HAS_PROFILE_USE)
            if(CC_HAS_PROFILE_CORRECTION)
                # auto-correct corrupted counters (happens a lot with x265)
                add_definitions(-fprofile-correction)
            endif(CC_HAS_PROFILE_CORRECTION)
            if(CC_HAS_COVMISMATCH)
                # ignore coverage mismatches (also happens a lot)
                add_definitions(-Wno-error=coverage-mismatch)
            endif(CC_HAS_COVMISMATCH)
        endif(INTEL_CXX)
    endif(FPROFILE_USE)
    if(STATIC_LINK_CRT)
        add_definitions(-static)
        list(APPEND LINKER_OPTIONS "-static-libgcc -static-libstdc++")
    endif(STATIC_LINK_CRT)
    check_cxx_compiler_flag(-Wno-strict-overflow CC_HAS_NO_STRICT_OVERFLOW)
    check_cxx_compiler_flag(-Wno-narrowing CC_HAS_NO_NARROWING) 
    check_cxx_compiler_flag(-Wno-array-bounds CC_HAS_NO_ARRAY_BOUNDS) 
    if (CC_HAS_NO_ARRAY_BOUNDS)
        add_definitions(-Wno-array-bounds) # these are unhelpful
    endif()
    check_cxx_compiler_flag(-ffast-math CC_HAS_FAST_MATH) 
    if (CC_HAS_FAST_MATH)
        add_definitions(-ffast-math)
    endif()
    if (NOT (ARM64 OR CROSS_COMPILE_ARM64 OR RISCV64 OR CROSS_COMPILE_RISCV64))
        check_cxx_compiler_flag(-mstackrealign CC_HAS_STACK_REALIGN)
        if (CC_HAS_STACK_REALIGN)
            add_definitions(-mstackrealign)
        endif()
    endif()
    # Disable exceptions. Reduce executable size, increase compability.
    check_cxx_compiler_flag(-fno-exceptions CC_HAS_FNO_EXCEPTIONS_FLAG)
    if(CC_HAS_FNO_EXCEPTIONS_FLAG)
        add_definitions(-fno-exceptions)
    endif()
    set(FSANITIZE "" CACHE STRING "-fsanitize options for GCC/clang")
    if(FSANITIZE)
        add_definitions(-fsanitize=${FSANITIZE})
        # clang and gcc need the sanitize options to be passed at link
        # time so the appropriate ASAN/TSAN runtime libraries can be
        # linked.
        list(APPEND LINKER_OPTIONS "-fsanitize=${FSANITIZE}")
    endif()
    if(ENABLE_CET)
        check_cxx_compiler_flag(-fcf-protection=full CC_HAS_CET_FLAG)
        if(CC_HAS_CET_FLAG)
            add_definitions(-fcf-protection=full)
        endif()
    endif()
    option(ENABLE_AGGRESSIVE_CHECKS "Enable stack protection and -ftrapv" OFF)
    if(ENABLE_AGGRESSIVE_CHECKS)
        # use with care, -ftrapv can cause testbench SIGILL exceptions
        # since it is testing corner cases of signed integer math
        add_definitions(-DUSING_FTRAPV=1)
        check_cxx_compiler_flag(-fsanitize=undefined-trap CC_HAS_CATCH_UNDEFINED) # clang
        check_cxx_compiler_flag(-ftrapv CC_HAS_FTRAPV)                            # gcc
        check_cxx_compiler_flag(-fstack-protector-all CC_HAS_STACK_PROTECT)       # gcc
        if(CC_HAS_FTRAPV)
            add_definitions(-ftrapv)
        endif()
        if(CC_HAS_CATCH_UNDEFINED)
            add_definitions(-fsanitize=undefined-trap -fsanitize-undefined-trap-on-error)
        endif()
        if(CC_HAS_STACK_PROTECT)
            add_definitions(-fstack-protector-all)
            if(MINGW)
                list(APPEND PLATFORM_LIBS ssp)
            endif()
        endif()
    endif(ENABLE_AGGRESSIVE_CHECKS)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE CC_VERSION)
endif(GCC)

find_package(Nasm)
if(ARM OR CROSS_COMPILE_ARM OR ARM64 OR CROSS_COMPILE_ARM64)
    option(ENABLE_ASSEMBLY "Enable use of assembly coded primitives" ON)
    # Force ENABLE_ASSEMBLY to be OFF when all SIMD extensions are disabled.
    if (NOT ENABLE_NEON)
        set(ENABLE_ASSEMBLY OFF CACHE BOOL "" FORCE)
    endif()
elseif((RISCV64 OR CROSS_COMPILE_RISCV64) AND ENABLE_RVV)
    option(ENABLE_ASSEMBLY "Enable use of assembly coded primitives" ON)
elseif(NASM_FOUND AND X86)
    if (NASM_VERSION_STRING VERSION_LESS "2.13.0")
        message(STATUS "Nasm version ${NASM_VERSION_STRING} is too old. 2.13.0 or later required")
        option(ENABLE_ASSEMBLY "Enable use of assembly coded primitives" OFF)
    else()
        message(STATUS "Found Nasm ${NASM_VERSION_STRING} to build assembly primitives")
        option(ENABLE_ASSEMBLY "Enable use of assembly coded primitives" ON)
    endif()
else()
    option(ENABLE_ASSEMBLY "Enable use of assembly coded primitives" OFF)
endif()

if(ENABLE_ASSEMBLY)
   add_definitions(-DENABLE_ASSEMBLY)
endif()

option(CHECKED_BUILD "Enable run-time sanity checks (debugging)" OFF)
if(CHECKED_BUILD)
    add_definitions(-DCHECKED_BUILD=1)
endif()

# Build options
set(LIB_INSTALL_DIR lib CACHE STRING "Install location of libraries")
set(BIN_INSTALL_DIR bin CACHE STRING "Install location of executables")
set(EXTRA_LIB "" CACHE STRING "Extra libraries to link against")
set(EXTRA_LINK_FLAGS "" CACHE STRING "Extra link flags")
if(EXTRA_LINK_FLAGS)
    list(APPEND LINKER_OPTIONS ${EXTRA_LINK_FLAGS})
endif()
if(EXTRA_LIB)
    option(LINKED_8BIT  "8bit libx265 is being linked with this library" OFF)
    option(LINKED_10BIT "10bit libx265 is being linked with this library" OFF)
    option(LINKED_12BIT "12bit libx265 is being linked with this library" OFF)
endif(EXTRA_LIB)
mark_as_advanced(EXTRA_LIB EXTRA_LINK_FLAGS)

if(X64 OR ARM64 OR PPC64 OR RISCV64)
    # NOTE: We only officially support high-bit-depth compiles of x265
    # on 64bit architectures. Main10 plus large resolution plus slow
    # preset plus 32bit address space usually means malloc failure.  You
    # can disable this if(X64) check if you desparately need a 32bit
    # build with 10bit/12bit support, but this violates the "shrink wrap
    # license" so to speak.  If it breaks you get to keep both halves.
    # You will need to disable assembly manually.
    option(HIGH_BIT_DEPTH "Store pixel samples as 16bit values (Main10/Main12)" OFF)
endif(X64 OR ARM64 OR PPC64 OR RISCV64)
if(HIGH_BIT_DEPTH)
    option(MAIN12 "Support Main12 instead of Main10" OFF)
    if(MAIN12)
        add_definitions(-DHIGH_BIT_DEPTH=1 -DX265_DEPTH=12)
    else()
        add_definitions(-DHIGH_BIT_DEPTH=1 -DX265_DEPTH=10)
    endif()
else(HIGH_BIT_DEPTH)
    add_definitions(-DHIGH_BIT_DEPTH=0 -DX265_DEPTH=8)
endif(HIGH_BIT_DEPTH)

if (ENABLE_HDR10_PLUS)
    include_directories(. dynamicHDR10 "${PROJECT_BINARY_DIR}")
    add_subdirectory(dynamicHDR10)
    add_definitions(-DENABLE_HDR10_PLUS)
endif(ENABLE_HDR10_PLUS)

option(ENABLE_SVT_HEVC "Enable SVT HEVC Encoder" OFF)
if(ENABLE_SVT_HEVC)
    find_package(svthevc)
    if(SVTHEVC_FOUND)
        add_definitions(-DSVT_HEVC)
        include_directories(${SVT_HEVC_INCLUDE_DIR})
    endif(SVTHEVC_FOUND)
endif(ENABLE_SVT_HEVC)

# this option can only be used when linking multiple libx265 libraries
# together, and some alternate API access method is implemented.
option(EXPORT_C_API "Implement public C programming interface" ON)
mark_as_advanced(EXPORT_C_API)
if(EXPORT_C_API)
    set(X265_NS x265)
    add_definitions(-DEXPORT_C_API=1)
elseif(HIGH_BIT_DEPTH)
    if(MAIN12)
        set(X265_NS x265_12bit)
    else()
        set(X265_NS x265_10bit)
    endif()
    add_definitions(-DEXPORT_C_API=0)
else()
    set(X265_NS x265_8bit)
    add_definitions(-DEXPORT_C_API=0)
endif()
add_definitions(-DX265_NS=${X265_NS})

if(ARM64 OR RISCV64)
  if(HIGH_BIT_DEPTH)
    if(MAIN12)
      list(APPEND ASM_FLAGS -DHIGH_BIT_DEPTH=1 -DBIT_DEPTH=12 -DX265_NS=${X265_NS})
    else()
      list(APPEND ASM_FLAGS -DHIGH_BIT_DEPTH=1 -DBIT_DEPTH=10 -DX265_NS=${X265_NS})
    endif()
  else()
    list(APPEND ASM_FLAGS -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8 -DX265_NS=${X265_NS})
  endif()
endif(ARM64 OR RISCV64)

option(WARNINGS_AS_ERRORS "Stop compiles on first warning" OFF)
if(WARNINGS_AS_ERRORS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_definitions(-Werror)
    elseif(MSVC)
        add_definitions(/WX)
    endif()
endif(WARNINGS_AS_ERRORS)

if(WIN32)
    # Visual leak detector
    find_package(VLD QUIET)
    if(VLD_FOUND)
        add_definitions(-DHAVE_VLD)
        include_directories(${VLD_INCLUDE_DIRS})
        list(APPEND PLATFORM_LIBS ${VLD_LIBRARIES})
        link_directories(${VLD_LIBRARY_DIRS})
    endif()
    option(WINXP_SUPPORT "Make binaries compatible with Windows XP and Vista" OFF)
    if(WINXP_SUPPORT)
        # force use of workarounds for CONDITION_VARIABLE and atomic
        # intrinsics introduced after XP
        add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WINXP -D_WIN32_WINNT_WIN7=0x0601)
    else(WINXP_SUPPORT)
        # default to targeting Windows 7 for the NUMA APIs
        add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN7)
    endif(WINXP_SUPPORT)
endif()

if(POWER)
    # IBM Power8
    option(ENABLE_ALTIVEC "Enable ALTIVEC profiling instrumentation" ON)
    if(ENABLE_ALTIVEC)
        add_definitions(-DHAVE_ALTIVEC=1 -maltivec -mabi=altivec)
        add_definitions(-flax-vector-conversions -fpermissive)
    else()
        add_definitions(-DHAVE_ALTIVEC=0)
    endif()

    option(CPU_POWER8 "Enable CPU POWER8 profiling instrumentation" ON)
    if(CPU_POWER8)
        add_definitions(-mcpu=power8 -DX265_ARCH_POWER8=1)
    endif()
endif()

include(Version) # determine X265_VERSION and X265_LATEST_TAG
include_directories(. common encoder "${PROJECT_BINARY_DIR}")

option(ENABLE_PPA "Enable PPA profiling instrumentation" OFF)
if(ENABLE_PPA)
    add_definitions(-DENABLE_PPA)
    list(APPEND PLATFORM_LIBS PPA)
    if(UNIX)
        list(APPEND PLATFORM_LIBS dl)
    endif(UNIX)
    add_subdirectory(profile/PPA)
endif(ENABLE_PPA)

option(ENABLE_VTUNE "Enable Vtune profiling instrumentation" OFF)
if(ENABLE_VTUNE)
    find_package(Vtune)
    if(VTUNE_FOUND)
        add_definitions(-DENABLE_VTUNE)
        include_directories(${VTUNE_INCLUDE_DIR})
        list(APPEND PLATFORM_LIBS vtune)
        link_directories(${VTUNE_LIBRARY_DIR})
        if(WIN32)
            list(APPEND PLATFORM_LIBS libittnotify.lib)
        else()
            list(APPEND PLATFORM_LIBS libittnotify.a dl)
        endif()
        add_subdirectory(profile/vtune)
    endif(VTUNE_FOUND)
endif(ENABLE_VTUNE)

option(DETAILED_CU_STATS "Enable internal profiling of encoder work" OFF)
if(DETAILED_CU_STATS)
    add_definitions(-DDETAILED_CU_STATS)
endif(DETAILED_CU_STATS)

option(ENABLE_ALPHA "Enable alpha encoding in x265" OFF)
if(ENABLE_ALPHA)
    add_definitions(-DENABLE_ALPHA)
    set(ENABLE_ALPHA 1)
else()
    set(ENABLE_ALPHA 0)
endif()

option(ENABLE_MULTIVIEW "Enable Multi-view encoding in HEVC" OFF)
if(ENABLE_MULTIVIEW)
    add_definitions(-DENABLE_MULTIVIEW)
    set(ENABLE_MULTIVIEW 1)
else()
    set(ENABLE_MULTIVIEW 0)
endif()

option(ENABLE_SCC_EXT "Enable screen content coding extension in HEVC" OFF)
if(ENABLE_SCC_EXT)
    add_definitions(-DENABLE_SCC_EXT)
    set(ENABLE_SCC_EXT 1)
else()
    set(ENABLE_SCC_EXT 0)
endif()
configure_file("${PROJECT_SOURCE_DIR}/x265_config.h.in"
               "${PROJECT_BINARY_DIR}/x265_config.h")

add_subdirectory(encoder)
add_subdirectory(common)

if((MSVC_IDE OR XCODE OR GCC) AND ENABLE_ASSEMBLY)
    # this is required because of this cmake bug
    # http://www.cmake.org/Bug/print_bug_page.php?bug_id=8170
    if(WIN32)
        set(SUFFIX obj)
    else()
        set(SUFFIX o)
    endif()

    if(ARM OR CROSS_COMPILE_ARM)
    # compile ARM arch asm files here
        enable_language(ASM)
        if(APPLE)
            set(ARM_ARGS ${ARM_ARGS} -arch ${CMAKE_OSX_ARCHITECTURES})
        endif()
        foreach(ASM ${ARM_ASMS})
			set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/arm/${ASM})
            list(APPEND ASM_SRCS ${ASM_SRC})
            list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
            add_custom_command(
                OUTPUT ${ASM}.${SUFFIX}
                COMMAND ${CMAKE_AS} ${AS_ARGUMENTS} -- ${CMAKE_C_COMPILER} ${ARM_ARGS} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                DEPENDS ${ASM_SRC})
        endforeach()
	elseif(ARM64 OR CROSS_COMPILE_ARM64)
    # compile ARM64 arch asm files here
        enable_language(ASM)
        foreach(ASM ${ARM_ASMS})
            set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/aarch64/${ASM})
            list(APPEND ASM_SRCS ${ASM_SRC})
            list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
            add_custom_command(
                OUTPUT ${ASM}.${SUFFIX}
                COMMAND ${CMAKE_CXX_COMPILER}
                ARGS ${ARM_ARGS} ${ASM_FLAGS} ${AARCH64_NEON_FLAG} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                DEPENDS ${ASM_SRC})
        endforeach()
        if(CPU_HAS_NEON_DOTPROD)
            foreach(ASM ${ARM_ASMS_NEON_DOTPROD})
                set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/aarch64/${ASM})
                list(APPEND ASM_SRCS ${ASM_SRC})
                list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
                add_custom_command(
                    OUTPUT ${ASM}.${SUFFIX}
                    COMMAND ${CMAKE_CXX_COMPILER}
                    ARGS ${ARM_ARGS} ${ASM_FLAGS} ${AARCH64_NEON_DOTPROD_FLAG} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                    DEPENDS ${ASM_SRC})
            endforeach()
        endif()
        if(CPU_HAS_SVE)
            foreach(ASM ${ARM_ASMS_SVE})
                set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/aarch64/${ASM})
                list(APPEND ASM_SRCS ${ASM_SRC})
                list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
                add_custom_command(
                    OUTPUT ${ASM}.${SUFFIX}
                    COMMAND ${CMAKE_CXX_COMPILER}
                    ARGS ${ARM_ARGS} ${ASM_FLAGS} ${AARCH64_SVE_FLAG} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                    DEPENDS ${ASM_SRC})
            endforeach()
        endif()
        if(CPU_HAS_SVE2)
            foreach(ASM ${ARM_ASMS_SVE2})
                set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/aarch64/${ASM})
                list(APPEND ASM_SRCS ${ASM_SRC})
                list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
                add_custom_command(
                    OUTPUT ${ASM}.${SUFFIX}
                    COMMAND ${CMAKE_CXX_COMPILER}
                    ARGS ${ARM_ARGS} ${ASM_FLAGS} ${AARCH64_SVE2_FLAG} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                    DEPENDS ${ASM_SRC})
            endforeach()
        endif()
        if(CPU_HAS_SVE2_BITPERM)
            foreach(ASM ${ARM_ASMS_SVE2_BITPERM})
                set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/aarch64/${ASM})
                list(APPEND ASM_SRCS ${ASM_SRC})
                list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
                add_custom_command(
                    OUTPUT ${ASM}.${SUFFIX}
                    COMMAND ${CMAKE_CXX_COMPILER}
                    ARGS ${ARM_ARGS} ${ASM_FLAGS} ${AARCH64_SVE2_BITPERM_FLAG} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                    DEPENDS ${ASM_SRC})
            endforeach()
        endif()
    elseif(RISCV64 OR CROSS_COMPILE_RISCV64)
    # compile RISCV64 arch asm files here
        enable_language(ASM)
        foreach(ASM ${RISCV64_ASMS})
            set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/riscv64/${ASM})
            list(APPEND ASM_SRCS ${ASM_SRC})
            list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
            add_custom_command(
                OUTPUT ${ASM}.${SUFFIX}
                COMMAND ${CMAKE_CXX_COMPILER}
                ARGS ${RISCV64_ARGS} ${ASM_FLAGS} -c ${ASM_SRC} -o ${ASM}.${SUFFIX}
                DEPENDS ${ASM_SRC})
        endforeach()
    elseif(X86)
    # compile X86 arch asm files here
        foreach(ASM ${MSVC_ASMS})
            set(ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/x86/${ASM})
            list(APPEND ASM_SRCS ${ASM_SRC})
            list(APPEND ASM_OBJS ${ASM}.${SUFFIX})
            add_custom_command(
                OUTPUT ${ASM}.${SUFFIX}
                COMMAND ${NASM_EXECUTABLE} ARGS ${NASM_FLAGS} ${ASM_SRC} -o ${ASM}.${SUFFIX}
                DEPENDS ${ASM_SRC})
        endforeach()
    endif()
endif()
source_group(ASM FILES ${ASM_SRCS})
if(ENABLE_HDR10_PLUS)
    add_library(x265-static STATIC $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:dynamicHDR10> ${ASM_OBJS})
    add_library(hdr10plus-static STATIC $<TARGET_OBJECTS:dynamicHDR10>)
    set_target_properties(hdr10plus-static PROPERTIES OUTPUT_NAME hdr10plus)
else()
    add_library(x265-static STATIC $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common> ${ASM_OBJS})
endif()
if(NOT MSVC)
    set_target_properties(x265-static PROPERTIES OUTPUT_NAME x265)
endif()
if(EXTRA_LIB)
    target_link_libraries(x265-static ${EXTRA_LIB})
endif()
if(ENABLE_LIBVMAF)
    target_link_libraries(x265-static ${VMAF})
endif()
if(SVTHEVC_FOUND)
    target_link_libraries(x265-static ${SVT_HEVC_LIBRARY})
endif()
install(TARGETS x265-static
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

if(ENABLE_HDR10_PLUS)
    install(TARGETS hdr10plus-static
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
endif()

if(SVTHEVC_FOUND)
    install(FILES "${SVT_HEVC_INCLUDE_DIR}/EbApi.h" DESTINATION include)
    install(FILES "${SVT_HEVC_INCLUDE_DIR}/EbErrorCodes.h" DESTINATION include)
    install(FILES "${SVT_HEVC_LIBRARY}" DESTINATION ${LIB_INSTALL_DIR})
endif()

install(FILES x265.h "${PROJECT_BINARY_DIR}/x265_config.h" DESTINATION include)
if((WIN32 AND ENABLE_CLI) OR (WIN32 AND ENABLE_SHARED))
    if(MSVC_IDE)
        if(ENABLE_CLI)
            install(FILES "${PROJECT_BINARY_DIR}/Debug/x265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS Debug)
            install(FILES "${PROJECT_BINARY_DIR}/RelWithDebInfo/x265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS RelWithDebInfo)
        endif()
        if(ENABLE_SHARED)
            install(FILES "${PROJECT_BINARY_DIR}/Debug/libx265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS Debug OPTIONAL NAMELINK_ONLY)
            install(FILES "${PROJECT_BINARY_DIR}/RelWithDebInfo/libx265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS RelWithDebInfo OPTIONAL NAMELINK_ONLY)
        endif()
    else()
        if(ENABLE_CLI)
            install(FILES "${PROJECT_BINARY_DIR}/x265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS Debug)
            install(FILES "${PROJECT_BINARY_DIR}/x265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS RelWithDebInfo)
        endif()
        if(ENABLE_SHARED)
            install(FILES "${PROJECT_BINARY_DIR}/libx265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS Debug OPTIONAL NAMELINK_ONLY)
            install(FILES "${PROJECT_BINARY_DIR}/libx265.pdb" DESTINATION ${BIN_INSTALL_DIR} CONFIGURATIONS RelWithDebInfo OPTIONAL NAMELINK_ONLY)
        endif()
    endif()
endif()
if(CMAKE_RC_COMPILER)
    # The resource compiler does not need CFLAGS or macro defines. It
    # often breaks them
    string(REPLACE "<FLAGS>" "" CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILE_OBJECT}")
    string(REPLACE "<DEFINES>" "" CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILE_OBJECT}")
    # convert X265_LATEST_TAG (ex: 0.7) and X265_TAG_DISTANCE (ex: 103) to
    # @X265_VERSION_MAJOR@,@X265_VERSION_MINOR@,@X265_BRANCH_ID@,@X265_TAG_DISTANCE@
    string(REGEX MATCHALL "([0-9]+)" VERSION_LIST "${X265_LATEST_TAG}")
    list(GET VERSION_LIST 0 X265_VERSION_MAJOR)
    list(GET VERSION_LIST 1 X265_VERSION_MINOR)
    set(X265_BRANCH_ID 0) # TODO: 0 - stable, 1 - default or other
    set(X265_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/x265.rc")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/x265.rc.in" "${X265_RC_FILE}" @ONLY)
endif()

if(NOT (MSVC_IDE OR XCODE))
    add_custom_target(clean-generated COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean-generated.cmake)
endif()
option(ENABLE_SHARED "Build shared library" ON)
if(ENABLE_SHARED)
    if(ENABLE_HDR10_PLUS)
        add_library(x265-shared SHARED "${PROJECT_BINARY_DIR}/x265.def" ${ASM_OBJS}
                    ${X265_RC_FILE} $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:dynamicHDR10>)
        add_library(hdr10plus-shared SHARED $<TARGET_OBJECTS:dynamicHDR10>)

        if(MSVC)
            set_target_properties(hdr10plus-shared PROPERTIES OUTPUT_NAME libhdr10plus)
        else()
            set_target_properties(hdr10plus-shared PROPERTIES OUTPUT_NAME hdr10plus)
        endif()
    else()
        add_library(x265-shared SHARED "${PROJECT_BINARY_DIR}/x265.def" ${ASM_OBJS}
                   ${X265_RC_FILE} $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common>)
    endif()
    if(EXTRA_LIB)
        target_link_libraries(x265-shared ${EXTRA_LIB})
    endif()
	 target_link_libraries(x265-shared ${PLATFORM_LIBS})
    if(SVTHEVC_FOUND)
        target_link_libraries(x265-shared ${SVT_HEVC_LIBRARY})
    endif(SVTHEVC_FOUND)
    if(MSVC)
        set_target_properties(x265-shared PROPERTIES OUTPUT_NAME libx265)
    else()
        set_target_properties(x265-shared PROPERTIES OUTPUT_NAME x265)
    endif()
    if(UNIX)
        set_target_properties(x265-shared PROPERTIES VERSION ${X265_BUILD})
        if(APPLE)
            set_target_properties(x265-shared PROPERTIES MACOSX_RPATH 1)
        elseif(CYGWIN)
            # Cygwin is not officially supported or tested. MinGW with msys is recommended.
        else()
            list(APPEND LINKER_OPTIONS "-Wl,-Bsymbolic,-znoexecstack")
        endif()
    endif()
    set_target_properties(x265-shared PROPERTIES SOVERSION ${X265_BUILD})
    if(X265_LATEST_TAG OR NOT GIT_FOUND)
        if(WINDOWS)
            set_target_properties(x265-shared PROPERTIES VERSION ${X265_LATEST_TAG})
        endif()
        # shared library is not installed if a tag is not found
        install(TARGETS x265-shared
                LIBRARY DESTINATION ${LIB_INSTALL_DIR}
                ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
                RUNTIME DESTINATION ${BIN_INSTALL_DIR})
    endif()
    if(ENABLE_HDR10_PLUS)
        install(TARGETS hdr10plus-shared
            LIBRARY DESTINATION ${LIB_INSTALL_DIR}
            ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
    endif()
    if(LINKER_OPTIONS)
        # set_target_properties can't do list expansion
        string(REPLACE ";" " " LINKER_OPTION_STR "${LINKER_OPTIONS}")
        set_target_properties(x265-shared PROPERTIES LINK_FLAGS "${LINKER_OPTION_STR}")
    endif()
endif()

if(X265_LATEST_TAG OR NOT GIT_FOUND)
    # convert lists of link libraries into -lstdc++ -lm etc..
    foreach(LIB ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES} ${PLATFORM_LIBS})
        if(IS_ABSOLUTE ${LIB} AND EXISTS ${LIB})
            list(APPEND PLIBLIST "${LIB}")
        else()
            list(APPEND PLIBLIST "-l${LIB}")
        endif()
    endforeach()
    if(PLIBLIST)
        # blacklist of libraries that should not be in Libs.private
        list(REMOVE_ITEM PLIBLIST "-lc" "-lpthread" "-lmingwex" "-lmingwthrd"
            "-lmingw32" "-lmoldname" "-lmsvcrt" "-ladvapi32" "-lshell32"
            "-luser32" "-lkernel32")
        string(REPLACE ";" " " PRIVATE_LIBS "${PLIBLIST}")
    else()
        set(PRIVATE_LIBS "")
    endif(PLIBLIST)

    # Produce a pkg-config file
    configure_file("x265.pc.in" "x265.pc" @ONLY)
    install(FILES       "${CMAKE_CURRENT_BINARY_DIR}/x265.pc"
            DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
add_custom_target(uninstall
                  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake")

# Main CLI application
set(ENABLE_CLI ON CACHE BOOL "Build standalone CLI application")
if(ENABLE_CLI)
    file(GLOB InputFiles input/input.cpp input/yuv.cpp input/y4m.cpp input/*.h)
    file(GLOB OutputFiles output/output.cpp output/reconplay.cpp output/*.h
                          output/yuv.cpp output/y4m.cpp # recon
                          output/raw.cpp)               # muxers
    source_group(input FILES ${InputFiles})
    source_group(output FILES ${OutputFiles})

    check_include_files(getopt.h HAVE_GETOPT_H)
    if(NOT HAVE_GETOPT_H)
        if(MSVC)
            set_source_files_properties(compat/getopt/getopt.c PROPERTIES COMPILE_FLAGS "/wd4100 /wd4131 -DHAVE_STRING_H=1")
        endif(MSVC)
        include_directories(compat/getopt)
        set(GETOPT compat/getopt/getopt.c compat/getopt/getopt.h)
    endif(NOT HAVE_GETOPT_H)
    if(WIN32)
        set(ExportDefs "${PROJECT_BINARY_DIR}/x265.def")
    endif(WIN32)
    if(XCODE)
        # Xcode seems unable to link the CLI with libs, so link as one targget
        if(ENABLE_HDR10_PLUS)
        add_executable(cli ../COPYING ${InputFiles} ${OutputFiles} ${GETOPT}
                        x265.cpp x265.h x265cli.cpp x265cli.h abrEncApp.cpp abrEncApp.h
                        $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:dynamicHDR10> ${ASM_OBJS})
        else()
            add_executable(cli ../COPYING ${InputFiles} ${OutputFiles} ${GETOPT}
                        x265.cpp x265.h x265cli.cpp x265cli.h abrEncApp.cpp abrEncApp.h
                        $<TARGET_OBJECTS:encoder> $<TARGET_OBJECTS:common> ${ASM_OBJS})
        endif()
    else()
        add_executable(cli ../COPYING ${InputFiles} ${OutputFiles} ${GETOPT} ${X265_RC_FILE}
                       ${ExportDefs} x265.cpp x265.h x265cli.cpp x265cli.h abrEncApp.cpp abrEncApp.h)
        if(WIN32 OR NOT ENABLE_SHARED OR INTEL_CXX)
            # The CLI cannot link to the shared library on Windows, it
            # requires internal APIs not exported from the DLL
            target_link_libraries(cli x265-static ${PLATFORM_LIBS})
        else()
            target_link_libraries(cli x265-shared ${PLATFORM_LIBS})
        endif()
    endif()
    set_target_properties(cli PROPERTIES OUTPUT_NAME x265)
    if(LINKER_OPTIONS)
        # set_target_properties can't do list expansion
        string(REPLACE ";" " " LINKER_OPTION_STR "${LINKER_OPTIONS}")
        set_target_properties(cli PROPERTIES LINK_FLAGS "${LINKER_OPTION_STR}")
    endif()

    install(TARGETS cli DESTINATION ${BIN_INSTALL_DIR})
endif(ENABLE_CLI)

if(ENABLE_ASSEMBLY AND NOT XCODE)
    option(ENABLE_TESTS "Enable Unit Tests" OFF)
    if(ENABLE_TESTS)
        add_subdirectory(test)
    endif()
endif()

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    if(PLATFORM_LIBS)
        LIST(REMOVE_DUPLICATES PLATFORM_LIBS)
        set(PLATFORM_LIBS ${PLATFORM_LIBS} PARENT_SCOPE)
    endif(PLATFORM_LIBS)
endif(hasParent)
