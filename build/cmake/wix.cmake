# - Try to find Windows Installer XML
# See http://wix.sourceforge.net
#
# The follwoing variables are optionally searched for defaults
#  WIX_ROOT_DIR:            Base directory of WIX2 tree to use.
#
# The following are set after configuration is done: 
#  WIX_FOUND
#  WIX_ROOT_DIR
#  WIX_CANDLE
#  WIX_LIGHT
# 
# 2009/02 Petr Pytelka (pyta at lightcomp.cz)
#

if (WIN32)
	MACRO(DBG_MSG _MSG)
#		MESSAGE(STATUS "${CMAKE_CURRENT_LIST_FILE}(${CMAKE_CURRENT_LIST_LINE}):\r\n ${_MSG}")
	ENDMACRO(DBG_MSG)


    # typical root dirs of installations, exactly one of them is used
    SET (WIX_POSSIBLE_ROOT_DIRS
        "${WIX_ROOT_DIR}"
        "$ENV{WIX}"
        "$ENV{WIX_ROOT_DIR}"
        "$ENV{ProgramFiles}/WiX Toolset v3.10"
        "$ENV{ProgramFiles}/WiX Toolset v3.9"
        "$ENV{ProgramFiles}/WiX Toolset v3.8"
        "$ENV{ProgramFiles}/WiX Toolset v3.7"
        "$ENV{ProgramFiles}/Windows Installer XML v3.8"
        "$ENV{ProgramFiles}/Windows Installer XML v3.7"
        "$ENV{ProgramFiles}/Windows Installer XML v3.5"
        "$ENV{ProgramFiles}/Windows Installer XML v3"
        "$ENV{ProgramFiles}/Windows Installer XML"
        )


    #
    # select exactly ONE WIX base directory/tree 
    # to avoid mixing different version headers and libs
    #
    FIND_PATH(WIX_ROOT_DIR 
        NAMES 
        bin/candle.exe
        bin/light.exe
        bin/heat.exe
        PATHS ${WIX_POSSIBLE_ROOT_DIRS})
    DBG_MSG("WIX_ROOT_DIR=${WIX_ROOT_DIR}")
	
	IF(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")
		SET(WIX_VERSION 3)
	ELSE(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")
		SET(WIX_VERSION 2)
	ENDIF(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")
	


    #
    # Logic selecting required libs and headers
    #
    SET(WIX_FOUND OFF)
    IF(WIX_ROOT_DIR)
        SET(WIX_FOUND ON)
    ENDIF(WIX_ROOT_DIR)

    # display help message
    IF(NOT WIX_FOUND)
        # make FIND_PACKAGE friendly
        IF(NOT WIX_FIND_QUIETLY)
            IF(WIX_FIND_REQUIRED)
                MESSAGE(FATAL_ERROR
                    "Windows Installer XML required but some files not found. Please specify it's location with WIX_ROOT_DIR env. variable.")
            ELSE(WIX_FIND_REQUIRED)
                MESSAGE(STATUS 
                    "ERROR: Windows Installer XML was not found.")
            ENDIF(WIX_FIND_REQUIRED)
        ENDIF(NOT WIX_FIND_QUIETLY)
    ELSE(NOT WIX_FOUND)
        SET(WIX_CANDLE ${WIX_ROOT_DIR}/bin/candle.exe)
        SET(WIX_LIGHT ${WIX_ROOT_DIR}/bin/light.exe)
        SET(WIX_HEAT ${WIX_ROOT_DIR}/bin/heat.exe)
        #  MESSAGE(STATUS "Windows Installer XML found.")
    ENDIF(NOT WIX_FOUND)

    MARK_AS_ADVANCED(
        WIX_ROOT_DIR
        WIX_CANDLE
        WIX_LIGHT
        WIX_HEAT
        )

    #
    # Call wix compiler
    #
    # Parameters:
    #  _sources - a list with sources
    #  _obj - name of list for target objects
    #
    MACRO(WIX_COMPILE _sources _objs _extra_dep)
        DBG_MSG("WIX compile: ${_sources}")
        FOREACH (_current_FILE ${_sources})
            GET_FILENAME_COMPONENT(_tmp_FILE ${_current_FILE} ABSOLUTE)
            GET_FILENAME_COMPONENT(_basename ${_tmp_FILE} NAME_WE)
            GET_FILENAME_COMPONENT(_ext ${_tmp_FILE} EXT)

            SET (SOURCE_WIX_FILE ${_tmp_FILE} )
            DBG_MSG("WIX source file: ${SOURCE_WIX_FILE}")

			IF(${_ext} STREQUAL ".wixlib")
				SET (OUTPUT_WIXOBJ ${_tmp_FILE})
				DBG_MSG("WIX output: ${_tmp_FILE}")
			ELSE(${_ext} STREQUAL ".wixlib")
				SET (OUTPUT_WIXOBJ ${_basename}.wixobj )

				DBG_MSG("WIX output: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}")
				DBG_MSG("WIX command: ${WIX_CANDLE}")

				ADD_CUSTOM_COMMAND( 
					OUTPUT    ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
					COMMAND   ${WIX_CANDLE}
					ARGS       
								-ext WixFirewallExtension 
								-ext WixUtilExtension
								${WIX_CANDLE_FLAGS} 
								${SOURCE_WIX_FILE}
					DEPENDS   ${SOURCE_WIX_FILE} ${_extra_dep}
					COMMENT   "Compiling ${SOURCE_WIX_FILE} -ext WixFirewallExtension -ext WixUtilExtension ${WIX_CANDLE_FLAGS} -> ${OUTPUT_WIXOBJ}"
					)
				SET (OUTPUT_WIXOBJ ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ})
			ENDIF(${_ext} STREQUAL ".wixlib")
			SET(${_objs} ${${_objs}} ${OUTPUT_WIXOBJ})
			DBG_MSG("WIX compile output: ${${_objs}}")

        ENDFOREACH (_current_FILE)
    ENDMACRO(WIX_COMPILE)

    #
    # Call wix heat command for the specified DLLs
    #
    # Parameters:
    #  _sources - name of list with DLLs
    #  _obj - name of list for target objects
    #
    MACRO(WIX_HEAT _sources _objs)
        DBG_MSG("WiX heat: ${_sources}")
        FOREACH (_current_DLL ${_sources})
            GET_FILENAME_COMPONENT(_tmp_FILE ${_current_DLL} ABSOLUTE)
            GET_FILENAME_COMPONENT(_basename ${_tmp_FILE} NAME_WE)

            SET (SOURCE_WIX_FILE ${_tmp_FILE} )

            SET (OUTPUT_WIXOBJ ${_basename}.wxs )

            DBG_MSG("WIX output: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}")
            DBG_MSG("WIX command: ${WIX_HEAT}")

            ADD_CUSTOM_COMMAND( 
                OUTPUT    ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
                COMMAND   ${WIX_HEAT}
                ARGS      file ${SOURCE_WIX_FILE}
						  -ext WixFirewallExtension
						  -ext WixUtilExtension
                          -out ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
                          ${WIX_HEAT_FLAGS}
                DEPENDS   ${SOURCE_WIX_FILE}
                COMMENT   "Compiling ${SOURCE_WIX_FILE} -> ${OUTPUT_WIXOBJ}"
                )
            SET(${_objs} ${${_objs}} ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ} )
        ENDFOREACH(_current_DLL)
        DBG_MSG("WIX compile output: ${${_objs}}")
    ENDMACRO(WIX_HEAT)

    #
    # Link MSI file as post-build action
    #
    # Parameters
    #  _target - Name of target file
    #  _sources - A list with sources
	#  _loc_files - A list of localization files
    #
    MACRO(WIX_LINK _target _sources _loc_files)
        DBG_MSG("WIX command: ${WIX_LIGHT}\n WIX target: ${_target} objs: ${${_sources}}")
        DBG_MSG("WIX version: ${WIX_VERSION}")

        SET(WIX_LINK_FLAGS_A ${WIX_LINK_FLAGS})
        # Add localization
        FOREACH(_current_FILE ${_loc_files})
			IF(${WIX_VERSION} EQUAL 3)
				SET(WIX_LINK_FLAGS_A ${WIX_LINK_FLAGS_A} -cultures:"${_current_FILE}")
			ELSE(${WIX_VERSION} EQUAL 3)
				SET(WIX_LINK_FLAGS_A ${WIX_LINK_FLAGS_A} -loc "${_current_FILE}")
			ENDIF(${WIX_VERSION} EQUAL 3)
            DBG_MSG("WIX link localization: ${_current_FILE}")
        ENDFOREACH(_current_FILE)
        DBG_MSG("WIX link flags: ${WIX_LINK_FLAGS_A}")

		IF(${WIX_VERSION} EQUAL 3)
			ADD_CUSTOM_COMMAND(
				OUTPUT    ${_target}
				COMMAND   ${WIX_LIGHT}
				ARGS      ${WIX_LINK_FLAGS_A}
							-b ${CMAKE_CURRENT_SOURCE_DIR}
							-ext WixUIExtension 
							-ext WixFirewallExtension 
							-ext WixUtilExtension 
							-out "${_target}" 
							${${_sources}}
				DEPENDS   ${${_sources}}
				COMMENT   "Linking ${${_sources}} -> ${_target} (${WIX_LIGHT} ${WIX_LINK_FLAGS_A} -ext WixUIExtension -ext WixFirewallExtension -out \"${_target}\" ${${_sources}})"
				)
		ELSE(${WIX_VERSION} EQUAL 3)
			ADD_CUSTOM_COMMAND(
				OUTPUT    ${_target}
				COMMAND   ${WIX_LIGHT}
				ARGS      ${WIX_LINK_FLAGS_A} -out "${_target}" ${${_sources}}
				DEPENDS   ${${_sources}}
				COMMENT   "Linking ${${_sources}} -> ${_target} (${WIX_LINK_FLAGS_A})"
				)
		ENDIF(${WIX_VERSION} EQUAL 3)
    ENDMACRO(WIX_LINK)
	
    #
    # Create an installer from sourcefiles
    #
    # Parameters
    #  _target - Name of target file
    #  _sources - Name of list with sources
    #
    MACRO(ADD_WIX_INSTALLER	_target _sources _dependencies _loc_files)
		SET(WIX_OBJ_LIST)
		WIX_COMPILE("${_sources}" WIX_OBJ_LIST "${_dependencies}")
		SET(TNAME "${_target}-${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}-${VERSION_ARCH}.msi")
		WIX_LINK(${TNAME} WIX_OBJ_LIST "${_loc_files}")
		ADD_CUSTOM_TARGET(installer_${_target} 
			ALL
			DEPENDS ${TNAME}
			SOURCES ${_sources}
			)
		sign_file(installer_${_target} "${CMAKE_CURRENT_BINARY_DIR}/${TNAME}")
    ENDMACRO(ADD_WIX_INSTALLER)

	MACRO(WIX_FIND_MERGE_MODULE _VAR _FILE)
		IF(CMAKE_CL_64)
			SET(ARCH x64)
		ELSE(CMAKE_CL_64)
			SET(ARCH x86)
		ENDIF(CMAKE_CL_64)
		FIND_FILE(${_VAR}
			NAMES 
			"${_FILE}.msm"
			"${_FILE}_${ARCH}.msm"
			PATHS 
			${WIX_MERGE_MODULE_PATH}
			"$ENV{ProgramFiles}/Common Files/Merge Modules"
			${WIX_POSSIBLE_ROOT_DIRS}
			)
		SET(${_VAR} ${${_VAR}} PARENT_SCOPE)
	ENDMACRO(WIX_FIND_MERGE_MODULE)

endif(WIN32)
