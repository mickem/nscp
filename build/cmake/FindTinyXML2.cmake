# * Find tinyxml2 source/include folder This module finds tinyxml2 if it is
#   installed and determines where the files are. This code sets the following
#   variables:
#
# TINYXML2_FOUND             - have tinyxml2 been found TINYXML2_INCLUDE_DIR -
# path to where tinyxml2.h is found
#
find_path(
  TINYXML2_INCLUDE_DIR
  NAMES tinyxml2.h
  PATHS ${CMAKE_SOURCE_DIR}/ext/tinyxml2 ${TINYXML2_DIR}
        ${TINYXML2_INCLUDE_DIR} ${NSCP_INCLUDEDIR})

if(TINYXML2_INCLUDE_DIR)
  set(TINYXML2_FOUND TRUE)
else(TINYXML2_INCLUDE_DIR)
  set(TINYXML2_FOUND FALSE)
endif(TINYXML2_INCLUDE_DIR)
mark_as_advanced(TINYXML2_INCLUDE_DIR)
