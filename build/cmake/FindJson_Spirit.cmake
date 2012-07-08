# - Find tinyxml2 source/include folder
# This module finds tinyxml2 if it is installed and determines where 
# the files are. This code sets the following variables:
#
#  JSON_SPIRIT_FOUND             - have tinyxml2 been found
#  JSON_SPIRIT_INCLUDE_DIR       - path to where tinyxml2.h is found
#
FIND_PATH(JSON_SPIRIT_INCLUDE_DIR 
	NAMES json_spirit.h
	PATHS
		${JSON_SPRIT_DIR}/json_spirit
		${JSON_SPRIT_DIR}
		${NSCP_INCLUDEDIR}
		)

IF(JSON_SPIRIT_INCLUDE_DIR)
	SET(JSON_SPIRIT_FOUND TRUE)
ELSE(JSON_SPIRIT_INCLUDE_DIR)
	SET(JSON_SPIRIT_FOUND FALSE)
ENDIF(JSON_SPIRIT_INCLUDE_DIR)
MARK_AS_ADVANCED(
  JSON_SPIRIT_INCLUDE_DIR
)
