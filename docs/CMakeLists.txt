cmake_minimum_required(VERSION 2.6)

SET(ALL_FILES)
copy_single_file(copy_docs "Nagios Usage Guide.pdf" docs)
copy_single_file(copy_docs "NSClient++ Reference Manual.pdf" docs)

ADD_CUSTOM_TARGET(copy_docs ALL)
SET_TARGET_PROPERTIES(copy_docs PROPERTIES FOLDER "files")
#INSTALL(CODE "FILE(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/${destDir} TYPE EXECUTABLE FILES \"${source_file}\")")
