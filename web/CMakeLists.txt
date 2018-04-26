cmake_minimum_required(VERSION 2.6)

PROJECT(copy_scripts)

FILE (GLOB_RECURSE resources_CSS "dist/*.css")
FILE (GLOB_RECURSE resources_JS "dist/*.js")
FILE (GLOB_RECURSE resources_HTML "dist/*.html")
FILE (GLOB_RECURSE resources_PNG "dist/*.png")
FILE (GLOB_RECURSE resources_GIF "dist/*.gif")
FILE (GLOB_RECURSE resources_F1 "dist/*.eot")
FILE (GLOB_RECURSE resources_F2 "dist/*.svg")
FILE (GLOB_RECURSE resources_F3 "dist/*.ttf")
FILE (GLOB_RECURSE resources_F4 "dist/*.woff")
SET(resources_FNT ${resources_F1} ${resources_F2} ${resources_F3} ${resources_F4})
SET(resources_ALL ${resources_HTML} ${resources_JS} ${resources_CSS} ${resources_PNG} ${resources_GIF} ${resources_FNT})
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
 

