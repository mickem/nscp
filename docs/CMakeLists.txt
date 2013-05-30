cmake_minimum_required(VERSION 2.6)

PROJECT(copy_docs)

SET(ALL_FILES)
copy_single_file(ALL_FILES "Nagios Usage Guide.pdf" docs)
copy_single_file(ALL_FILES "NSClient++ Reference Manual.pdf" docs)
copy_single_file(ALL_FILES "conf.py" docs)
#copy_single_file(ALL_FILES "license.txt" .)
#copy_single_file(ALL_FILES "../changelog" .)

FILE (GLOB_RECURSE rst_DOCS "*.rst")
FILE (GLOB_RECURSE htm_DOCS "*.html")
FILE (GLOB_RECURSE png_DOCS "*.png")
FILE (GLOB_RECURSE py_DOCS "*.py")
FILE (GLOB_RECURSE inc_DOCS "*.inc")

SET(all_DOCS ${rst_DOCS} ${htm_DOCS} ${png_DOCS} ${py_DOCS} ${inc_DOCS})
FOREACH(file ${all_DOCS})
	get_filename_component(filename ${file} NAME)
	get_filename_component(filepath ${file} PATH)
	file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
	#MESSAGE(STATUS " + python - ${filename} - ${relpath}")
	copy_single_file(ALL_FILES ${relpath}/${filename} docs/source/${relpath})
ENDFOREACH(file ${all_DOCS})


ADD_CUSTOM_TARGET(copy_docs ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_docs PROPERTIES FOLDER "files")
#INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES \"${source_file}\")")

IF(SPHINX_FOUND)
	ADD_CUSTOM_TARGET(build_doc_sources
		COMMAND ${CMAKE_BINARY_DIR}/nscp py --settings dummy --load-all --script docs --format rst --output docs/source
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		DEPENDS nscp copy_scripts
		COMMENT "Generate RST files")
	
	if(NOT DEFINED SPHINX_THEME)
		set(SPHINX_THEME default)
	endif()
	if(NOT DEFINED SPHINX_THEME_DIR)
		set(SPHINX_THEME_DIR)
	endif()

	ADD_CUSTOM_TARGET(build_docs_html ALL
		${SPHINX_EXECUTABLE}
		-q -b html
		-d "${CMAKE_CURRENT_BINARY_DIR}/_cache"
		"${CMAKE_CURRENT_BINARY_DIR}/source"
		"${CMAKE_CURRENT_BINARY_DIR}/html"
		DEPENDS build_doc_sources copy_docs
		COMMENT "Building HTML documentation with Sphinx")

	ADD_CUSTOM_TARGET(build_docs_epub ALL
		${SPHINX_EXECUTABLE}
		-q -b epub
		-d "${CMAKE_CURRENT_BINARY_DIR}/_cache"
		"${CMAKE_CURRENT_BINARY_DIR}/source"
		"${CMAKE_CURRENT_BINARY_DIR}/epub"
		DEPENDS build_doc_sources copy_docs
		COMMENT "Building HTML documentation with Sphinx")
ENDIF(SPHINX_FOUND)
