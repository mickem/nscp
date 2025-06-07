cmake_minimum_required(VERSION 3.10)

project(copy_op5_config)

file(GLOB config_INI "*.ini")

set(ALL_FILES)

foreach(file ${config_INI})
  get_filename_component(filename ${file} NAME)
  copy_single_file(ALL_FILES ${filename} ".")
endforeach(file ${config_INI})

add_custom_target(copy_op5_config ALL DEPENDS ${ALL_FILES})
set_target_properties(copy_op5_config PROPERTIES FOLDER "files")
