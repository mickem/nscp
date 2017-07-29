cmake_minimum_required(VERSION 2.6)

IF(MKDOCS_FOUND)
IF(PYTHON_VERSION_MAJOR EQUAL 2 AND PYTHON_VERSION_MINOR LESS 7)
	MESSAGE(STATUS "Python to old: No documentation will be built")
ELSE()
  PROJECT(generate_docs)
  IF(WIN32)
    SET(DEFINEARG "/paths:module-path=\${base-path}/modules")
  ELSE()
    SET(DEFINEARG "/paths:module-path=%(base-path)/modules")
  ENDIF()
  ADD_CUSTOM_TARGET(generate_doc_sources
    COMMAND ${CMAKE_BINARY_DIR}/nscp client 
      --module PythonScript 
      --no-stderr 
      --settings dummy 
      --load-all 
      --log error 
      --log oneline 
      --define "\"${DEFINEARG}\""
      -- execute --script scripts/python/docs.py --input docs --output docs
    WORKING_DIRECTORY ${BUILD_ROOT_FOLDER}
    DEPENDS nscp ${ALL_MODULE_NAMES} copy_files
    COMMENT "Generate RST documentation source")
  ADD_DEPENDENCIES(generate_doc_sources nscp ${ALL_MODULE_NAMES})
	SET_TARGET_PROPERTIES(generate_doc_sources PROPERTIES FOLDER "docs")


	ADD_CUSTOM_TARGET(build_docs_html ALL
		${MKDOCS_EXECUTABLE}
		build
		--site-dir "${CMAKE_CURRENT_BINARY_DIR}/html"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		COMMENT "Building HTML documentation")
	ADD_DEPENDENCIES(build_docs_html generate_doc_sources)

	SET_TARGET_PROPERTIES(build_docs_html PROPERTIES FOLDER "docs")
	install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/" DESTINATION "${INSTALL_FILES_BASE}web/help")
ENDIF()
ELSE(MKDOCS_FOUND)
	MESSAGE(STATUS "mkdocs not found: No documentation will be built")
ENDIF(MKDOCS_FOUND)
