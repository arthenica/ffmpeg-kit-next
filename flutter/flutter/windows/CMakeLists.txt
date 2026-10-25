set(PROJECT_NAME "ffmpeg_kit_next_flutter")
cmake_minimum_required(VERSION 3.14)
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "ffmpeg_kit_next_flutter_plugin")
set(FFMPEG_KIT_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/Frameworks/ffmpeg-kit-next")

# Flutter builds must use the copied bundle, not DLLs available through MSYS2.
if(NOT EXISTS "${FFMPEG_KIT_BUNDLE}/lib/cmake/ffmpeg-kit-next/ffmpeg-kit-next-config.cmake")
  message(FATAL_ERROR "Build windows.sh, then run copy_local_binaries.sh windows before building the Flutter application.")
endif()

find_package(ffmpeg-kit-next REQUIRED CONFIG
  PATHS "${FFMPEG_KIT_BUNDLE}/lib/cmake/ffmpeg-kit-next"
  NO_DEFAULT_PATH)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FFmpegKit Flutter Windows requires a 64-bit target.")
endif()

if(NOT FFMPEG_KIT_NEXT_ARCHITECTURE STREQUAL "x64" AND
   NOT FFMPEG_KIT_NEXT_ARCHITECTURE STREQUAL "arm64")
  message(FATAL_ERROR "This Flutter Windows implementation requires an x64 or arm64 native bundle.")
endif()

if(CMAKE_GENERATOR_PLATFORM)
  # Visual Studio uses ARM64, while the native bundle reports arm64.
  string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" FFMPEG_KIT_TARGET_ARCHITECTURE)
  if(NOT FFMPEG_KIT_TARGET_ARCHITECTURE STREQUAL FFMPEG_KIT_NEXT_ARCHITECTURE)
    message(FATAL_ERROR "The ${FFMPEG_KIT_NEXT_ARCHITECTURE} native bundle does not match the ${CMAKE_GENERATOR_PLATFORM} Flutter target.")
  endif()
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

# The licence texts of the DLLs ship with them, in a folder of their own beside the runner.
# Flutter's app CMake sets its install prefix to $<TARGET_FILE_DIR:${BINARY_NAME}> only after this
# file has run, and a generator expression in the prefix is not evaluated for a relative
# destination, so name the runner's folder the same way here.
if(NOT IS_DIRECTORY "${FFMPEG_KIT_BUNDLE}/licenses")
  message(FATAL_ERROR "Missing licence texts in ${FFMPEG_KIT_BUNDLE}/licenses. Run copy_local_binaries.sh windows again with a complete bundle.")
endif()
install(DIRECTORY "${FFMPEG_KIT_BUNDLE}/licenses/"
  DESTINATION "$<TARGET_FILE_DIR:${BINARY_NAME}>/ffmpeg-kit-next-licenses"
  COMPONENT Runtime)
