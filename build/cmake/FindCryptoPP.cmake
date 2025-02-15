# -*- cmake -*-
#
# Find crypto++ library or sources.
#
# Input variables: CRYPTOPP_DIR    - set this to specify the crypto++ source to
# be built.
#
# Output variables:
#
# CRYPTOPP_FOUND                  - set if library was found
# CRYPTOPP_INCLUDE_DIR    - Set to where include files ar found (or sources)
# CRYPTOPP_LIBRARIES              - Set to library

find_path(CRYPTOPP_INCLUDE_DIR cryptlib.h
          PATHS ${CRYPTOPP_DIR} ${CRYPTOPP_ROOT} /usr/include/crypto++
                /usr/include/cryptopp /usr/include)

set(CRYPTOPP_LIB_ROOT)
if(CMAKE_CL_64)
  set(CRYPTOPP_LIB_ROOT ${CRYPTOPP_INCLUDE_DIR}/x64)
else()
  set(CRYPTOPP_LIB_ROOT ${CRYPTOPP_INCLUDE_DIR}/Win32)
endif()

find_library(
  CRYPTOPP_LIBRARIES_RELEASE
  NAMES crypto++ cryptlib cryptopp
  PATHS ${CRYPTOPP_LIB_ROOT}/Output/Release ${CRYPTOPP_LIB_ROOT}/Output
        /usr/lib/)
find_library(
  CRYPTOPP_LIBRARIES_DEBUG
  NAMES crypto++ cryptlib cryptopp
  PATHS ${CRYPTOPP_LIB_ROOT}/Output/Debug ${CRYPTOPP_LIB_ROOT}/Output /usr/lib/)

if(CMAKE_TRACE)
  message(STATUS " - CRYPTOPP_INCLUDE_DIR=${CRYPTOPP_INCLUDE_DIR}")
  message(STATUS " - CRYPTOPP_LIB_ROOT=${CRYPTOPP_LIB_ROOT}")
  message(STATUS " - CRYPTOPP_LIBRARIES_DEBUG=${CRYPTOPP_LIBRARIES_DEBUG}")
  message(STATUS " - CRYPTOPP_LIBRARIES_RELEASE=${CRYPTOPP_LIBRARIES_RELEASE}")
  message(STATUS " - CRYPTOPP_DIR=${CRYPTOPP_DIR}")
endif()

if(CRYPTOPP_INCLUDE_DIR
   AND CRYPTOPP_LIBRARIES_RELEASE
   AND CRYPTOPP_LIBRARIES_DEBUG)
  set(CRYPTOPP_FOUND TRUE)
  set(CRYPTOPP_LIBRARIES optimized ${CRYPTOPP_LIBRARIES_RELEASE} debug
                         ${CRYPTOPP_LIBRARIES_DEBUG})
endif()
if(CMAKE_TRACE)
  message(STATUS " - CRYPTOPP_FOUND=${CRYPTOPP_FOUND}")
  message(STATUS " - CRYPTOPP_LIBRARIES=${CRYPTOPP_LIBRARIES}")
endif()
