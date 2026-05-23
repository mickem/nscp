# Locate the vendored miniz source folder (Windows build path).
#
# Sets:
#   MINIZ_FOUND        - whether miniz.c was located
#   MINIZ_INCLUDE_DIR  - directory containing miniz.c / miniz.h
#
# Used on Windows where libzip is not first-class. The Linux/macOS build
# instead uses libzip via FindLibZip.cmake — the two backends are picked
# at compile time by service/plugins/zip_plugin.cpp's `#ifdef _WIN32`.

find_path(MINIZ_INCLUDE_DIR NAMES miniz.c PATHS ${MINIZ_INCLUDE_DIR})

if(MINIZ_INCLUDE_DIR)
    set(MINIZ_FOUND TRUE)
else()
    set(MINIZ_FOUND FALSE)
endif()

mark_as_advanced(MINIZ_INCLUDE_DIR)
