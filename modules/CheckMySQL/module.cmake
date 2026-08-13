# Requires MariaDB Connector/C (LGPL), which speaks the native protocol to
# MySQL, MariaDB, Percona and other MySQL-compatible servers on both Windows
# and Linux.
find_package(MariaDB)
if(MARIADB_FOUND)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE_SKIP_REASON
        "MariaDB Connector/C not found (install libmariadb-dev or set MARIADB_ROOT_DIR)"
    )
endif()
