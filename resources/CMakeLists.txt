cmake_minimum_required(VERSION 3.10)

macro(copy_single_file src destDir)
    message(STATUS " + ${src}")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${src} DESTINATION ${destDir})
endmacro(copy_single_file)

message(STATUS "Copying resources:")
copy_single_file(icons help.ico ${CMAKE_BINARY_DIR}/res)
