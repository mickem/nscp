# * Find MariaDB Connector/C (libmariadb), the client library used by the
#   CheckMySQL module to talk to MySQL, MariaDB, Percona and other
#   MySQL-compatible servers. The following variables are optionally searched
#   for defaults:
#
# MARIADB_ROOT_DIR - root of a Connector/C installation (system prefix or the
#                    unpacked Windows "MariaDB Connector C 64-bit" folder)
#
# This code sets the following variables:
#
# MARIADB_FOUND       - the connector's headers and library have been found
# MARIADB_INCLUDE_DIR - the directory where mysql.h is found
# MARIADB_LIBRARY     - the client library to link (libmariadb)
# MARIADB_DLL         - Windows only: the runtime DLL matching MARIADB_LIBRARY,
#                       which has to be shipped next to nscp.exe (see the note
#                       in modules/CheckMySQL/CMakeLists.txt). Empty when only a
#                       static library was found.
find_path(
    MARIADB_INCLUDE_DIR
    NAMES
        mysql.h
    PATH_SUFFIXES
        mariadb
        mysql
    PATHS
        ${MARIADB_ROOT_DIR}/include
        ${MARIADB_ROOT_DIR}/usr/include
        "C:/Program Files/MariaDB/MariaDB Connector C 64-bit/include"
        "C:/Program Files (x86)/MariaDB/MariaDB Connector C/include"
)

find_library(
    MARIADB_LIBRARY
    NAMES
        mariadb
        libmariadb
        mariadbclient
    PATH_SUFFIXES
        mariadb
    PATHS
        ${MARIADB_ROOT_DIR}/lib
        ${MARIADB_ROOT_DIR}/lib/x86_64-linux-gnu
        ${MARIADB_ROOT_DIR}/usr/lib
        ${MARIADB_ROOT_DIR}/usr/lib/x86_64-linux-gnu
        "C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib"
        "C:/Program Files (x86)/MariaDB/MariaDB Connector C/lib"
)

if(WIN32)
    # libmariadb.lib is an import library, so the DLL has to travel with us.
    # A Connector/C install tree puts it in bin/ (the CMake default layout) or
    # beside the import library in lib/mariadb/, depending on the version - look
    # in both, plus next to whatever we actually resolved above.
    get_filename_component(_mariadb_lib_dir "${MARIADB_LIBRARY}" DIRECTORY)
    find_file(
        MARIADB_DLL
        NAMES
            libmariadb.dll
        PATHS
            ${_mariadb_lib_dir}
            ${MARIADB_ROOT_DIR}/bin
            ${MARIADB_ROOT_DIR}/lib
            "C:/Program Files/MariaDB/MariaDB Connector C 64-bit/lib"
            "C:/Program Files (x86)/MariaDB/MariaDB Connector C/lib"
        PATH_SUFFIXES
            mariadb
        NO_DEFAULT_PATH
    )
    unset(_mariadb_lib_dir)
endif()

if(MARIADB_INCLUDE_DIR AND MARIADB_LIBRARY)
    set(MARIADB_FOUND TRUE)
else()
    set(MARIADB_FOUND FALSE)
endif()
mark_as_advanced(
    MARIADB_INCLUDE_DIR
    MARIADB_LIBRARY
    MARIADB_DLL
)
