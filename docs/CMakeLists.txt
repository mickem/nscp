cmake_minimum_required(VERSION 2.6)

PROJECT(copy_docs)

SET(ALL_FILES)
copy_single_file(ALL_FILES "Nagios Usage Guide.pdf" docs)
copy_single_file(ALL_FILES "NSClient++ Reference Manual.pdf" docs)

ADD_CUSTOM_TARGET(copy_docs ALL DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_docs PROPERTIES FOLDER "files")
#INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES \"${source_file}\")")
