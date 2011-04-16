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

FILE (GLOB scripts_BAT "*.bat")
FILE (GLOB scripts_VBS "*.vbs")
FILE (GLOB scripts_PS1 "*.ps1")
FILE (GLOB scripts_LIB "lib/*.vbs")

SET(script_ALL ${scripts_BAT} ${scripts_VBS} ${scripts_PS1})

FOREACH(file ${script_ALL})
	get_filename_component(filename ${file} NAME)
	MESSAGE(STATUS " + ${filename}")
	copy_single_file(${file} scripts)
	#FILE(COPY ${file} DESTINATION ${CMAKE_BINARY_DIR}/scripts)
ENDFOREACH(file ${script_ALL})


FOREACH(file ${scripts_LIB})
	get_filename_component(filename ${file} NAME)
	MESSAGE(STATUS " + lib - ${filename}")
	copy_single_file(${file} scripts/lib)
	#FILE(COPY ${file} DESTINATION ${CMAKE_BINARY_DIR}/scripts/lib)
ENDFOREACH(file ${scripts_LIB})
