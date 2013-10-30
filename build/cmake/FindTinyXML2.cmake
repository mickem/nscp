# - Find tinyxml2 source/include folder
# This module finds tinyxml2 if it is installed and determines where 
# the files are. This code sets the following variables:
#
#  TINYXML2_FOUND             - have tinyxml2 been found
#  TINYXML2_INCLUDE_DIR       - path to where tinyxml2.h is found
#
FIND_PATH(TINYXML2_INCLUDE_DIR
	NAMES tinyxml2.h
	PATHS
		${CMAKE_SOURCE_DIR}/ext/tinyxml2
		${TINYXML2_DIR}
		${TINYXML2_INCLUDE_DIR}
		${NSCP_INCLUDEDIR}
)

IF(TINYXML2_INCLUDE_DIR)
	SET(TINYXML2_FOUND TRUE)
ELSE(TINYXML2_INCLUDE_DIR)
	SET(TINYXML2_FOUND FALSE)
ENDIF(TINYXML2_INCLUDE_DIR)
MARK_AS_ADVANCED(
  TINYXML2_INCLUDE_DIR
)
