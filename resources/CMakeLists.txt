cmake_minimum_required(VERSION 3.5)

MACRO(copy_single_file src destDir)
	MESSAGE(STATUS " + ${src}")
	FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${src} DESTINATION ${destDir})
ENDMACRO(copy_single_file)

MESSAGE(STATUS "Copying resources:")
copy_single_file(icons help.ico ${CMAKE_BINARY_DIR}/res)
