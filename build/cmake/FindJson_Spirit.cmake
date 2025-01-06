# * Find json-spirit source/include folder This module finds json-spirit if it
#   is installed and determines where the files are. This code sets the
#   following variables:
#
# JSON_SPIRIT_FOUND             - have json-spirit been found
# JSON_SPIRIT_INCLUDE_DIR       - path to where json-spirit is found
#
find_path(
  JSON_SPIRIT_INCLUDE_DIR
  NAMES json_spirit.h
  PATHS ${CMAKE_SOURCE_DIR}/ext/json-spirit/include/json_spirit
        ${JSON_SPRIT_DIR}/include/json_spirit ${NSCP_INCLUDEDIR})
find_path(
  JSON_SPIRIT_CMAKE
  NAMES CMakeLists.txt
  PATHS ${CMAKE_SOURCE_DIR}/ext/json-spirit/build ${JSON_SPRIT_DIR}/build
        ${NSCP_INCLUDEDIR})

if(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
  set(JSON_SPIRIT_FOUND TRUE)
  set(JSON_SPIRIT_LIBRARY_TYPE "STATIC")
  set(JSON_SPIRIT_BUILD_DEMOS FALSE)
  set(JSON_SPIRIT_BUILD_TESTS FALSE)
else(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
  set(JSON_SPIRIT_FOUND FALSE)
endif(JSON_SPIRIT_INCLUDE_DIR AND JSON_SPIRIT_CMAKE)
mark_as_advanced(JSON_SPIRIT_INCLUDE_DIR JSON_SPIRIT_CMAKE)
