# -*- cmake -*-
#
# Find crypto++ library or sources.
#
# Input variables:
#   CRYPTOPP_DIR	- set this to specify the crypto++ source to be built.
#
# Output variables:
#
#   CRYPTOPP_FOUND			- set if library was found
#	CRYPTOPP_INCLUDE_DIR	- Set to where include files ar found (or sources)
#	CRYPTOPP_LIBRARIES		- Set to library
#   CRYPTOPP_IS_LIB			- set if library was found
#   CRYPTOPP_IS_SOURCE		- set if sources were found

FIND_PATH(CRYPTOPP_INCLUDE_DIR 
	cryptlib.h
	PATHS
		/usr/include/crypto++
		/usr/include
)

FIND_LIBRARY(CRYPTOPP_LIBRARIES
	NAMES crypto++
)

IF(CRYPTOPP_INCLUDE_DIR AND CRYPTOPP_LIBRARY)
	SET(CRYPTOPP_FOUND TRUE)
	SET(CRYPTOPP_IS_LIB TRUE)
	SET(CRYPTOPP_IS_SOURCE FALSE)
	IF(CMAKE_TRACE)
		MESSAGE(STATUS "Crypto++ libraries found in: ${CRYPTOPP_INCLUDE_DIR}")
	ENDIF(CMAKE_TRACE)
ELSE(CRYPTOPP_INCLUDE_DIR AND CRYPTOPP_LIBRARY)
	IF(CRYPTOPP_DIR)
		FIND_PATH(CRYPTOPP_INCLUDE_DIR 
			cryptlib.h
			PATHS
				${CRYPTOPP_DIR}
		)
		IF(CRYPTOPP_INCLUDE_DIR)
			IF(CMAKE_TRACE)
				MESSAGE(STATUS "Crypto++ source found in: ${CRYPTOPP_INCLUDE_DIR}")
			ENDIF(CMAKE_TRACE)
			SET(CRYPTOPP_FOUND TRUE)
			SET(CRYPTOPP_IS_LIB FALSE)
			SET(CRYPTOPP_IS_SOURCE TRUE)
		ENDIF(CRYPTOPP_INCLUDE_DIR)
	ENDIF(CRYPTOPP_DIR)
ENDIF(CRYPTOPP_INCLUDE_DIR AND CRYPTOPP_LIBRARY)
