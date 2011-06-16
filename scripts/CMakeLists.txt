cmake_minimum_required(VERSION 2.6)

FILE (GLOB scripts_BAT "*.bat")
FILE (GLOB scripts_VBS "*.vbs")
FILE (GLOB scripts_PS1 "*.ps1")
FILE (GLOB scripts_LIB "lib/*.vbs")

SET(script_ALL ${scripts_BAT} ${scripts_VBS} ${scripts_PS1})

FOREACH(file ${script_ALL})
	get_filename_component(filename ${file} NAME)
	#MESSAGE(STATUS " + ${filename} (${file})")
	copy_single_file(scripts ${filename} scripts)
ENDFOREACH(file ${script_ALL})


FOREACH(file ${scripts_LIB})
	get_filename_component(filename ${file} NAME)
	#MESSAGE(STATUS " + lib - ${filename}")
	copy_single_file("script lib" lib/${filename} scripts/lib)
ENDFOREACH(file ${scripts_LIB})
