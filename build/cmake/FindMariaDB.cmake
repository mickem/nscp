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

if(MARIADB_INCLUDE_DIR AND MARIADB_LIBRARY)
    set(MARIADB_FOUND TRUE)
else()
    set(MARIADB_FOUND FALSE)
endif()
mark_as_advanced(
    MARIADB_INCLUDE_DIR
    MARIADB_LIBRARY
)
