cmake_minimum_required(VERSION 3.10)

# Developer helper to re-run WiX heat over the harvested resource folders when
# their content changes. Generated into the build folder (never the source
# tree); the .wxs files it regenerates live in this source folder and are
# checked in, so the script uses absolute paths and can be run from anywhere.
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" RE_GENERATE_SOURCE_ROOT)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" RE_GENERATE_WXS_FOLDER)
configure_file(re-generate.bat.in ${CMAKE_CURRENT_BINARY_DIR}/re-generate.bat)
