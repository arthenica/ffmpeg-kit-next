# The name of the plugin's shared library and the top-level target.
set(PROJECT_NAME "ffmpeg_kit_next_flutter")
cmake_minimum_required(VERSION 3.10)
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

target_include_directories(${PLUGIN_NAME} PRIVATE "${FFMPEG_KIT_DIR}/include")

# Link every vendored shared object (libffmpegkit.so plus the FFmpeg libraries it
# requires — libavcodec.so.62, ...). The ffmpeg-kit-next.pc lists the FFmpeg libs
# under Requires, so libffmpegkit.so does not self-contain them; the plugin must
# link them directly to resolve all symbols.
file(GLOB FFMPEG_KIT_SHARED_LIBS "${FFMPEG_KIT_LIB_DIR}/*.so*")
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
