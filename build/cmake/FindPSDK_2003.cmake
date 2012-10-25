IF(WIN32)
	FIND_PATH(PSDK_2003_INCLUDE_DIRS Windows.h
		PATHS
		${INC_PSDK_2003}
		${INC_PSDK_2003}/include
		"C:/Program Files/Microsoft Platform SDK/include"
		"C:/Program Files/Microsoft Platform SDK for Windows Server 2003 R2/include"
		NO_DEFAULT_PATH
	)
	IF(CMAKE_CL_64)
		FIND_PATH(PSDK_2003_LIBRARY_DIRS WSock32.Lib
			PATHS
			${INC_PSDK_2003}/lib/AMD64
			"C:/Program Files/Microsoft Platform SDK/lib/AMD64"
			"C:/Program Files/Microsoft Platform SDK for Windows Server 2003 R2/lib/AMD64"
		)
	ELSE(CMAKE_CL_64)
		FIND_PATH(PSDK_2003_LIBRARY_DIRS WS2_32.Lib
			PATHS
			${INC_PSDK_2003}/lib
			"C:/Program Files/Microsoft Platform SDK/lib"
			"C:/Program Files/Microsoft Platform SDK for Windows Server 2003 R2/lib"
		)
	ENDIF(CMAKE_CL_64)
	IF(PSDK_2003_INCLUDE_DIRS AND PSDK_2003_LIBRARY_DIRS)
		SET(PSDK_2003_FOUND TRUE)
	ELSE(PSDK_2003_INCLUDE_DIRS AND PSDK_2003_LIBRARY_DIRS)
		SET(PSDK_2003_FOUND FALSE)
	ENDIF(PSDK_2003_INCLUDE_DIRS AND PSDK_2003_LIBRARY_DIRS)
ENDIF(WIN32)
