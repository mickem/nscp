cmake_minimum_required(VERSION 2.6)

SET(ALL_FILES)
copy_single_file(license.txt .)
copy_single_file(nrpe_dh_512.pem security)
copy_single_file(old-settings.map .)
copy_single_file(counters.defs .)
copy_single_file(../changelog .)

ADD_CUSTOM_TARGET(copy_files ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_files PROPERTIES FOLDER "files")
#INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES \"${source_file}\")")
