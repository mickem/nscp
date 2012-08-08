MACRO(LOAD_SECTIONS _TARGET_LIST _path _title)
	message(STATUS "Adding all: ${_title}")
	SET(${_TARGET_LIST})
	FILE(GLOB TMP_LIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${_path})
	FOREACH(_CURRENT_MODULE ${TMP_LIST})
		get_filename_component(CURRENT_MODULE_PATH ${_CURRENT_MODULE} PATH)
		get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
		SET(BUILD_MODULE 0)
		SET(BUILD_MODULE_SKIP_REASON "")
		include(${_CURRENT_MODULE})
		IF(BUILD_MODULE)
			message(STATUS " + ${CURRENT_MODULE_NAME}")
			ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
			SET(${_TARGET_LIST} ${${_TARGET_LIST}} ${CURRENT_MODULE_NAME})
		ELSE(BUILD_MODULE)
			message(STATUS " - ${CURRENT_MODULE_NAME}: Skipped ${BUILD_MODULE_SKIP_REASON}")
		ENDIF(BUILD_MODULE)
	ENDFOREACH(_CURRENT_MODULE ${TMP_LIST})
ENDMACRO(LOAD_SECTIONS)


MACRO(copy_single_file _TARGET_LIST src destDir)
	GET_FILENAME_COMPONENT(TARGET ${src} NAME)
	SET(source_file ${CMAKE_CURRENT_SOURCE_DIR}/${src})
	IF(${destDir} STREQUAL ".")
		SET(target_file ${CMAKE_BINARY_DIR}/${TARGET})
	ELSE(${destDir} STREQUAL ".")
		SET(target_file ${CMAKE_BINARY_DIR}/${destDir}/${TARGET})
	ENDIF(${destDir} STREQUAL ".")
	#MESSAGE(STATUS " - Copying ${source_file} to ${target_file}...")
	ADD_CUSTOM_COMMAND(
		OUTPUT ${target_file}
		COMMAND cmake ARGS -E copy "${source_file}" "${target_file}"
		COMMENT Copying ${source_file} to ${target_file}
		DEPENDS ${source_file}
		)
	SET(${_TARGET_LIST} ${${_TARGET_LIST}} ${target_file})
ENDMACRO(copy_single_file)

MACRO(add_nscp_py_test name script)
	ADD_TEST("${name}"
		nscp 
			py 
			--settings dummy 
			--exec run 
			--script ${script}
			--query py_unittest
		)
ENDMACRO(add_nscp_py_test)

