cmake_minimum_required(VERSION 2.6)

SET(ALIAS NSCP)
SET(TARGET ${ALIAS}_installer)
	
PROJECT(${TARGET})

SET(SRCS
	Product.wxs

	../ui/ConfigureDlg.wxs
	../ui/InvalidConfigureationDlg.wxs
	../ui/SelectConfigurationDlg.wxs
	../ui/WixUI_MondoNSCP.wxs
)
#	WixUI.wixlib
SET(DEPENDENCIES
	main_dll
)
#GET_FILENAME_COMPONENT(_tmp_FILE WixUI_en-us.wxl ABSOLUTE)

SET(LOCALIZATIONS
	en-us
)

SET(WIX_CANDLE_FLAGS -dboost=true -darch=${VERSION_ARCH} 
	-dVersion.Major=${VERSION_SERIES} -dVersion.Minor=${VERSION_MAJOR} -dVersion.Revision=${VERSION_MINOR} -dVersion.Build=${VERSION_BUILD} 
	-dSource=${NSCP_PROJECT_BINARY_DIR} -dHelpers=${NSCP_PROJECT_BINARY_DIR}/installer-dlls
	)
SOURCE_GROUP(Generated FILES ${SRCS})
#set_source_files_properties(${WIXOBJ_LIST} ${WIX_DEST} PROPERTIES GENERATED 1)
	
SOURCE_GROUP(Sources FILES ${SRCS})
ADD_WIX_INSTALLER(${ALIAS} "${SRCS}" "${DEPENDENCIES}" "${LOCALIZATIONS}")
ADD_DEPENDENCIES(${TARGET} ${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} ${ALL_INSTALLERS_DLLS_NAMES} copy_files copy_docs copy_scripts)

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installers")
