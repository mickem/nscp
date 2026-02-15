cmake_minimum_required(VERSION 3.10)

project(copy_op5_scripts)

if(WIN32)
    file(GLOB scripts_op5_BAT "*.bat")
    file(GLOB scripts_op5_VBS "*.vbs")
    file(GLOB scripts_op5_PS1 "*.ps1")
else(WIN32)
    file(GLOB scripts_op5_SH "*.sh")
endif(WIN32)
file(GLOB_RECURSE scripts_op5_PYT "python/*.py")
file(GLOB_RECURSE scripts_op5_LUA "lua/*.lua")

set(script_op5_ALL
    ${scripts_op5_BAT}
    ${scripts_op5_VBS}
    ${scripts_op5_PS1}
    ${scripts_op5_SH}
)
set(ALL_FILES)

foreach(file ${script_op5_ALL})
    get_filename_component(filename ${file} NAME)
    copy_single_file_755(ALL_FILES ${filename} scripts/op5)
endforeach(
    file
    ${script_op5_ALL}
)

foreach(file ${scripts_op5_PYT})
    get_filename_component(filename ${file} NAME)
    get_filename_component(filepath ${file} PATH)
    file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
    string(REPLACE "/" "_" alias "${relpath}")
    string(REPLACE "." "_" alias "${alias}")
    copy_single_file(ALL_FILES ${relpath}/${filename} scripts/op5/${relpath})
endforeach(
    file
    ${scripts_op5_PYT}
)

foreach(file ${scripts_op5_LUA})
    get_filename_component(filename ${file} NAME)
    get_filename_component(filepath ${file} PATH)
    file(RELATIVE_PATH relpath ${CMAKE_CURRENT_SOURCE_DIR} "${filepath}")
    string(REPLACE "/" "_" alias "${relpath}")
    string(REPLACE "." "_" alias "${alias}")
    copy_single_file(ALL_FILES ${relpath}/${filename} scripts/op5/${relpath})
endforeach(
    file
    ${scripts_op5_LUA}
)

add_custom_target(copy_op5_scripts ALL DEPENDS ${ALL_FILES})
set_target_properties(
    copy_op5_scripts
    PROPERTIES
        FOLDER
            "files"
)
