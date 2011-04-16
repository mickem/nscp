cmake_minimum_required(VERSION 2.6)

MACRO(copy_single_file src destDir)
	GET_FILENAME_COMPONENT(TARGET ${src} NAME)
	SET(target_file ${CMAKE_BINARY_DIR}/${destDir}/${TARGET})
	ADD_CUSTOM_COMMAND(OUTPUT ${target_file}
		DEPENDS ${src}
		COMMAND cmake 
		ARGS -E copy ${src} ${target_file}
		COMMENT Copying ${src} to ${target_file}
		)
	#ADD_CUSTOM_TARGET(${TARGET} ALL DEPENDS ${src})
	#SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "files/scripts")
	INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES ${src})")
ENDMACRO(copy_single_file)

copy_single_file("Nagios Usage Guide.pdf" docs)
copy_single_file("NSClient++ Reference Manual.pdf" docs)
