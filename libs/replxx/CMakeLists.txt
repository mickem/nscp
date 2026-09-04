cmake_minimum_required(VERSION 3.10)

# Vendored replxx (BSD-3-Clause) — see README.nscp.md. This is our own build
# file, not upstream's: upstream's carries install/export rules, a
# shared-library option and a coverage harness that we have no use for.
set(TARGET replxx)

set(SRCS
    src/ConvertUTF.cpp
    src/conversion.cxx
    src/escape.cxx
    src/history.cxx
    src/prompt.cxx
    src/replxx.cxx
    src/replxx_impl.cxx
    src/terminal.cxx
    src/util.cxx
    src/wcwidth.cpp
    src/windows.cxx
)
if(WIN32)
    set(SRCS
        ${SRCS}
        include/replxx.h
        include/replxx.hxx
        src/ConvertUTF.h
        src/conversion.hxx
        src/escape.hxx
        src/history.hxx
        src/killring.hxx
        src/prompt.hxx
        src/replxx_impl.hxx
        src/terminal.hxx
        src/unicodestring.hxx
        src/utf8string.hxx
        src/util.hxx
        src/windows.hxx
    )
endif(WIN32)

add_library(${TARGET} STATIC ${SRCS})
NSCP_DEBUG_SYMBOLS(${TARGET})

# replxx needs C++11; the project as a whole leaves the standard at the
# compiler default, so state the floor here rather than assume it.
set_target_properties(
    ${TARGET}
    PROPERTIES
        CXX_STANDARD
            11
        CXX_STANDARD_REQUIRED
            ON
        FOLDER
            "libraries"
)

# REPLXX_STATIC is PUBLIC on purpose: it drives the REPLXX_IMPEXP macro in the
# public header, so consumers must see the same value we built with or MSVC
# will decorate the symbols as dllimport and fail to link.
target_compile_definitions(${TARGET} PUBLIC REPLXX_STATIC)
target_include_directories(
    ${TARGET}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(MSVC)
    # Upstream builds clean under its own warning set but not under ours;
    # it is third-party code we do not modify, so quiet it rather than patch.
    target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS=1)
    # 4244/4267 narrowing, 4456/4459 shadowing, 4245 signed/unsigned, 4996
    # deprecated CRT. All are upstream's, all are benign, and none of them are
    # ours to fix.
    target_compile_options(
        ${TARGET}
        PRIVATE
            /wd4244
            /wd4245
            /wd4267
            /wd4456
            /wd4459
            /wd4996
    )
    # escape.cxx is header-only in practice and produces an object file with no
    # public symbols, which the librarian reports as LNK4221.
    set_target_properties(
        ${TARGET}
        PROPERTIES
            STATIC_LIBRARY_OPTIONS
                "/ignore:4221"
    )
endif(MSVC)

target_link_libraries(${TARGET} ${CMAKE_THREAD_LIBS_INIT})

# Linked into CommandClient, which is a MODULE (shared object) on Linux.
if(CMAKE_COMPILER_IS_GNUCXX)
    nscp_apply_pic(${TARGET})
endif(CMAKE_COMPILER_IS_GNUCXX)
