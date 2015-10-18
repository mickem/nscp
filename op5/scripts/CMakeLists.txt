cmake_minimum_required(VERSION 2.6)

PROJECT(copy_op5_scripts)

IF(WIN32)
	FILE (GLOB scripts_op5_BAT "*.bat")
	FILE (GLOB scripts_op5_VBS "*.vbs")
	FILE (GLOB scripts_op5_PS1 "*.ps1")
ELSE(WIN32)
	FILE (GLOB scripts_op5_SH "*.sh")
ENDIF(WIN32)
FILE (GLOB_RECURSE scripts_op5_PYT "python/*.py")
FILE (GLOB_RECURSE scripts_op5_LUA "lua/*.lua")

SET(script_op5_ALL ${scripts_op5_BAT} ${scripts_op5_VBS} ${scripts_op5_PS1} ${scripts_op5_SH})
SET(ALL_FILES)

FOREACH(file ${script_op5_ALL})
	get_filename_component(filename ${file} NAME)
	copy_single_file_755(ALL_FILES ${filename} scripts/op5)
ENDFOREACH(file ${script_op5_ALL})


FOREACH(file ${scripts_op5_PYT})
	get_filename_component(filename ${file} NAME)
	get_filename_component(filepath ${file} PATH)
	file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
	STRING(REPLACE "/" "_" alias "${relpath}")
	STRING(REPLACE "." "_" alias "${alias}")
	copy_single_file(ALL_FILES ${relpath}/${filename} scripts/op5/${relpath})
ENDFOREACH(file ${scripts_op5_PYT})

FOREACH(file ${scripts_op5_LUA})
	get_filename_component(filename ${file} NAME)
	get_filename_component(filepath ${file} PATH)
	file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
	STRING(REPLACE "/" "_" alias "${relpath}")
	STRING(REPLACE "." "_" alias "${alias}")
	copy_single_file(ALL_FILES ${relpath}/${filename} scripts/op5/${relpath})
ENDFOREACH(file ${scripts_op5_LUA})

ADD_CUSTOM_TARGET(copy_op5_scripts ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_op5_scripts PROPERTIES FOLDER "files")
