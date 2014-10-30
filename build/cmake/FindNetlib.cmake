# - Find c++.netlib source/include folder
# This module finds cpp-netlib if it is installed and determines where 
# the files are. This code sets the following variables:
#
#  NETLIB_FOUND             - have cpp-netlib been found
#  NETLIB_INCLUDE_DIR       - path to where cpp-netlib is found
#  NETLIB_LIBRARY           - name of library (project) to add as dependency
FIND_PATH(NETLIB_INCLUDE_DIR
	NAMES boost/network.hpp
	PATHS
		${CMAKE_SOURCE_DIR}/ext/cpp-netlib
		${NETLIB_DIR}
		${NETLIB_INCLUDE_DIR}
		${NSCP_INCLUDEDIR}
)
IF(NETLIB_INCLUDE_DIR)
	SET(NETLIB_FOUND TRUE)
	SET(NETLIB_CLIENT_LIBRARY cppnetlib-client-connections)
	SET(NETLIB_SERVER_LIBRARY cppnetlib-server-parsers)
	SET(NETLIB_URI_LIBRARY cppnetlib-uri)
ELSE(NETLIB_INCLUDE_DIR)
	SET(NETLIB_FOUND FALSE)
	SET(NETLIB_CLIENT_LIBRARY)
	SET(NETLIB_SERVER_LIBRARY)
	SET(NETLIB_URI_LIBRARY)
ENDIF(NETLIB_INCLUDE_DIR)
MARK_AS_ADVANCED(
  NETLIB_INCLUDE_DIR
  NETLIB_LIBRARY
)
