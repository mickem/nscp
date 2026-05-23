cmake_minimum_required(VERSION 3.10)

set(TARGET nscp_miniz)

project(${TARGET})

# Two backends present the same nscp_miniz target to the rest of the build:
# - Windows: prefer vendored miniz source resolved by FindMiniz.cmake.
#   Pulled in as a STATIC library from MINIZ_INCLUDE_DIR/miniz.c.
# - Linux/macOS: thin INTERFACE wrapping the system libzip resolved by
#   FindLibZip.cmake. Prefers the imported `libzip::zip` target (CMake
#   config package) and falls back to the pkg-config `LIBZIP_*` vars.
#
# The zip_archive shim in service/plugins/zip_plugin.cpp picks the matching
# API via `#ifdef MINIZ`.
if(MINIZ_FOUND)
    set(miniz_SOURCE
        ${MINIZ_INCLUDE_DIR}/miniz.c
        ${MINIZ_INCLUDE_DIR}/miniz.h
    )

    add_library(${TARGET} STATIC ${miniz_SOURCE})
    target_include_directories(${TARGET} PUBLIC ${MINIZ_INCLUDE_DIR})

    set_target_properties(
        ${TARGET}
        PROPERTIES
            FOLDER
                "core"
    )
else()
    if(NOT LIBZIP_FOUND)
        message(
            FATAL_ERROR
            "nscp_miniz: libzip not found (install libzip-dev / libzip-devel)"
        )
    endif()
    add_library(${TARGET} INTERFACE)
    if(TARGET libzip::zip)
        target_link_libraries(${TARGET} INTERFACE libzip::zip)
    else()
        target_include_directories(${TARGET} INTERFACE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(${TARGET} INTERFACE ${LIBZIP_LIBRARIES})
    endif()
endif()
