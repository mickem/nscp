cmake_minimum_required(VERSION 3.10)

project(copy_web)

file(GLOB_RECURSE resources_HTML "dist/*.html")
file(GLOB_RECURSE resources_JS "dist/*.js")
file(GLOB_RECURSE resources_PNG "dist/*.png")
set(resources_ALL
    ${resources_HTML}
    ${resources_JS}
    ${resources_PNG}
)
set(ALL_RESOURCES)

foreach(file ${resources_ALL})
    get_filename_component(filename ${file} NAME)
    get_filename_component(filepath ${file} PATH)
    file(RELATIVE_PATH relpath "${CMAKE_CURRENT_SOURCE_DIR}/dist" "${filepath}")
    string(REPLACE "/" "_" alias "${relpath}")
    string(REPLACE "." "_" alias "${alias}")
    copy_single_file(ALL_RESOURCES dist/${relpath}/${filename} web/${relpath})
endforeach(
    file
    ${resources_ALL}
)

add_custom_target(copy_resources ALL DEPENDS ${ALL_RESOURCES})
set_target_properties(
    copy_resources
    PROPERTIES
        FOLDER
            "files"
)
