cmake_minimum_required(VERSION 3.5)

SET(ALIAS NSCP)
SET(TARGET installer_${ALIAS})
	
PROJECT(${TARGET})

SET(SRCS
	Product.wxs

	../ui/ConfigureDlg.wxs
	../ui/InvalidConfigureationDlg.wxs
	../ui/SelectConfigurationDlg.wxs
	../ui/MonitoringToolDlg.wxs
	../ui/WixUI_MondoNSCP.wxs
	../common/web.wxs
	../common/scripts.wxs
	../common/op5_scripts.wxs
	../common/op5_config.wxs
)
IF(USE_STATIC_RUNTIME)
	SET(RUNTIME "static")
ELSE(USE_STATIC_RUNTIME)
	SET(SRCS ${SRCS} RUNTIME_DLL.wxs)
	SET(RUNTIME "dynamic")
ENDIF(USE_STATIC_RUNTIME)

#GET_FILENAME_COMPONENT(_tmp_FILE WixUI_en-us.wxl ABSOLUTE)

SET(LOCALIZATIONS
	en-us
)

SET(ARCH x86)
IF(CMAKE_CL_64)
	SET(ARCH x64)
ENDIF(CMAKE_CL_64)

SET(TMP "000${VERSION_BUILD}")
string(LENGTH ${TMP} TMP_LEN)
MATH(EXPR TMP_LEN ${TMP_LEN}-3)
string(SUBSTRING "000${VERSION_BUILD}" ${TMP_LEN} 3 VERSION_BUILD_PAD)

WIX_FIND_MERGE_MODULE(CRT_MERGE_MODULE "Microsoft_VC143_CRT")

if(OPENSSL_INCLUDE_DIR MATCHES ".*1\\.1\\.0.*")
set(MSI_OPENSSL_CRYPTO libcrypto-1_1-x64.dll)
set(MSI_OPENSSL_LIB libssl-1_1-x64.dll)
else()
set(MSI_OPENSSL_CRYPTO libeay32.dll)
set(MSI_OPENSSL_LIB ssleay32.dll)
endif()

SET(WIX_CANDLE_FLAGS -dboost=true -darch=${ARCH} -arch ${ARCH}
	-dVersion.Major=${VERSION_SERIES} -dVersion.Minor=${VERSION_MAJOR} -dVersion.Revision=${VERSION_MINOR} -dVersion.Build=${VERSION_BUILD} -dVersion.BuildPad=${VERSION_BUILD_PAD}
	"-dWebHelpSource=${NSCP_PROJECT_BINARY_DIR_NATIVE}\\docs\\html"
	"-dWebSource=..\\..\\web" 
	"-dScriptSource=..\\..\\scripts" 
	"-dOP5ScriptSource=..\\..\\scripts\\op5" 
	"-dOP5ConfigSource=${NSCP_PROJECT_BINARY_DIR}" 
	"-dInSource=${CMAKE_CURRENT_SOURCE_DIR}"
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
SOURCE_GROUP(Generated FILES ${SRCS})
#set_source_files_properties(${WIXOBJ_LIST} ${WIX_DEST} PROPERTIES GENERATED 1)
	
SOURCE_GROUP(Sources FILES ${SRCS})
ADD_WIX_INSTALLER(${ALIAS} "${SRCS}" "${DEPENDENCIES}" "${LOCALIZATIONS}")
ADD_DEPENDENCIES(${TARGET} ${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_ZIP_MODULES} ${ALL_TOOL_NAMES} ${DEPENDENCIES} ${ALL_CLIENT_NAMES} copy_files build_docs_html copy_scripts copy_resources)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installers")
