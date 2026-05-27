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
    # Propagate the backend selection to consumers (e.g. include/bytes/unzip.cpp)
    # so linking this target is enough to pick the miniz code path.
    target_compile_definitions(${TARGET} PUBLIC MINIZ)

    set_target_properties(
        ${TARGET}
        PROPERTIES
            FOLDER
                "core"
    )
elseif(LIBZIP_FOUND)
    add_library(${TARGET} INTERFACE)
    if(TARGET libzip::zip)
        target_link_libraries(${TARGET} INTERFACE libzip::zip)
    else()
        target_include_directories(${TARGET} INTERFACE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(${TARGET} INTERFACE ${LIBZIP_LIBRARIES})
    endif()
else()
    # No ZIP backend available. Rather than failing the build we present an
    # empty target that propagates NSCP_NO_ZIP to consumers; the unzip shim in
    # include/bytes/unzip.cpp then compiles a stub reader that reports every
    # archive as unreadable, and zip_plugin therefore refuses to load .zip
    # plugins. This keeps the daemon buildable on hosts without miniz or libzip.
    add_library(${TARGET} INTERFACE)
    target_compile_definitions(${TARGET} INTERFACE NSCP_NO_ZIP)
endif()
