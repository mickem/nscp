cmake_minimum_required(VERSION 2.6)

SET(ALIAS Seconday_NSCP)
SET(TARGET installer_${ALIAS})
	
PROJECT(${TARGET})

SET(SRCS
	../installer-NSCP/Product.wxs

	../ui/ConfigureDlg.wxs
	../ui/InvalidConfigureationDlg.wxs
	../ui/SelectConfigurationDlg.wxs
	../ui/MonitoringToolDlg.wxs
	../ui/WixUI_MondoNSCP.wxs
	../common/web.wxs
	../common/scripts.wxs
	../common/html_help.wxs
	../common/op5_scripts.wxs
	../common/op5_config.wxs
)
IF(USE_STATIC_RUNTIME)
	SET(RUNTIME "static")
ELSE(USE_STATIC_RUNTIME)
	SET(SRCS ${SRCS} ../installer-NSCP/RUNTIME_DLL.wxs)
	SET(RUNTIME "dynamic")
ENDIF(USE_STATIC_RUNTIME)

SET(DEPENDENCIES
	installer_lib
)
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

WIX_FIND_MERGE_MODULE(CRT_MERGE_MODULE "Microsoft_VC110_CRT")
set(HAS_ATL "yes")
if(VSTUDIO_EXPRESS)
set(HAS_ATL "no")
endif()
set(MSVER "vc110")
if(MSVC13)
set(MSVER "vc130")
endif()
if(MSVC14)
set(MSVER "vc140")
endif()
SET(WIX_CANDLE_FLAGS -dboost=true -darch=${ARCH} -arch ${ARCH}
	-dVersion.Major=${VERSION_SERIES} -dVersion.Minor=${VERSION_MAJOR} -dVersion.Revision=${VERSION_MINOR} -dVersion.Build=${VERSION_BUILD} -dVersion.BuildPad=${VERSION_BUILD_PAD}
	"-dWebHelpSource=${NSCP_PROJECT_BINARY_DIR_NATIVE}\\docs\\html"
	"-dWebSource=..\\..\\web" 
	"-dScriptSource=..\\..\\scripts" 
	"-dOP5ScriptSource=..\\..\\scripts\\op5" 
	"-dOP5ConfigSource=${NSCP_PROJECT_BINARY_DIR}" 
	"-dInSource=${CMAKE_CURRENT_SOURCE_DIR}"
	"-dApp.Title=Secondary NSClient++"
	"-dApp.Path=Secondary NSClient++"
	"-dApp.ShortPath=SecNSClient"
	"-dApp.StartMenuPath=NSClient++"
	"-dApp.ServiceName=nscp2"
	"-dApp.UppgradeCode=0B36E3B7-0042-452d-B376-57E0C07ADDAA"
	-dSource=${NSCP_PROJECT_BINARY_DIR} 
	-dHelpers=${NSCP_PROJECT_BINARY_DIR}/installer-dlls
	-dBoostTag=${MSVER}-mt-1_58
	-dHasATL=${HAS_ATL}
	-dCRTMergeModule=${CRT_MERGE_MODULE}
	-dRuntime=${RUNTIME}
	-dWixUILicenseRtf=${CMAKE_CURRENT_SOURCE_DIR}/License.rtf
	)
SOURCE_GROUP(Generated FILES ${SRCS})
#set_source_files_properties(${WIXOBJ_LIST} ${WIX_DEST} PROPERTIES GENERATED 1)
	
SOURCE_GROUP(Sources FILES ${SRCS})
ADD_WIX_INSTALLER(${ALIAS} "${SRCS}" "${DEPENDENCIES}" "${LOCALIZATIONS}")
ADD_DEPENDENCIES(${TARGET} ${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} ${DEPENDENCIES} ${ALL_CLIENT_NAMES} copy_files build_docs_html copy_scripts copy_resources)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installers")
