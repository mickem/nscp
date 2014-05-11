MACRO(LOAD_SECTIONS _TARGET_LIST _path _title)
	message(STATUS "Adding all: ${_title}")
	SET(${_TARGET_LIST})
	FILE(GLOB TMP_LIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${_path})
	FOREACH(_CURRENT_MODULE ${TMP_LIST})
		get_filename_component(CURRENT_MODULE_PATH ${_CURRENT_MODULE} PATH)
		get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
		SET(BUILD_MODULE 0)
		SET(BUILD_MODULE_SKIP_REASON "Skipped")
		SET(MODULE_NOTE "")
		include(${_CURRENT_MODULE})
		IF(BUILD_MODULE)
			IF(MODULE_NOTE)
				SET(MODULE_NOTE " (${MODULE_NOTE})")
			ENDIF(MODULE_NOTE)
			message(STATUS " + ${CURRENT_MODULE_NAME}${MODULE_NOTE}")
			ADD_SUBDIRECTORY("${CURRENT_MODULE_PATH}")
			SET(${_TARGET_LIST} ${${_TARGET_LIST}} ${CURRENT_MODULE_NAME})
		ELSE(BUILD_MODULE)
			message(STATUS " - ${CURRENT_MODULE_NAME}: ${BUILD_MODULE_SKIP_REASON}")
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
	INSTALL(FILES ${target_file} DESTINATION ${destDir})
ENDMACRO(copy_single_file)

MACRO(copy_single_file_755 _TARGET_LIST src destDir)
	GET_FILENAME_COMPONENT(TARGET ${src} NAME)
	SET(source_file ${CMAKE_CURRENT_SOURCE_DIR}/${src})
	IF(${destDir} STREQUAL ".")
		SET(target_file ${CMAKE_BINARY_DIR}/${TARGET})
	ELSE(${destDir} STREQUAL ".")
		SET(target_file ${CMAKE_BINARY_DIR}/${destDir}/${TARGET})
	ENDIF(${destDir} STREQUAL ".")
	#MESSAGE(STATUS " - Copying ${source_file} to ${target_file}...")
IF(WIN32)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${target_file}
		COMMAND cmake ARGS -E copy "${source_file}" "${target_file}"
		COMMENT Copying ${source_file} to ${target_file}
		DEPENDS ${source_file}
		)
ELSE(WIN32)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${target_file}
		COMMAND cmake ARGS -E copy "${source_file}" "${target_file}"
		COMMAND chmod ARGS 755 "${target_file}"
		COMMENT Copying ${source_file} to ${target_file}
		DEPENDS ${source_file}
		)
ENDIF(WIN32)
	SET(${_TARGET_LIST} ${${_TARGET_LIST}} ${target_file})
	INSTALL(FILES ${target_file} DESTINATION ${destDir})
ENDMACRO(copy_single_file_755)

MACRO(add_nscp_py_test name script)
	ADD_TEST("${name}"
		nscp 
			unit
			--language python
			--script ${script}
		)
ENDMACRO(add_nscp_py_test)

MACRO(add_nscp_lua_test name script)
IF (LUA_FOUND)
	ADD_TEST("${name}"
		nscp 
			unit
			--language lua
			--script ${script}.lua
			--log error
		)
ELSE (LUA_FOUND)
	MESSAGE(STATUS "Skipping test ${name} since lua is not avalible")
ENDIF (LUA_FOUND)
ENDMACRO(add_nscp_lua_test)

MACRO(CREATE_MODULE _SRCS _SOURCE _TARGET)
INCLUDE_DIRECTORIES(${_TARGET})
ADD_CUSTOM_COMMAND(
	OUTPUT ${_TARGET}/module.cpp  ${_TARGET}/module.hpp ${_TARGET}/module.def
	COMMAND ${PYTHON_EXECUTABLE}
		ARGS
		"${BUILD_PYTHON_FOLDER}/create_plugin_module.py" 
		--source ${_SOURCE}
		--target ${_TARGET}
	COMMENT Generating ${_TARGET}/module.cpp and ${_TARGET}/module.hpp from ${_SOURCE}/module.json
	DEPENDS ${_SOURCE}/module.json
	)
SET(${_SRCS} ${${_SRCS}} ${_TARGET}/module.cpp)
IF(WIN32)
	SET(${_SRCS} ${${_SRCS}} ${_TARGET}/module.hpp)
	SET(${_SRCS} ${${_SRCS}} ${_TARGET}/module.def)
ENDIF(WIN32)
ENDMACRO(CREATE_MODULE)


MACRO(OPENSSL_LINK_FIX _TARGET)
IF(WIN32)
	SET_TARGET_PROPERTIES(${_TARGET} PROPERTIES LINK_FLAGS /SAFESEH:NO)
ENDIF(WIN32)
ENDMACRO(OPENSSL_LINK_FIX)
