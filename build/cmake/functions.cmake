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
            option(
                BUILD_MODULE_${CURRENT_MODULE_NAME}
                "Build module ${CURRENT_MODULE_NAME}"
                ON
            )
            if(NOT BUILD_MODULE_${CURRENT_MODULE_NAME})
                set(BUILD_MODULE 0)
                set(BUILD_MODULE_SKIP_REASON
                    "Disabled (-DBUILD_MODULE_${CURRENT_MODULE_NAME}=OFF)"
                )
            endif()
            if(BUILD_MODULE)
                if(MODULE_NOTE)
                    set(MODULE_NOTE " (${MODULE_NOTE})")
                endif(MODULE_NOTE)
                message(STATUS " + ${CURRENT_MODULE_NAME}${MODULE_NOTE}")
                add_subdirectory("${CURRENT_MODULE_PATH}")
                set(${_TARGET_LIST}
                    ${${_TARGET_LIST}}
                    ${CURRENT_MODULE_NAME}
                )
            else(BUILD_MODULE)
                message(
                    STATUS
                    " - ${CURRENT_MODULE_NAME}: ${BUILD_MODULE_SKIP_REASON}"
                )
            endif(BUILD_MODULE)
        endif()
    endforeach(
        _CURRENT_MODULE
        ${TMP_LIST}
    )
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
            OUTPUT
                ${target_file}
            COMMAND
                ${CMAKE_COMMAND}
            ARGS
                -E copy "${source_file}" "${target_file}"
            COMMENT "Copying ${source_file} to ${target_file}"
            DEPENDS
                ${source_file}
        )
    else()
        add_custom_command(
            OUTPUT
                ${target_file}
            COMMAND
                ${CMAKE_COMMAND}
            ARGS
                -E copy "${source_file}" "${target_file}"
            COMMAND
                chmod
            ARGS
                755 "${target_file}"
            COMMENT "Copying ${source_file} to ${target_file}"
            DEPENDS
                ${source_file}
        )
    endif()
    set(${_TARGET_LIST}
        ${${_TARGET_LIST}}
        ${target_file}
    )
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
    install(PROGRAMS ${target_file} DESTINATION ${INSTALL_FILES_BASE}${destDir})
endmacro()

macro(CREATE_MODULE _SRCS _SOURCE _TARGET)
    include_directories(${_TARGET})
    add_custom_command(
        OUTPUT
            ${_TARGET}/module.cpp
            ${_TARGET}/module.hpp
            ${_TARGET}/module.def
            ${_TARGET}/module.rc
        COMMAND
            ${Python3_EXECUTABLE}
        ARGS
            "${BUILD_PYTHON_FOLDER}/create_plugin_module.py" --source ${_SOURCE}
            --target ${_TARGET}
        COMMENT
            "Generating ${_TARGET}/module.cpp and ${_TARGET}/module.hpp from ${_SOURCE}/module.json"
        DEPENDS
            ${_SOURCE}/module.json
            "${BUILD_PYTHON_FOLDER}/create_plugin_module.py"
    )
    set(${_SRCS}
        ${${_SRCS}}
        ${_TARGET}/module.cpp
    )
    if(WIN32)
        set(${_SRCS}
            ${${_SRCS}}
            ${_TARGET}/module.hpp
        )
        set(${_SRCS}
            ${${_SRCS}}
            ${_TARGET}/module.def
        )
        set(${_SRCS}
            ${${_SRCS}}
            ${_TARGET}/module.rc
        )
    endif(WIN32)
endmacro(CREATE_MODULE)

macro(CREATE_ZIP_MODULE _MODULE _SOURCE)
    # ADD_CUSTOM_TARGET(
    add_custom_command(
        OUTPUT
            ${BUILD_TARGET_LIB_PATH}/${_MODULE}.zip
        COMMAND
            ${Python3_EXECUTABLE}
        ARGS
            "${BUILD_PYTHON_FOLDER}/create_zip_module.py" --source ${_SOURCE}
            --target ${BUILD_TARGET_LIB_PATH}
        COMMENT "Generating ${BUILD_TARGET_LIB_PATH}/${_MODULE}.zip"
        DEPENDS
            ${_SOURCE}/module.json
    )
endmacro(CREATE_ZIP_MODULE)

macro(OPENSSL_LINK_FIX _TARGET)
    if(WIN32)
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LINK_FLAGS
                    "/SAFESEH:NO /IGNORE:4099"
        )
    endif(WIN32)
endmacro(OPENSSL_LINK_FIX)

macro(SET_LIBRARY_OUT_FOLDER _TARGET)
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_EXE_PATH}
        )
    endif()
endmacro(SET_LIBRARY_OUT_FOLDER)

macro(SET_LIBRARY_OUT_FOLDER_MODULE _TARGET)
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_EXE_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_EXE_PATH}
        )
    endif()
endmacro(SET_LIBRARY_OUT_FOLDER_MODULE)

macro(COPY_FILE _SOURCE _TARGET)
    if(
        (
            ${CMAKE_MAJOR_VERSION}
                EQUAL
                2
            AND ${CMAKE_MINOR_VERSION}
                GREATER
                6
        )
        OR ${CMAKE_MAJOR_VERSION}
            GREATER
            2
    )
        file(COPY ${_SOURCE} DESTINATION ${_TARGET})
    else()
        add_custom_command(
            TARGET copy_${_SOURCE}
            PRE_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy_directory ${_SOURCE} ${_TARGET}
        )
    endif()
endmacro()

macro(NSCP_DEBUG_SYMBOLS TARGET_NAME)
    if(WIN32)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES
                LINK_FLAGS
                    "/PDBSTRIPPED:${TARGET_NAME}-stripped.pdb"
        )
    endif(WIN32)
endmacro()

# Enable position-independent code on Linux (GCC) for 64-bit architectures.
#
# Static libraries that get linked into NSClient++ shared modules must be
# compiled with -fPIC. On x86_64 the linker tolerates non-PIC code via copy
# relocations, but on aarch64 / arm64 the relocation R_AARCH64_ADR_PREL_PG_HI21
# is not allowed in shared objects and the link fails outright.
#
# Apple is excluded (PIC handling is automatic on macOS) and so is anything
# that's not GCC; on MSVC the concept doesn't apply.
function(nscp_apply_pic _TARGET)
    if(
        CMAKE_COMPILER_IS_GNUCXX
        AND CMAKE_SYSTEM_PROCESSOR
            MATCHES
            "x86_64|aarch64|arm64"
        AND NOT APPLE
    )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                POSITION_INDEPENDENT_CODE
                    ON
        )
    endif()
endfunction()

macro(NSCP_INSTALL_MODULE _TARGET)
    if(WIN32)
        set(_FOLDER "${MODULE_SUBFOLDER}")
        install(
            TARGETS
                ${_TARGET}
            RUNTIME
                DESTINATION ${_FOLDER}
            LIBRARY
                DESTINATION ${_FOLDER}
            ARCHIVE
                DESTINATION ${_FOLDER}
        )
    else()
        # macOS used to have its own branch here, installing into a bare
        # "modules" folder with no RPATH at all - which predates the
        # prefix-derived layout and left a macOS build unable to find its own
        # private libraries. It follows the same layout as every other unix now;
        # only the RPATH token differs (@loader_path vs $ORIGIN), and that is
        # already baked into NSCP_RPATH_MODULE.
        set(_FOLDER ${MODULE_TARGET_FOLDER})
        # Modules sit in NSCP_PKGLIBDIR/modules and load the project's private
        # shared libraries from NSCP_PKGLIBDIR one directory up. Carry a
        # loader-relative RPATH so they resolve regardless of install prefix.
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                INSTALL_RPATH
                    "${NSCP_RPATH_MODULE}"
        )
        install(TARGETS ${_TARGET} LIBRARY DESTINATION ${_FOLDER})
    endif()
    # Where the module lands in the *build* tree (the install destination is
    # above). BUILD_TARGET_LIB_PATH, not BUILD_TARGET_ROOT_PATH/${_FOLDER}: on
    # unix _FOLDER is the absolute install path now, and concatenating the two
    # would drop the modules at <build>//usr/local/lib/nsclient/modules. It is
    # the same directory as before on MSVC, where _FOLDER is "modules".
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY
                    ${BUILD_TARGET_LIB_PATH}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_LIB_PATH}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_LIB_PATH}
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_LIB_PATH}
        )
    endif()
endmacro()

macro(NSCP_MAKE_LIBRARY _TARGET _SRCS)
    # Optional trailing EXCLUDE_FROM_ALL keyword: keep the target out of the
    # default build so it is only built when another target depends on it (e.g.
    # nscp_mongoose, which is only needed by WEBServer). When excluded, the
    # install rule is made OPTIONAL so `make install` doesn't fail if the
    # library was never built.
    set(_NSCP_LIB_EXCLUDE "")
    set(_NSCP_INSTALL_OPTIONAL "")
    foreach(_NSCP_LIB_ARG ${ARGN})
        if(_NSCP_LIB_ARG STREQUAL "EXCLUDE_FROM_ALL")
            set(_NSCP_LIB_EXCLUDE EXCLUDE_FROM_ALL)
            set(_NSCP_INSTALL_OPTIONAL OPTIONAL)
        endif()
    endforeach()
    if(USE_STATIC_RUNTIME)
        add_library(${_TARGET} STATIC ${_NSCP_LIB_EXCLUDE} ${_SRCS})
        nscp_apply_pic(${_TARGET})
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                VERSION
                    "${NSCP_LIB_VERSION}"
        )
    else(USE_STATIC_RUNTIME)
        add_library(${_TARGET} SHARED ${_NSCP_LIB_EXCLUDE} ${_SRCS})
        SET_LIBRARY_OUT_FOLDER(${_TARGET})
        # Windows exports nothing from a DLL unless asked, and there is no
        # WINDOWS_EXPORT_ALL_SYMBOLS here on purpose: every library in this tree
        # says what is public through its own macro, from its own dll_defines.hpp
        # (NSCAPI_EXPORT, NSCP_NET_EXPORT, NSCP_CLIENT_EXPORT, NSCP_WHERE_EXPORT,
        # NSCAPI_PROTOBUF_EXPORT, and BOOST_JSON_DECL inside nscp_json).
        #
        # Exporting everything instead is not free: an exported symbol is a root
        # the linker may not discard, so /OPT:REF stops pruning anywhere in the
        # DLL and the library keeps code no caller reaches. Measured on
        # nscp_where_filter, which shipped both ways: 1.66 MB exporting
        # everything against 0.80 MB annotated.
        # These are package-PRIVATE libraries they install under NSCP_PKGLIBDIR alongside the modules, not the public
        # libdir, and ship no public ABI. So no SOVERSION/VERSION symlink chain (dead weight + a lintian remark for a
        # private lib). On Windows the VERSION property is harmless but equally unnecessary here.
        #
        # $ORIGIN RPATH: the libraries depend on each other and are co-located,
        # and DT_RUNPATH is non-transitive, so each must find its siblings.
        if(NOT WIN32)
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    INSTALL_RPATH
                        "${NSCP_RPATH_LIB}"
            )
        endif()
    endif(USE_STATIC_RUNTIME)
    set_target_properties(
        ${_TARGET}
        PROPERTIES
            FOLDER
                "libraries"
    )

    if(NOT USE_STATIC_RUNTIME)
        if(WIN32)
            install(
                TARGETS
                    ${_TARGET}
                ${_NSCP_INSTALL_OPTIONAL}
                RUNTIME
                    DESTINATION .
                LIBRARY
                    DESTINATION .
            )
        else()
            install(
                TARGETS
                    ${_TARGET}
                ${_NSCP_INSTALL_OPTIONAL}
                LIBRARY
                    DESTINATION ${LIB_TARGET_FOLDER}
            )
        endif()
        if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY
                        ${BUILD_TARGET_ROOT_PATH}
            )
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_DEBUG
                        ${BUILD_TARGET_ROOT_PATH}
            )
            set_target_properties(
                ${_TARGET}
                PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY_RELEASE
                        ${BUILD_TARGET_ROOT_PATH}
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
    NSCP_DEBUG_SYMBOLS(${_TARGET})
    if(WIN32)
        install(TARGETS ${_TARGET} RUNTIME DESTINATION .)
    else()
        # Executables (the nscp daemon in sbin, client tools in bin) load the
        # project's private shared libraries from NSCP_PKGLIBDIR. Carry an
        # $ORIGIN-relative RPATH derived from this target's install folder so it
        # resolves regardless of prefix (sbin and bin are both one level under
        # the prefix, so this is typically $ORIGIN/../lib/nsclient).
        file(
            RELATIVE_PATH
            _NSCP_EXE_TO_PKGLIB
            "${_FOLDER}"
            "${NSCP_PKGLIBDIR}"
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                INSTALL_RPATH
                    "${NSCP_RPATH_ORIGIN}/${_NSCP_EXE_TO_PKGLIB}"
        )
        install(TARGETS ${_TARGET} RUNTIME DESTINATION ${_FOLDER})
    endif()
    if(MSVC11 OR MSVC12 OR MSVC13 OR MSVC14 OR APPLE)
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_ROOT_PATH}
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
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE
                    ${BUILD_TARGET_ROOT_PATH}
        )
        set_target_properties(
            ${_TARGET}
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
                    ${BUILD_TARGET_ROOT_PATH}
        )
    endif()
    set_target_properties(
        ${_TARGET}
        PROPERTIES
            FOLDER
                "tests"
    )
    add_test(NAME ${_TARGET} COMMAND ${_TARGET})
    if(NSCP_SANITIZER_TEST_ENV)
        set_tests_properties(
            ${_TARGET}
            PROPERTIES
                ENVIRONMENT
                    "${NSCP_SANITIZER_TEST_ENV}"
        )
    endif()
endmacro()

function(NSCP_CREATE_TEST _TARGET)
    # Tests are scattered across libs/ and modules/ CMakeLists as well as
    # tests/. Honouring NSCP_BUILD_TESTS here makes every call site a no-op in
    # one place when tests are disabled (e.g. building without Google Test).
    if(NOT NSCP_BUILD_TESTS)
        return()
    endif()
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        ""
        ""
        "SOURCES;LIBRARIES;INCLUDES"
    )
    add_executable(${_TARGET} ${ARG_SOURCES})
    # Tests compile the library sources they need (command_line_parser.cpp,
    # socket_helpers.cpp, settings/helper.cpp, ...) straight into the test binary
    # so they can drive them without a core, and a declaration saying
    # __declspec(dllimport) for a symbol the same binary defines gives C4273 at
    # compile time and LNK4217 on every use. So the export macros are neutered.
    #
    # Note what this does NOT say: no test links nscp_client or nscp_net, but
    # most of them do link plugin_api, through NSCP_DEF_PLUGIN_LIB. Neutering
    # plugin_api_NOLIB there is safe only because a reference without dllimport
    # still resolves through the import library, via a thunk for a function.
    # That does not hold for data: plugin_api exports the vtable of
    # nscapi::settings_proxy, and a test that both sets this define and needs
    # that vtable from the DLL would fail with LNK2001. None does today - every
    # test that constructs one compiles proxy.cpp itself - but a new one that
    # hits it should drop the define for that target rather than work around it.
    target_compile_definitions(
        ${_TARGET}
        PRIVATE
            ${PLUGIN_API_TARGET}_NOLIB
            nscp_client_NOLIB
            nscp_net_NOLIB
    )
    if(ARG_LIBRARIES)
        target_link_libraries(${_TARGET} ${ARG_LIBRARIES})
    endif()
    if(ARG_INCLUDES)
        target_include_directories(${_TARGET} PRIVATE ${ARG_INCLUDES})
    endif()
    nscp_add_test(${_TARGET})
endfunction()

# Register a Lua acceptance test that drives the built `nscp` binary:
#   nscp unit --language lua --script <script>
# The test name should end in `_test` so `ctest -R '_test$'` (used by
# tools/sanitizers/run.sh) picks it up alongside the C++ unit tests. It runs
# under the same NSCP_SANITIZER_TEST_ENV as those tests, so leaks/UB in the
# Lua-driven code paths are caught. Requires the `nscp` target, the LUAScript
# module and the copy_scripts target, all built as part of the default build.
function(NSCP_ADD_LUA_TEST _NAME _SCRIPT)
    if(NOT NSCP_BUILD_TESTS)
        return()
    endif()
    add_test(
        NAME ${_NAME}
        COMMAND
            $<TARGET_FILE:nscp>
            unit
            --language lua
            --script ${_SCRIPT}
            --path-override scripts=${BUILD_TARGET_ROOT_PATH}/scripts
            # Keep everything a module writes inside the build tree. Without
            # this ${certificate-path} is the install location
            # (/usr/local/lib/nsclient/security), which a developer running
            # ctest cannot create: NRPEServer generates or validates a
            # certificate as it loads - even with insecure=true, because the
            # certificate setting still has its default value - so the whole
            # module refuses to load and any test that talks to it fails.
            # ${data-path} is the same story for writable per-machine state.
            --path-override certificate-path=${BUILD_TARGET_ROOT_PATH}/security
            --path-override data-path=${BUILD_TARGET_ROOT_PATH}
        # Run from the build root so ${base-path} (the external-scripts working
        # dir) makes relative script paths like "scripts/check_test.sh" resolve.
        WORKING_DIRECTORY ${BUILD_TARGET_ROOT_PATH}
    )
    if(NSCP_SANITIZER_TEST_ENV)
        set_tests_properties(
            ${_NAME}
            PROPERTIES
                ENVIRONMENT
                    "${NSCP_SANITIZER_TEST_ENV}"
        )
    endif()
endfunction()

macro(NSCP_MAKE_EXE_SBIN _TARGET _SRCS)
    NSCP_MAKE_EXE(${_TARGET} "${_SRCS}" ${SBIN_TARGET_FOLDER})
endmacro()
macro(NSCP_MAKE_EXE_BIN _TARGET _SRCS)
    NSCP_MAKE_EXE(${_TARGET} "${_SRCS}" ${BIN_TARGET_FOLDER})
endmacro()

macro(NSCP_FORCE_INCLUDE _TARGET _SRC)
    if(WIN32)
        string(
            REPLACE "/"
            "\\"
            WINSRC
            "${_SRC}"
        )
        set_target_properties(
            ${TARGET}
            PROPERTIES
                COMPILE_FLAGS
                    "/FI\"${WINSRC}\""
        )
    else(WIN32)
        set_target_properties(
            ${TARGET}
            PROPERTIES
                COMPILE_FLAGS
                    "-include \"${_SRC}\""
        )
        if(USE_STATIC_RUNTIME)
            # PIC is needed when this object is later linked into a shared
            # NSCP module. Handled separately so it composes with the
            # -include flag set above instead of overwriting COMPILE_FLAGS.
            nscp_apply_pic(${TARGET})
        endif()
    endif(WIN32)
endmacro()

# Canonical target architecture: x86, x64 or arm64.
#
# There were five copies of this decision in the tree and they did not agree.
# Most branched on CMAKE_CL_64, which only means "64-bit pointers" and so
# calls an ARM64 build x64; the rest branched on CMAKE_VS_PLATFORM_NAME, which
# is right but only exists under the Visual Studio generators, so a Ninja
# build on a native ARM64 host fell back to the same wrong answer.
#
# MSVC_<lang>_ARCHITECTURE_ID comes from compiler identification rather than
# from the generator, so it is set for Ninja and NMake too. Its spelling has
# varied over CMake releases (X86 vs x86), hence the lowercasing.
function(nscp_target_arch _out)
    set(_id "")
    if(MSVC_CXX_ARCHITECTURE_ID)
        string(TOLOWER "${MSVC_CXX_ARCHITECTURE_ID}" _id)
    elseif(MSVC_C_ARCHITECTURE_ID)
        string(TOLOWER "${MSVC_C_ARCHITECTURE_ID}" _id)
    elseif(CMAKE_VS_PLATFORM_NAME)
        string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _id)
    endif()

    if(_id STREQUAL "arm64" OR _id STREQUAL "arm64ec")
        set(${_out} arm64 PARENT_SCOPE)
    elseif(_id STREQUAL "x64" OR _id STREQUAL "amd64")
        set(${_out} x64 PARENT_SCOPE)
    elseif(_id STREQUAL "x86" OR _id STREQUAL "win32")
        set(${_out} x86 PARENT_SCOPE)
    elseif(CMAKE_CL_64)
        set(${_out} x64 PARENT_SCOPE)
    else()
        set(${_out} x86 PARENT_SCOPE)
    endif()
endfunction()

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
    # The glob below finds nothing on a current VS layout on any
    # architecture, but a wrong architecture here would be worse than an
    # empty list.
    nscp_target_arch(_VC_ARCH)
    set(_redit_folder
        "${_VS_ROOT_FOLDER}/redist/${_VC_ARCH}/Microsoft.VC${_VC_VERSION}.CRT"
    )
    file(GLOB ${_TARGET_VAR} "${_redit_folder}/*.dll")
endmacro(find_redist)

# Build a C# project with the dotnet SDK into the shared managed output folder
# (NSCP_DOTNET_OUTPUT_DIR = <build>/modules/dotnet, where the DotnetPlugins
# module looks for NSCP.Core.dll and the plugin assemblies).
#
#   NSCP_ADD_DOTNET_PROJECT(<target> <path/to/project.csproj>
#       [DEPENDS <targets>...] [PRODUCTS <files>...] [PROPERTIES <name=value>...])
#
# The intermediate (obj/) and bin/ folders are redirected into the current
# binary dir so the source tree stays clean. NSCP_DOTNET_PROTO_DIR (the protoc
# --csharp_out folder), NSCP_DOTNET_OUTPUT_DIR and the NSClient++ version are
# handed to MSBuild as NscpProtoDir / NscpCoreDir / NscpVersion, and the
# restore knobs from FindDotnet.cmake (NSCP_DOTNET_PACKAGE_SOURCE,
# NSCP_DOTNET_NO_RESTORE, NSCP_DOTNET_PROTOBUF_VERSION) as RestoreSources,
# --no-restore and GoogleProtobufVersion.
function(NSCP_ADD_DOTNET_PROJECT _TARGET _PROJECT)
    cmake_parse_arguments(
        PARSE_ARGV 2
        ARG
        ""
        ""
        "DEPENDS;PRODUCTS;PROPERTIES"
    )
    set(_props
        "-p:NscpProtoDir=${NSCP_DOTNET_PROTO_DIR}"
        "-p:NscpCoreDir=${NSCP_DOTNET_OUTPUT_DIR}"
        "-p:NscpVersion=${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUILD}"
        "-p:BaseIntermediateOutputPath=${CMAKE_CURRENT_BINARY_DIR}/obj/"
        "-p:BaseOutputPath=${CMAKE_CURRENT_BINARY_DIR}/bin/"
    )
    if(NSCP_DOTNET_PROTOBUF_VERSION)
        list(APPEND _props "-p:GoogleProtobufVersion=${NSCP_DOTNET_PROTOBUF_VERSION}")
    endif()
    if(NSCP_DOTNET_PACKAGE_SOURCE)
        list(APPEND _props "-p:RestoreSources=${NSCP_DOTNET_PACKAGE_SOURCE}")
    endif()
    set(_restore "")
    if(NSCP_DOTNET_NO_RESTORE)
        set(_restore "--no-restore")
    endif()
    foreach(_prop ${ARG_PROPERTIES})
        list(APPEND _props "-p:${_prop}")
    endforeach()
    add_custom_target(
        ${_TARGET}
        ALL
        COMMAND
            ${CMAKE_COMMAND} -E env DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
            DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 ${DOTNET_EXECUTABLE} build
            ${_PROJECT} --configuration Release --nologo --verbosity quiet
            --output ${NSCP_DOTNET_OUTPUT_DIR} ${_restore} ${_props}
        BYPRODUCTS ${ARG_PRODUCTS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Building .NET project ${_PROJECT} -> ${NSCP_DOTNET_OUTPUT_DIR}"
        VERBATIM
    )
    if(ARG_DEPENDS)
        add_dependencies(${_TARGET} ${ARG_DEPENDS})
    endif()
    set_target_properties(${_TARGET} PROPERTIES FOLDER "libraries/dotnet")
endfunction(NSCP_ADD_DOTNET_PROJECT)
