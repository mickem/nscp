cmake_minimum_required(VERSION 2.6)

MACRO(copy_single_file src destDir)
	MESSAGE(STATUS " + ${src}")
	FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${src} DESTINATION ${destDir})
ENDMACRO(copy_single_file)

MESSAGE(STATUS "Copying resources:")
copy_single_file(help.ico ${CMAKE_BINARY_DIR}/res)
