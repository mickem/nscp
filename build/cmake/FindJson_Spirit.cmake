# - Find json-spirit source/include folder
# This module finds json-spirit if it is installed and determines where 
# the files are. This code sets the following variables:
#
#  JSON_SPIRIT_FOUND             - have json-spirit been found
#  JSON_SPIRIT_INCLUDE_DIR       - path to where json-spirit is found
#
FIND_PATH(JSON_SPIRIT_INCLUDE_DIR 
	NAMES json_spirit.h
	PATHS
		${CMAKE_SOURCE_DIR}/ext/json-spirit/include/json_spirit
		${JSON_SPRIT_DIR}/include/json_spirit
		${NSCP_INCLUDEDIR}
		)
FIND_PATH(JSON_SPIRIT_CMAKE 
	NAMES CMakeLists.txt
	PATHS
		${CMAKE_SOURCE_DIR}/ext/json-spirit/build
		${JSON_SPRIT_DIR}/build
		${NSCP_INCLUDEDIR}
		)

IF(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
	SET(JSON_SPIRIT_FOUND TRUE)
	SET(JSON_SPIRIT_LIBRARY_TYPE "STATIC")
	SET(JSON_SPIRIT_BUILD_DEMOS FALSE)
	SET(JSON_SPIRIT_BUILD_TESTS FALSE)
ELSE(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
	SET(JSON_SPIRIT_FOUND FALSE)
ENDIF(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
MARK_AS_ADVANCED(
  JSON_SPIRIT_INCLUDE_DIR
  JSON_SPIRIT_CMAKE
)
