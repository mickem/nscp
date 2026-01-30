macro(LOAD_SECTIONS _TARGET_LIST _path _title)
    message(STATUS "Adding all: ${_title}")
    set(${_TARGET_LIST})
    file(GLOB TMP_LIST RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${_path})
    foreach(_CURRENT_MODULE ${TMP_LIST})
        get_filename_component(CURRENT_MODULE_PATH ${_CURRENT_MODULE} PATH)
        get_filename_component(CURRENT_MODULE_FILENAME ${_CURRENT_MODULE} NAME)
        get_filename_component(CURRENT_MODULE_NAME ${CURRENT_MODULE_PATH} NAME)
        if("${CURRENT_MODULE_FILENAME}" STREQUAL "CMakeLists.txt")
            message(STATUS " * ${CURRENT_MODULE_NAME}${MODULE_NOTE}")
            add_subdirectory("${CURRENT_MODULE_PATH}")
        else()
            set(BUILD_MODULE 0)
            set(BUILD_MODULE_SKIP_REASON "Skipped")
            set(MODULE_NOTE "")
            include(${_CURRENT_MODULE})
            if(BUILD_MODULE)
                if(MODULE_NOTE)
                    set(MODULE_NOTE " (${MODULE_NOTE})")
                endif(MODULE_NOTE)
                message(STATUS " + ${CURRENT_MODULE_NAME}${MODULE_NOTE}")
                add_subdirectory("${CURRENT_MODULE_PATH}")
                set(${_TARGET_LIST} ${${_TARGET_LIST}} ${CURRENT_MODULE_NAME})
            else(BUILD_MODULE)
                message(
                    STATUS
                    " - ${CURRENT_MODULE_NAME}: ${BUILD_MODULE_SKIP_REASON}"
                )
            endif(BUILD_MODULE)
        endif()
    endforeach(_CURRENT_MODULE ${TMP_LIST})
endmacro(LOAD_SECTIONS)

macro(copy_single_file_helper _TARGET_LIST src destDir CHMOD)
    get_filename_component(TARGET ${src} NAME)
    set(source_file ${CMAKE_CURRENT_SOURCE_DIR}/${src})
    if(${destDir} STREQUAL ".")
        set(target_file ${CMAKE_BINARY_DIR}/${TARGET})
    else(${destDir} STREQUAL ".")
        set(target_file ${CMAKE_BINARY_DIR}/${destDir}/${TARGET})
    endif(${destDir} STREQUAL ".")
    if(WIN32 OR ${CHMOD} EQUAL 0)
        add_custom_command(
            OUTPUT ${target_file}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy "${source_file}" "${target_file}"
            COMMENT "Copying ${source_file} to ${target_file}"
            DEPENDS ${source_file}
        )
    else()
        add_custom_command(
            OUTPUT ${target_file}
            COMMAND ${CMAKE_COMMAND}
            ARGS -E copy "${source_file}" "${target_file}"
            COMMAND chmod
            ARGS 755 "${target_file}"
            COMMENT "Copying ${source_file} to ${target_file}"
            DEPENDS ${source_file}
        )
    endif()
    set(${_TARGET_LIST} ${${_TARGET_LIST}} ${target_file})
endmacro()
macro(copy_single_test_file _TARGET_LIST src destDir)
    copy_single_file_helper(${_TARGET_LIST} ${src} ${destDir} 0)
endmacro()

macro(copy_single_file _TARGET_LIST src destDir)
    copy_single_file_helper(${_TARGET_LIST} ${src} ${destDir} 0)
    install(FILES ${target_file} DESTINATION ${INSTALL_FILES_BASE}${destDir})
endmacro()

macro(copy_single_file_755 _TARGET_LIST src destDir)
    copy_single_file_helper(${_TARGET_LIST} ${src} ${destDir} 1)
    install(FILES ${target_file} DESTINATION ${INSTALL_FILES_BASE}${destDir})
endmacro()

macro(CREATE_MODULE _SRCS _SOURCE _TARGET)
    include_directories(${_TARGET})
    add_custom_command(
        OUTPUT
            ${_TARGET}/module.cpp
            ${_TARGET}/module.hpp
            ${_TARGET}/module.def
            ${_TARGET}/module.rc
        COMMAND ${Python3_EXECUTABLE}
        ARGS
            "${BUILD_PYTHON_FOLDER}/create_plugin_module.py" --source ${_SOURCE}
            --target ${_TARGET}
        COMMENT
            "Generating ${_TARGET}/module.cpp and ${_TARGET}/module.hpp from ${_SOURCE}/module.json"
        DEPENDS ${_SOURCE}/module.json
    )
    set(${_SRCS} ${${_SRCS}} ${_TARGET}/module.cpp)
    if(WIN32)
        set(${_SRCS} ${${_SRCS}} ${_TARGET}/module.hpp)
        set(${_SRCS} ${${_SRCS}} ${_TARGET}/module.def)
        set(${_SRCS} ${${_SRCS}} ${_TARGET}/module.rc)
    endif(WIN32)
endmacro(CREATE_MODULE)

macro(CREATE_ZIP_MODULE _MODULE _SOURCE)
    # ADD_CUSTOM_TARGET(
    add_custom_command(
        OUTPUT ${BUILD_TARGET_LIB_PATH}/${_MODULE}.zip
        COMMAND ${Python3_EXECUTABLE}
        ARGS
            "${BUILD_PYTHON_FOLDER}/create_zip_module.py" --source ${_SOURCE}
            --target ${BUILD_TARGET_LIB_PATH}
        COMMENT "Generating ${BUILD_TARGET_LIB_PATH}/${_MODULE}.zip"
        DEPENDS ${_SOURCE}/module.json
    )
endmacro(CREATE_ZIP_MODULE)

macro(OPENSSL_LINK_FIX _TARGET)
    if(WIN32)
        set_target_properties(${_TARGET} PROPERTIES LINK_FLAGS /SAFESEH:NO)
        set_target_properties(${_TARGET} PROPERTIES LINK_FLAGS /IGNORE:4099)
    endif(WIN32)
endmacro(OPENSSL_LINK_FIX)

macro(SET_LIBRARY_OUT_FOLDER _TARGET)
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BUILD_TARGET_EXE_PATH}
        )
    endif()
endmacro(SET_LIBRARY_OUT_FOLDER)

macro(SET_LIBRARY_OUT_FOLDER_MODULE _TARGET)
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BUILD_TARGET_EXE_PATH}
        )
    endif()
endmacro(SET_LIBRARY_OUT_FOLDER_MODULE)

macro(COPY_FILE _SOURCE _TARGET)
    if(
        (${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} GREATER 6)
        OR ${CMAKE_MAJOR_VERSION} GREATER 2
    )
        file(COPY ${_SOURCE} DESTINATION ${_TARGET})
    else()
        add_custom_command(
            TARGET copy_${_SOURCE}
            PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${_SOURCE} ${_TARGET}
        )
    endif()
endmacro()

macro(NSCP_DEBUG_SYMBOLS TARGET_NAME)
    if(WIN32)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES LINK_FLAGS "/PDBSTRIPPED:${TARGET_NAME}-stripped.pdb"
        )
    endif(WIN32)
endmacro()

macro(NSCP_INSTALL_MODULE _TARGET)
    if(WIN32)
        set(_FOLDER "${MODULE_SUBFOLDER}")
        install(
            TARGETS ${_TARGET}
            RUNTIME DESTINATION ${_FOLDER}
            LIBRARY DESTINATION ${_FOLDER}
            ARCHIVE DESTINATION ${_FOLDER}
        )
    elseif(APPLE)
        set(_FOLDER ${MODULE_SUBFOLDER})
        install(TARGETS ${_TARGET} LIBRARY DESTINATION ${_FOLDER})
    else()
        set(_FOLDER ${MODULE_TARGET_FOLDER})
        install(TARGETS ${_TARGET} LIBRARY DESTINATION ${_FOLDER})
    endif()
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_ROOT_PATH}/${_FOLDER}
        )
    endif()
endmacro()

macro(NSCP_MAKE_LIBRARY _TARGET _SRCS)
    if(USE_STATIC_RUNTIME)
        add_library(${_TARGET} STATIC ${_SRCS})
        if(
            CMAKE_COMPILER_IS_GNUCXX
            AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64"
            AND NOT APPLE
        )
            set_target_properties(${_TARGET} PROPERTIES COMPILE_FLAGS "-fPIC")
        endif(
            CMAKE_COMPILER_IS_GNUCXX
            AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64"
            AND NOT APPLE
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES VERSION "${NSCP_LIB_VERSION}"
        )
    else(USE_STATIC_RUNTIME)
        add_library(${_TARGET} SHARED ${_SRCS})
        set_library_out_folder(${_TARGET})
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                VERSION "${NSCP_LIB_VERSION}"
                SOVERSION "${NSCP_LIB_VERSION}"
        )
    endif(USE_STATIC_RUNTIME)
    set_target_properties(${_TARGET} PROPERTIES FOLDER "libraries")

    if(NOT USE_STATIC_RUNTIME)
        if(WIN32)
            install(
                TARGETS ${_TARGET}
                RUNTIME DESTINATION .
                LIBRARY DESTINATION .
            )
        else()
            install(TARGETS ${_TARGET} LIBRARY DESTINATION ${LIB_TARGET_FOLDER})
        endif()
        if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
            set_target_properties(
                ${_TARGET}
                PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT_PATH}
            )
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_ROOT_PATH}
            )
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BUILD_TARGET_ROOT_PATH}
            )
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                        ${BUILD_TARGET_ROOT_PATH}
            )
        endif()
    endif(NOT USE_STATIC_RUNTIME)
endmacro()

macro(NSCP_MAKE_EXE _TARGET _SRCS _FOLDER)
    add_executable(${_TARGET} ${_SRCS})
    nscp_debug_symbols(${_TARGET})
    if(WIN32)
        install(TARGETS ${_TARGET} RUNTIME DESTINATION .)
    else()
        install(TARGETS ${_TARGET} RUNTIME DESTINATION ${_FOLDER})
    endif()
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_ROOT_PATH}
        )
    endif()
endmacro()

macro(nscp_add_test _TARGET)
    target_compile_definitions(${_TARGET} PRIVATE NSCAPI_UNIT_TESTS)
    if(MSVC11)
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                COMPILE_FLAGS
                    "-DGTEST_HAS_TR1_TUPLE=1 -D_VARIADIC_MAX=10 -DGTEST_USE_OWN_TR1_TUPLE=0"
        )
    endif(MSVC11)
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(${_TARGET} PROPERTIES FOLDER "tests")
    endif()
    add_test(NAME ${_TARGET} COMMAND ${_TARGET})
endmacro()

macro(NSCP_MAKE_EXE_SBIN _TARGET _SRCS)
    nscp_make_exe(${_TARGET} "${_SRCS}" ${SBIN_TARGET_FOLDER})
endmacro()
macro(NSCP_MAKE_EXE_BIN _TARGET _SRCS)
    nscp_make_exe(${_TARGET} "${_SRCS}" ${BIN_TARGET_FOLDER})
endmacro()

macro(NSCP_FORCE_INCLUDE _TARGET _SRC)
    if(WIN32)
        string(REPLACE "/" "\\" WINSRC "${_SRC}")
        set_target_properties(
            ${TARGET}
            PROPERTIES COMPILE_FLAGS "/FI\"${WINSRC}\""
        )
    else(WIN32)
        if(
            USE_STATIC_RUNTIME
            AND CMAKE_COMPILER_IS_GNUCXX
            AND "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64"
            AND NOT APPLE
        )
            set_target_properties(
                ${TARGET}
                PROPERTIES COMPILE_FLAGS "-fPIC -include \"${_SRC}\""
            )
        else()
            set_target_properties(
                ${TARGET}
                PROPERTIES COMPILE_FLAGS "-include \"${_SRC}\""
            )
        endif()
    endif(WIN32)
endmacro()

macro(find_redist _TARGET_VAR)
    get_filename_component(_VS_BIN_FOLDER ${CMAKE_LINKER} PATH)
    get_filename_component(_VS_ROOT_FOLDER ${_VS_BIN_FOLDER} PATH)
    if(MSVC14)
        set(_VC_VERSION "140")
    elseif(MSVC12)
        set(_VC_VERSION "120")
    elseif(MSVC11)
        set(_VC_VERSION "110")
    elseif(MSVC10)
        set(_VC_VERSION "100")
    elseif(MSVC90)
        set(_VC_VERSION "90")
    elseif(MSVC80)
        set(_VC_VERSION "80")
    endif()
    if(CMAKE_CL_64)
        set(_VC_ARCH x64)
    else(CMAKE_CL_64)
        set(_VC_ARCH x86)
    endif(CMAKE_CL_64)
    set(_redit_folder
        "${_VS_ROOT_FOLDER}/redist/${_VC_ARCH}/Microsoft.VC${_VC_VERSION}.CRT"
    )
    file(GLOB ${_TARGET_VAR} "${_redit_folder}/*.dll")
endmacro(find_redist)
