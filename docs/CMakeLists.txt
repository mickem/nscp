cmake_minimum_required(VERSION 2.8)

MACRO(copy_single_file src destDir)
	MESSAGE(STATUS " + ${src}")
	FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${src} DESTINATION ${destDir})
ENDMACRO(copy_single_file)

MESSAGE(STATUS "Copying documentation:")
copy_single_file("Nagios Usage Guide.pdf" ${CMAKE_BINARY_DIR}/docs)
copy_single_file("NSClient++ Reference Manual.pdf" ${CMAKE_BINARY_DIR}/docs)
