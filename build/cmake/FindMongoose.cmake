# - Find mongoose source/include folder
# This module finds mongoose if it is installed and determines where 
# the files are. This code sets the following variables:
#
#  MONGOOSE_FOUND             - have mongoose been found
#  MONGOOSE_INCLUDE_DIR       - path to where mongoose/Server.h is found
#  MONGOOSE_LIBRARY           - name of library (project) to add as dependency
FIND_PATH(MONGOOSE_INCLUDE_DIR
	NAMES mongoose/Server.h
	PATHS
		${CMAKE_SOURCE_DIR}/libs/mongoose-cpp
		${MONGOOSE_DIR}
		${MONGOOSE_INCLUDE_DIR}
		${NSCP_INCLUDEDIR}
)

IF(MONGOOSE_INCLUDE_DIR)
	SET(MONGOOSE_FOUND TRUE)
	SET(MONGOOSE_LIBRARY _mongoose)
ELSE(MONGOOSE_INCLUDE_DIR)
	SET(MONGOOSE_FOUND FALSE)
	SET(MONGOOSE_LIBRARY)
ENDIF(MONGOOSE_INCLUDE_DIR)
MARK_AS_ADVANCED(
  MONGOOSE_INCLUDE_DIR
  MONGOOSE_LIBRARY
)
