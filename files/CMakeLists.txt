cmake_minimum_required(VERSION 2.6)
PROJECT(copy_files)

SET(ALL_FILES)
copy_single_file(ALL_FILES license.txt .)
copy_single_file(ALL_FILES nrpe_dh_512.pem security)
copy_single_file(ALL_FILES old-settings.map .)
copy_single_file(ALL_FILES nsclient.dist .)
copy_single_file(ALL_FILES ../changelog .)

IF(NOT WIN32)
	CONFIGURE_FILE(
	  unix-nsclient.conf.in
	  ${PROJECT_BINARY_DIR}/unix-nsclient.conf
	  @ONLY
	)
	INSTALL(FILES ${PROJECT_BINARY_DIR}/unix-nsclient.conf 
		DESTINATION /etc/nsclient
		RENAME nsclient.ini)
	file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/log)
	install(DIRECTORY "nsclient" DESTINATION "/var/log")
ENDIF()
IF(SYSTEMD)
	INSTALL(FILES nscp.service
		DESTINATION /lib/systemd/system)
ENDIF()

ADD_CUSTOM_TARGET(copy_files ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_files PROPERTIES FOLDER "files")
