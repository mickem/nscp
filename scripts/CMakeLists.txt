cmake_minimum_required(VERSION 2.6)

FILE (GLOB scripts_BAT "*.bat")
FILE (GLOB scripts_VBS "*.vbs")
FILE (GLOB scripts_PS1 "*.ps1")
FILE (GLOB scripts_LIB "lib/*.vbs")
#IF(WIN32)
FILE (GLOB_RECURSE scripts_PYT "python/*.py")
#ELSE(WIN32)
#FILE (GLOB scripts_PYT "python/*.py" "python/lib/*.py")
#ENDIF(WIN32)
FILE (GLOB scripts_LUA "lua/*.lua")

SET(script_ALL ${scripts_BAT} ${scripts_VBS} ${scripts_PS1})
SET(ALL_FILES)



FOREACH(file ${script_ALL})
	get_filename_component(filename ${file} NAME)
	#MESSAGE(STATUS " + ${filename} (${file})")
	copy_single_file(${filename} scripts)
ENDFOREACH(file ${script_ALL})


FOREACH(file ${scripts_LIB})
	get_filename_component(filename ${file} NAME)
	#MESSAGE(STATUS " + lib - ${filename}")
	copy_single_file(lib/${filename} scripts/lib)
ENDFOREACH(file ${scripts_LIB})

FOREACH(file ${scripts_PYT})
	get_filename_component(filename ${file} NAME)
	get_filename_component(filepath ${file} PATH)
	file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
	STRING(REPLACE "/" "_" alias "${relpath}")
	STRING(REPLACE "." "_" alias "${alias}")
	#MESSAGE(STATUS " + python - ${filename} - ${relpath} - ${alias}")
	copy_single_file(${relpath}/${filename} scripts/${relpath})
ENDFOREACH(file ${scripts_PYT})

FOREACH(file ${scripts_LUA})
	get_filename_component(filename ${file} NAME)
	#MESSAGE(STATUS " + lua - ${filename}")
	copy_single_file(lua/${filename} scripts/lua)
ENDFOREACH(file ${scripts_LUA})

ADD_CUSTOM_TARGET(copy_scripts ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_scripts PROPERTIES FOLDER "files")
#INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES \"${source_file}\")")
