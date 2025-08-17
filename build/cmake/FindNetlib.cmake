# * Find c++.netlib source/include folder This module finds cpp-netlib if it is
#   installed and determines where the files are. This code sets the following
#   variables:
#
# NETLIB_FOUND             - have cpp-netlib been found NETLIB_INCLUDE_DIR -
# path to where cpp-netlib is found NETLIB_LIBRARY           - name of library
# (project) to add as dependency
find_path(
    NETLIB_INCLUDE_DIR
    NAMES boost/network.hpp
    PATHS
        ${CMAKE_SOURCE_DIR}/ext/cpp-netlib
        ${NETLIB_DIR}
        ${NETLIB_INCLUDE_DIR}
        ${NSCP_INCLUDEDIR}
)
if(NETLIB_INCLUDE_DIR)
    set(NETLIB_FOUND TRUE)
    set(NETLIB_CLIENT_LIBRARY cppnetlib-client-connections)
    set(NETLIB_SERVER_LIBRARY cppnetlib-server-parsers)
    set(NETLIB_URI_LIBRARY cppnetlib-uri)
else(NETLIB_INCLUDE_DIR)
    set(NETLIB_FOUND FALSE)
    set(NETLIB_CLIENT_LIBRARY)
    set(NETLIB_SERVER_LIBRARY)
    set(NETLIB_URI_LIBRARY)
endif(NETLIB_INCLUDE_DIR)
mark_as_advanced(NETLIB_INCLUDE_DIR NETLIB_LIBRARY)
