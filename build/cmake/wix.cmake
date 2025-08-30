# * Try to find Windows Installer XML See http://wix.sourceforge.net
#
# The follwoing variables are optionally searched for defaults WIX_ROOT_DIR:
# Base directory of WIX2 tree to use.
#
# The following are set after configuration is done: WIX_FOUND WIX_ROOT_DIR
# WIX_CANDLE WIX_LIGHT
#
# 2009/02 Petr Pytelka (pyta at lightcomp.cz)
#

if(WIN32)
    macro(DBG_MSG _MSG)
        # MESSAGE(STATUS
        # "${CMAKE_CURRENT_LIST_FILE}(${CMAKE_CURRENT_LIST_LINE}):\r\n ${_MSG}")
    endmacro(DBG_MSG)

    set(PF86 "PROGRAMFILES(X86)")

    # typical root dirs of installations, exactly one of them is used
    set(WIX_POSSIBLE_ROOT_DIRS
        "${WIX_ROOT_DIR}"
        "$ENV{WIX}"
        "$ENV{WIX_ROOT_DIR}"
        "$ENV{${PF86}}/WiX Toolset v3.14"
        "$ENV{${PF86}}/WiX Toolset v3.11"
    )

    #
    # select exactly ONE WIX base directory/tree to avoid mixing different version
    # headers and libs
    #
    find_path(
        WIX_ROOT_DIR
        NAMES bin/candle.exe bin/light.exe bin/heat.exe
        PATHS ${WIX_POSSIBLE_ROOT_DIRS}
    )
    dbg_msg("WIX_ROOT_DIR=${WIX_ROOT_DIR}")

    if(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")
        set(WIX_VERSION 3)
    else(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")
        set(WIX_VERSION 2)
    endif(EXISTS "${WIX_ROOT_DIR}/bin/pyro.exe")

    #
    # Logic selecting required libs and headers
    #
    set(WIX_FOUND OFF)
    if(WIX_ROOT_DIR)
        set(WIX_FOUND ON)
    endif(WIX_ROOT_DIR)

    # display help message
    if(NOT WIX_FOUND)
        # make FIND_PACKAGE friendly
        if(NOT WIX_FIND_QUIETLY)
            if(WIX_FIND_REQUIRED)
                message(
                    FATAL_ERROR
                    "Windows Installer XML required but some files not found. Please specify it's location with WIX_ROOT_DIR env. variable."
                )
            else(WIX_FIND_REQUIRED)
                message(STATUS "ERROR: Windows Installer XML was not found.")
            endif(WIX_FIND_REQUIRED)
        endif(NOT WIX_FIND_QUIETLY)
    else(NOT WIX_FOUND)
        set(WIX_CANDLE ${WIX_ROOT_DIR}/bin/candle.exe)
        set(WIX_LIGHT ${WIX_ROOT_DIR}/bin/light.exe)
        set(WIX_HEAT ${WIX_ROOT_DIR}/bin/heat.exe)
        # MESSAGE(STATUS "Windows Installer XML found.")
    endif(NOT WIX_FOUND)

    mark_as_advanced(WIX_ROOT_DIR WIX_CANDLE WIX_LIGHT WIX_HEAT)

    #
    # Call wix compiler
    #
    # Parameters: _sources - a list with sources _obj - name of list for target
    # objects
    #
    macro(WIX_COMPILE _sources _objs _extra_dep)
        dbg_msg("WIX compile: ${_sources}")
        foreach(_current_FILE ${_sources})
            get_filename_component(_tmp_FILE ${_current_FILE} ABSOLUTE)
            get_filename_component(_basename ${_tmp_FILE} NAME_WE)
            get_filename_component(_ext ${_tmp_FILE} EXT)

            set(SOURCE_WIX_FILE ${_tmp_FILE})
            dbg_msg("WIX source file: ${SOURCE_WIX_FILE}")

            if(${_ext} STREQUAL ".wixlib")
                set(OUTPUT_WIXOBJ ${_tmp_FILE})
                dbg_msg("WIX output: ${_tmp_FILE}")
            else(${_ext} STREQUAL ".wixlib")
                set(OUTPUT_WIXOBJ ${_basename}.wixobj)

                dbg_msg("WIX output: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}")
                dbg_msg("WIX command: ${WIX_CANDLE}")

                add_custom_command(
                    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
                    COMMAND ${WIX_CANDLE}
                    ARGS
                        -ext WixFirewallExtension -ext WixUtilExtension
                        ${WIX_CANDLE_FLAGS} ${SOURCE_WIX_FILE}
                    DEPENDS ${SOURCE_WIX_FILE} ${_extra_dep}
                    COMMENT
                        "Compiling ${SOURCE_WIX_FILE} -ext WixFirewallExtension -ext WixUtilExtension ${WIX_CANDLE_FLAGS} -> ${OUTPUT_WIXOBJ}"
                )
                set(OUTPUT_WIXOBJ ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ})
            endif(${_ext} STREQUAL ".wixlib")
            set(${_objs} ${${_objs}} ${OUTPUT_WIXOBJ})
            dbg_msg("WIX compile output: ${${_objs}}")
        endforeach(_current_FILE)
    endmacro(WIX_COMPILE)

    #
    # Call wix heat command for the specified DLLs
    #
    # Parameters: _sources - name of list with DLLs _obj - name of list for target
    # objects
    #
    macro(WIX_HEAT _sources _objs)
        dbg_msg("WiX heat: ${_sources}")
        foreach(_current_DLL ${_sources})
            get_filename_component(_tmp_FILE ${_current_DLL} ABSOLUTE)
            get_filename_component(_basename ${_tmp_FILE} NAME_WE)

            set(SOURCE_WIX_FILE ${_tmp_FILE})

            set(OUTPUT_WIXOBJ ${_basename}.wxs)

            dbg_msg("WIX output: ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}")
            dbg_msg("WIX command: ${WIX_HEAT}")

            add_custom_command(
                OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
                COMMAND ${WIX_HEAT}
                ARGS
                    file ${SOURCE_WIX_FILE} -ext WixFirewallExtension -ext
                    WixUtilExtension -out
                    ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
                    ${WIX_HEAT_FLAGS}
                DEPENDS ${SOURCE_WIX_FILE}
                COMMENT "Compiling ${SOURCE_WIX_FILE} -> ${OUTPUT_WIXOBJ}"
            )
            set(${_objs}
                ${${_objs}}
                ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_WIXOBJ}
            )
        endforeach(_current_DLL)
        dbg_msg("WIX compile output: ${${_objs}}")
    endmacro(WIX_HEAT)

    #
    # Link MSI file as post-build action
    #
    # Parameters _target - Name of target file _sources - A list with sources
    # _loc_files - A list of localization files
    #
    macro(WIX_LINK _target _sources _loc_files)
        dbg_msg(
          "WIX command: ${WIX_LIGHT}\n WIX target: ${_target} objs: ${${_sources}}"
        )
        dbg_msg("WIX version: ${WIX_VERSION}")

        set(WIX_LINK_FLAGS_A ${WIX_LINK_FLAGS})
        # Add localization
        foreach(_current_FILE ${_loc_files})
            if(${WIX_VERSION} EQUAL 3)
                set(WIX_LINK_FLAGS_A
                    ${WIX_LINK_FLAGS_A}
                    -cultures:"${_current_FILE}"
                )
            else(${WIX_VERSION} EQUAL 3)
                set(WIX_LINK_FLAGS_A
                    ${WIX_LINK_FLAGS_A}
                    -loc
                    "${_current_FILE}"
                )
            endif(${WIX_VERSION} EQUAL 3)
            dbg_msg("WIX link localization: ${_current_FILE}")
        endforeach(_current_FILE)
        dbg_msg("WIX link flags: ${WIX_LINK_FLAGS_A}")

        if(${WIX_VERSION} EQUAL 3)
            add_custom_command(
                OUTPUT ${_target}
                COMMAND ${WIX_LIGHT}
                ARGS
                    ${WIX_LINK_FLAGS_A} -b ${CMAKE_CURRENT_SOURCE_DIR} -ext
                    WixUIExtension -ext WixFirewallExtension -ext
                    WixUtilExtension -out "${_target}" ${${_sources}}
                DEPENDS ${${_sources}}
                COMMENT
                    "Linking ${${_sources}} -> ${_target} (${WIX_LIGHT} ${WIX_LINK_FLAGS_A} -ext WixUIExtension -ext WixFirewallExtension -out \"${_target}\" ${${_sources}})"
            )
        else(${WIX_VERSION} EQUAL 3)
            add_custom_command(
                OUTPUT ${_target}
                COMMAND ${WIX_LIGHT}
                ARGS ${WIX_LINK_FLAGS_A} -out "${_target}" ${${_sources}}
                DEPENDS ${${_sources}}
                COMMENT
                    "Linking ${${_sources}} -> ${_target} (${WIX_LINK_FLAGS_A})"
            )
        endif(${WIX_VERSION} EQUAL 3)
    endmacro(WIX_LINK)

    #
    # Create an installer from sourcefiles
    #
    # Parameters _target - Name of target file _sources - Name of list with
    # sources
    #
    macro(ADD_WIX_INSTALLER _target _sources _dependencies _loc_files)
        set(WIX_OBJ_LIST)
        wix_compile("${_sources}" WIX_OBJ_LIST "${_dependencies}")
        set(TNAME
            "${_target}-${BUILD_VERSION}-${VERSION_ARCH}${RELEASE_SUFFIX}.msi"
        )
        wix_link(${TNAME} WIX_OBJ_LIST "${_loc_files}")
        add_custom_target(
            installer_${_target}
            ALL
            DEPENDS ${TNAME}
            SOURCES ${_sources}
        )
    endmacro(ADD_WIX_INSTALLER)

    macro(WIX_FIND_MERGE_MODULE _VAR _FILE)
        if(CMAKE_CL_64)
            set(ARCH x64)
        else(CMAKE_CL_64)
            set(ARCH x86)
        endif(CMAKE_CL_64)
        find_file(
            ${_VAR}
            NAMES "${_FILE}.msm" "${_FILE}_${ARCH}.msm"
            PATHS
                "$ENV{VCInstallDir}/Redist/MSVC/v143/MergeModules"
                "$ENV{VCInstallDir}/Redist/MSVC/v142/MergeModules"
                ${WIX_MERGE_MODULE_PATH}
                "$ENV{ProgramFiles}/Common Files/Merge Modules"
                "c:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/v143/MergeModules"
                ${WIX_POSSIBLE_ROOT_DIRS}
        )
        set(${_VAR} ${${_VAR}} PARENT_SCOPE)
    endmacro(WIX_FIND_MERGE_MODULE)
endif(WIN32)
