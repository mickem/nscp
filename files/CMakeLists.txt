cmake_minimum_required(VERSION 2.6)

MACRO(copy_single_file src destDir)
	GET_FILENAME_COMPONENT(filename ${src} NAME)
	STRING(REPLACE "." "" TARGET ${filename})
	STRING(REPLACE "/" "" TARGET ${TARGET})
	ADD_CUSTOM_COMMAND(OUTPUT ${destDir}/${src}
		COMMAND cmake 
		ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${src} ${CMAKE_BINARY_DIR}/${destDir}/${src}
		COMMENT Copying ${src} to ${CMAKE_BINARY_DIR}/${destDir}/${src}
		)
	ADD_CUSTOM_TARGET(${TARGET} ALL DEPENDS ${CMAKE_BINARY_DIR}/${destDir}/${src})
	SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "files/files")
	INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES ${CMAKE_CURRENT_SOURCE_DIR}/${src})")
ENDMACRO(copy_single_file)

copy_single_file(license.txt .)
copy_single_file(nrpe_dh_512.pem security)
copy_single_file(old-settings.map .)
copy_single_file(counters.defs .)
copy_single_file(../changelog .)
