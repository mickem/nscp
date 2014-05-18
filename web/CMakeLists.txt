cmake_minimum_required(VERSION 2.6)

PROJECT(copy_scripts)

FILE (GLOB_RECURSE resources_CSS "*.css")
FILE (GLOB_RECURSE resources_JS "*.js")
FILE (GLOB_RECURSE resources_HTML "*.html")

SET(resources_ALL ${resources_HTML} ${resources_JS} ${resources_CSS})
SET(ALL_RESOURCES)

FOREACH(file ${resources_ALL})
	get_filename_component(filename ${file} NAME)
	get_filename_component(filepath ${file} PATH)
	file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
	STRING(REPLACE "/" "_" alias "${relpath}")
	STRING(REPLACE "." "_" alias "${alias}")
	copy_single_file(ALL_RESOURCES ${relpath}/${filename} web/${relpath})
ENDFOREACH(file ${resources_ALL})

ADD_CUSTOM_TARGET(copy_resources ALL DEPENDS ${ALL_RESOURCES})
SET_TARGET_PROPERTIES(copy_resources PROPERTIES FOLDER "files")
