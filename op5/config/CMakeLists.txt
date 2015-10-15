cmake_minimum_required(VERSION 2.6)

PROJECT(copy_op5_config)

FILE (GLOB config_INI "*.ini")

SET(ALL_FILES)

FOREACH(file ${config_INI})
	get_filename_component(filename ${file} NAME)
	copy_single_file(ALL_FILES ${filename} ".")
ENDFOREACH(file ${config_INI})

ADD_CUSTOM_TARGET(copy_op5_config ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_op5_config PROPERTIES FOLDER "files")
