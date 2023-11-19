cmake_minimum_required(VERSION 3.5)

IF(MKDOCS_FOUND)
  PROJECT(generate_docs)
  SET(DEFINEARG "/paths:module-path=%(base-path)/modules")
  ADD_CUSTOM_TARGET(generate_doc_sources
    COMMAND ${CMAKE_BINARY_DIR}/nscp client 
      --module PythonScript 
      --settings dummy 
      --load-all 
      --log info 
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
ELSE(MKDOCS_FOUND)
	MESSAGE(STATUS "mkdocs not found: No documentation will be built")
ENDIF(MKDOCS_FOUND)
