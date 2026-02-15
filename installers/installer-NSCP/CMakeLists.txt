cmake_minimum_required(VERSION 3.10)

set(ALIAS NSCP)
set(TARGET installer_${ALIAS})

project(${TARGET})

set(SRCS
    Product.wxs
    ../ui/ConfigureDlg.wxs
    ../ui/InvalidConfigDlg1.wxs
    ../ui/InvalidConfigDlg2.wxs
    ../ui/InvalidConfigDlg3.wxs
    ../ui/SelectConfigurationDlg.wxs
    ../ui/MonitoringToolDlg.wxs
    ../ui/WixUI_MondoNSCP.wxs
    ../common/web.wxs
    ../common/scripts.wxs
    ../common/op5_scripts.wxs
    ../common/op5_config.wxs
)
if(USE_STATIC_RUNTIME)
    set(RUNTIME "static")
else(USE_STATIC_RUNTIME)
    set(SRCS
        ${SRCS}
        RUNTIME_DLL.wxs
    )
    set(RUNTIME "dynamic")
endif(USE_STATIC_RUNTIME)

if(CHECK_NSCLIENT_MISSING)
    set(HAS_NSCLIENT "false")
else()
    set(HAS_NSCLIENT "true")
endif()
# GET_FILENAME_COMPONENT(_tmp_FILE WixUI_en-us.wxl ABSOLUTE)

set(LOCALIZATIONS en-us)

set(ARCH x86)
if(CMAKE_CL_64)
    set(ARCH x64)
endif(CMAKE_CL_64)

WIX_FIND_MERGE_MODULE(CRT_MERGE_MODULE "Microsoft_VC143_CRT")
message(STATUS "CRT_MERGE_MODULE: ${CRT_MERGE_MODULE}")
if(NOT CRT_MERGE_MODULE)
    message(WARNING "CRT_MERGE_MODULE not found: ${CRT_MERGE_MODULE}")
endif(NOT CRT_MERGE_MODULE)

if(OPENSSL_INCLUDE_DIR MATCHES ".*1\\.1\\.0.*")
    set(MSI_OPENSSL_CRYPTO libcrypto-1_1-x64.dll)
    set(MSI_OPENSSL_LIB libssl-1_1-x64.dll)
else()
    set(MSI_OPENSSL_CRYPTO libeay32.dll)
    set(MSI_OPENSSL_LIB ssleay32.dll)
endif()

set(WIX_CANDLE_FLAGS
    -dboost=true
    -darch=${ARCH}
    -dhas_client=${HAS_NSCLIENT}
    -arch
    ${ARCH}
    -dVersion.Major=${VERSION_SERIES}
    -dVersion.Minor=${VERSION_MAJOR}
    -dVersion.Revision=${VERSION_MINOR}
    -dVersion.Build=${VERSION_BUILD}
    "-dWebHelpSource=${NSCP_PROJECT_BINARY_DIR_NATIVE}\\docs\\html"
    "-dWebSource=${NSCP_PROJECT_BINARY_DIR}\\web"
    "-dScriptSource=${NSCP_PROJECT_BINARY_DIR}\\scripts"
    "-dOP5ScriptSource=${NSCP_PROJECT_BINARY_DIR}\\scripts\\op5"
    "-dOP5ConfigSource=${NSCP_PROJECT_BINARY_DIR}"
    "-dInSource=${CMAKE_CURRENT_SOURCE_DIR}"
    "-dInstallerDllPath=${NSCP_PROJECT_BINARY_DIR_NATIVE}/../installer_lib/Release"
    "-dApp.Title=NSClient++"
    "-dApp.Path=NSClient++"
    "-dApp.ShortPath=NSClient"
    "-dApp.StartMenuPath=NSClient++"
    "-dApp.ServiceName=nscp"
    "-dApp.UppgradeCode=0B36E3B7-0042-452d-B376-57E0C07ADDBB"
    -dOpenSSlCrypto=${MSI_OPENSSL_CRYPTO}
    -dOpenSSlLib=${MSI_OPENSSL_LIB}
    -dSource=${NSCP_PROJECT_BINARY_DIR}
    -dHelpers=${NSCP_PROJECT_BINARY_DIR}/installer-dlls
    -dCRTMergeModule=${CRT_MERGE_MODULE}
    -dRuntime=${RUNTIME}
    -dWixUILicenseRtf=${CMAKE_CURRENT_SOURCE_DIR}/License.rtf
)
source_group(Generated FILES ${SRCS})
# set_source_files_properties(${WIXOBJ_LIST} ${WIX_DEST} PROPERTIES GENERATED 1)

source_group(Sources FILES ${SRCS})
ADD_WIX_INSTALLER(${ALIAS} "${SRCS}" "${DEPENDENCIES}" "${LOCALIZATIONS}")
add_dependencies(
    ${TARGET}
    ${MAIN_NSCP_TARGET}
    ${ALL_MODULE_NAMES}
    ${ALL_ZIP_MODULES}
    ${ALL_TOOL_NAMES}
    ${DEPENDENCIES}
    ${ALL_CLIENT_NAMES}
    copy_files
    build_docs_html
    copy_scripts
    copy_resources
    copy_op5_config
    copy_op5_scripts
)

set_target_properties(
    ${TARGET}
    PROPERTIES
        EXCLUDE_FROM_ALL
            true
)
set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "installers"
)
