IF(Python3_Development_FOUND)
	SET (BUILD_MODULE 1)
ELSE()
	MESSAGE(STATUS "Disabling PythonScript since Python was not found")
ENDIF()
