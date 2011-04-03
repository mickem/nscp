cmake_minimum_required(VERSION 2.8)

MACRO(copy_single_file src destDir)
	MESSAGE(STATUS " + ${src}")
	FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${src} DESTINATION ${destDir})
ENDMACRO(copy_single_file)

MESSAGE(STATUS "Copying files:")
copy_single_file(license.txt ${CMAKE_BINARY_DIR})
copy_single_file(nrpe_dh_512.pem ${CMAKE_BINARY_DIR}/security)
copy_single_file(old-settings.map ${CMAKE_BINARY_DIR})
copy_single_file(counters.defs ${CMAKE_BINARY_DIR})
copy_single_file(../changelog ${CMAKE_BINARY_DIR})



