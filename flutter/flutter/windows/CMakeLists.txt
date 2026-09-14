set(PROJECT_NAME "ffmpeg_kit_next_flutter")
cmake_minimum_required(VERSION 3.14)
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "ffmpeg_kit_next_flutter_plugin")
set(FFMPEG_KIT_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/Frameworks/ffmpeg-kit-next")

# Flutter builds must use the copied bundle, not DLLs available through MSYS2.
if(NOT EXISTS "${FFMPEG_KIT_BUNDLE}/lib/cmake/ffmpeg-kit-next/ffmpeg-kit-next-config.cmake")
  message(FATAL_ERROR "Build windows.sh, then run copy_local_binaries.sh windows before building the Flutter application.")
endif()

find_package(ffmpeg-kit-next 9.0.0 REQUIRED CONFIG
  PATHS "${FFMPEG_KIT_BUNDLE}/lib/cmake/ffmpeg-kit-next"
  NO_DEFAULT_PATH)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FFmpegKit Flutter Windows requires a 64-bit target.")
endif()

if(NOT FFMPEG_KIT_NEXT_ARCHITECTURE STREQUAL "x64")
  message(FATAL_ERROR "This Flutter Windows implementation requires an x64 native bundle.")
endif()

if(CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
  message(FATAL_ERROR "The native bundle and Flutter target must both be x64.")
endif()

add_library(${PLUGIN_NAME} SHARED
  "ffmpeg_kit_next_flutter_plugin.cpp"
)

apply_standard_settings(${PLUGIN_NAME})
target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_17)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL NOMINMAX)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

# The imported target keeps its MSVC import library paired with the bundle DLL.
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter_wrapper_plugin)
target_link_libraries(${PLUGIN_NAME} PRIVATE ffmpeg-kit-next::ffmpegkit)

file(GLOB FFMPEG_KIT_RUNTIME_DLLS CONFIGURE_DEPENDS
  "${FFMPEG_KIT_BUNDLE}/bin/*.dll")

# Flutter copies this parent-scope list beside the runner for runtime loading.
set(ffmpeg_kit_next_flutter_bundled_libraries
  ${FFMPEG_KIT_RUNTIME_DLLS}
  PARENT_SCOPE)
