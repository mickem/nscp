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
	nscp
	main_dll
	
	check_60s.bat_scripts
	check_battery.vbs_scripts
	check_files.vbs_scripts
	check_long.bat_scripts
	check_no_rdp.bat_scripts
	check_ok.bat_scripts
	check_ping.bat_scripts
	check_printer.vbs_scripts
	check_test.bat_scripts
	check_test.ps1_scripts
	check_test.vbs_scripts
	check_updates.vbs_scripts
	powershell.ps1_scripts
	
	"NagiosPlugins.vbs_script lib"
	"wrapper.vbs_script lib"
	
	counters.defs_files
	license.txt_files
	nrpe_dh_512.pem_files
	old-settings.map_files
	changelog_files
	
	"Nagios Usage Guide.pdf_docs"
	"NSClient++ Reference Manual.pdf_docs"
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

ADD_DEPENDENCIES(${TARGET} ${MAIN_NSCP_TARGET} ${ALL_MODULE_NAMES} ${ALL_TOOL_NAMES} ${ALL_INSTALLERS_DLLS_NAMES})

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "installers")
