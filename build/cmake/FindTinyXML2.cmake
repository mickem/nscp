# Locate tinyxml2 - either as an installed library (Debian: libtinyxml2-dev,
# RPM: tinyxml2-devel) or as a source checkout pointed at by
# TINY_XML2_SOURCE_DIR (Windows / vendored builds).
#
# Sets:
#   TINYXML2_FOUND        - whether tinyxml2 was located
#   TINYXML2_INCLUDE_DIR  - include directory (source folder for the vendored
#                           build, install prefix otherwise)
#   TINYXML2_LIBRARIES    - libraries to link when using the system package
#                           (empty for the vendored build, which compiles
#                           tinyxml2.cpp directly into the consuming module)
#
# Consumers should check TARGET tinyxml2::tinyxml2 to decide between linking
# the imported target and compiling the bundled tinyxml2.cpp.

# 1) CMake config package (modern Debian/Ubuntu/Fedora all ship this).
find_package(tinyxml2 CONFIG QUIET)
if(TARGET tinyxml2::tinyxml2)
    set(TINYXML2_FOUND TRUE)
    get_target_property(
        _tinyxml2_inc
        tinyxml2::tinyxml2
        INTERFACE_INCLUDE_DIRECTORIES
    )
    if(_tinyxml2_inc)
        set(TINYXML2_INCLUDE_DIR ${_tinyxml2_inc})
    endif()
    set(TINYXML2_LIBRARIES tinyxml2::tinyxml2)
endif()

# 2) pkg-config fallback for older distros lacking the CMake config file.
if(NOT TINYXML2_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_TINYXML2 QUIET tinyxml2)
    endif()
    if(PC_TINYXML2_FOUND)
        set(TINYXML2_FOUND TRUE)
        set(TINYXML2_INCLUDE_DIR ${PC_TINYXML2_INCLUDE_DIRS})
        set(TINYXML2_LIBRARIES ${PC_TINYXML2_LIBRARIES})
        # Expose the same imported target the CONFIG package would, so consumers
        # that switch on `TARGET tinyxml2::tinyxml2` link the system library on
        # this path too (otherwise they fall through to the vendored-source
        # branch and try to compile a non-existent /usr/include/tinyxml2.cpp).
        if(NOT TARGET tinyxml2::tinyxml2)
            add_library(tinyxml2::tinyxml2 INTERFACE IMPORTED)
            set_target_properties(
                tinyxml2::tinyxml2
                PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${PC_TINYXML2_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${PC_TINYXML2_LIBRARIES}"
                    INTERFACE_LINK_DIRECTORIES "${PC_TINYXML2_LIBRARY_DIRS}"
            )
        endif()
    endif()
endif()

# 3) Vendored source fallback - kept for the Windows build where TINY_XML2_SOURCE_DIR
#    is populated by the .github/actions/tinyxml2 download step.
if(NOT TINYXML2_FOUND)
    find_path(
        TINYXML2_INCLUDE_DIR
        NAMES
            tinyxml2.cpp
        PATHS
            ${TINY_XML2_SOURCE_DIR}
            ${TINYXML2_INCLUDE_DIR}
            ${NSCP_INCLUDEDIR}
    )
    if(TINYXML2_INCLUDE_DIR)
        set(TINYXML2_FOUND TRUE)
        set(TINYXML2_LIBRARIES "")
    else()
        set(TINYXML2_FOUND FALSE)
    endif()
endif()

mark_as_advanced(TINYXML2_INCLUDE_DIR TINYXML2_LIBRARIES)
