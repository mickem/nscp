SET(CURRENT_MODULE_NAME "NSCP.Plugin.CSharpSample")
IF(CSHARP_FOUND AND WIN32)
	SET (BUILD_MODULE 0)
ELSE()
	SET(BUILD_MODULE_SKIP_REASON "CSharp not found")
ENDIF()
