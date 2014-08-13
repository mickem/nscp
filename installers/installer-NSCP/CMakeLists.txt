cmake_minimum_required(VERSION 2.6)

SET(ALIAS NSCP)
SET(TARGET installer_${ALIAS})
	
PROJECT(${TARGET})

SET(SRCS
	Product.wxs

	../ui/ConfigureDlg.wxs
	../ui/InvalidConfigureationDlg.wxs
	../ui/SelectConfigurationDlg.wxs
	../ui/WixUI_MondoNSCP.wxs
	../common/web.wxs
	../common/scripts.wxs
)
#	WixUI.wixlib
SET(DEPENDENCIES
	installer_lib
)
#GET_FILENAME_COMPONENT(_tmp_FILE WixUI_en-us.wxl ABSOLUTE)

SET(LOCALIZATIONS
	en-us
)

SET(WIX_CANDLE_FLAGS -dboost=true -darch=${VERSION_ARCH} -arch ${VERSION_ARCH}
	-dVersion.Major=${VERSION_SERIES} -dVersion.Minor=${VERSION_MAJOR} -dVersion.Revision=${VERSION_MINOR} -dVersion.Build=${VERSION_BUILD} 
	"-dWebSource=..\\..\\web" "-dScriptSource=..\\..\\scripts" -dSource=${NSCP_PROJECT_BINARY_DIR} -dHelpers=${NSCP_PROJECT_BINARY_DIR}/installer-dlls
	)
SOURCE_GROUP(Generated FILES ${SRCS})
#set_source_files_properties(${WIXOBJ_LIST} ${WIX_DEST} PROPERTIES GENERATED 1)
	
SOURCE_GROUP(Sources FILES ${SRCS})
ADD_WIX_INSTALLER(${ALIAS} "${SRCS}" "${DEPENDENCIES}" "${LOCALIZATIONS}")
ADD_DEPENDENCIES(${TARGET} ${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} ${DEPENDENCIES} ${ALL_CLIENT_NAMES} copy_files build_docs_pdf copy_scripts copy_resources)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installers")
