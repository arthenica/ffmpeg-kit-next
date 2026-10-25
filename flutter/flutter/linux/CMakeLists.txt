# The name of the plugin's shared library and the top-level target.
set(PROJECT_NAME "ffmpeg_kit_next_flutter")
cmake_minimum_required(VERSION 3.13)
project(${PROJECT_NAME} LANGUAGES CXX)

set(PLUGIN_NAME "ffmpeg_kit_next_flutter_plugin")

add_library(${PLUGIN_NAME} SHARED
  "ffmpeg_kit_flutter_plugin.cc"
)

apply_standard_settings(${PLUGIN_NAME})

set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_STANDARD 14)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter)
target_link_libraries(${PLUGIN_NAME} PRIVATE PkgConfig::GTK)

# Vendored ffmpeg-kit native library (copied by copy_local_binaries.sh).
set(FFMPEG_KIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Frameworks/ffmpeg-kit-next")
set(FFMPEG_KIT_LIB_DIR "${FFMPEG_KIT_DIR}/lib")

foreach(FFMPEG_KIT_HEADER FFmpegKit.h FFmpegKitConfig.h ffmpegkit_c.h libavutil/avutil.h)
  if(NOT EXISTS "${FFMPEG_KIT_DIR}/include/${FFMPEG_KIT_HEADER}")
    message(FATAL_ERROR "Missing native header ${FFMPEG_KIT_HEADER}. Build linux.sh/nix-linux.sh, then run copy_local_binaries.sh linux before building the Flutter application.")
  endif()
endforeach()

# Check the copied bundle before compiling; no release number is required.
foreach(FFMPEG_KIT_LIBRARY ffmpegkit avdevice avfilter swscale avformat avcodec swresample avutil)
  file(GLOB FFMPEG_KIT_COMPONENT_LIBS CONFIGURE_DEPENDS
    "${FFMPEG_KIT_LIB_DIR}/lib${FFMPEG_KIT_LIBRARY}.so"
    "${FFMPEG_KIT_LIB_DIR}/lib${FFMPEG_KIT_LIBRARY}.so.*")
  if(NOT FFMPEG_KIT_COMPONENT_LIBS)
    message(FATAL_ERROR "Missing native library lib${FFMPEG_KIT_LIBRARY}.so in ${FFMPEG_KIT_LIB_DIR}. Build linux.sh/nix-linux.sh, then run copy_local_binaries.sh linux.")
  endif()
endforeach()

target_include_directories(${PLUGIN_NAME} PRIVATE "${FFMPEG_KIT_DIR}/include")

# Link every vendored shared object (libffmpegkit.so plus the FFmpeg libraries it
# requires — libavcodec.so.62, ...). The ffmpeg-kit-next.pc lists the FFmpeg libs
# under Requires, so libffmpegkit.so does not self-contain them; the plugin must
# link them directly to resolve all symbols.
file(GLOB FFMPEG_KIT_SHARED_LIBS CONFIGURE_DEPENDS "${FFMPEG_KIT_LIB_DIR}/*.so*")
foreach(FFMPEG_KIT_SHARED_LIB IN LISTS FFMPEG_KIT_SHARED_LIBS)
  if(NOT EXISTS "${FFMPEG_KIT_SHARED_LIB}")
    message(FATAL_ERROR "Broken native library symlink: ${FFMPEG_KIT_SHARED_LIB}. Run copy_local_binaries.sh linux again with a complete bundle.")
  endif()
endforeach()
target_link_directories(${PLUGIN_NAME} PUBLIC "${FFMPEG_KIT_LIB_DIR}")
target_link_libraries(${PLUGIN_NAME} PRIVATE ${FFMPEG_KIT_SHARED_LIBS})

# Expose the lib directory and an -rpath-link as PUBLIC so the app runner that
# links this plugin can also resolve the versioned FFmpeg sonames at link time
# (otherwise ld warns "libavcodec.so.62 ... not found").
target_link_options(${PLUGIN_NAME} PUBLIC "LINKER:-rpath-link,${FFMPEG_KIT_LIB_DIR}")

# Resolve the .so files next to the executable's bundled libs at runtime.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "$ORIGIN")

# Tell Flutter's Linux tooling to copy every shared object into the app bundle so
# the plugin's $ORIGIN rpath resolves them at runtime.
set(ffmpeg_kit_next_flutter_bundled_libraries "${FFMPEG_KIT_SHARED_LIBS}" PARENT_SCOPE)

# The licence texts of the native libraries ship with them, in a folder of their own in the app
# bundle. The destination is relative to the install prefix, which Flutter's app CMake sets to the
# bundle folder after this file has run.
if(NOT IS_DIRECTORY "${FFMPEG_KIT_DIR}/licenses")
  message(FATAL_ERROR "Missing licence texts in ${FFMPEG_KIT_DIR}/licenses. Run copy_local_binaries.sh linux again with a complete bundle.")
endif()
install(DIRECTORY "${FFMPEG_KIT_DIR}/licenses/"
  DESTINATION "ffmpeg-kit-next-licenses"
  COMPONENT Runtime)
