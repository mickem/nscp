cmake_minimum_required(VERSION 2.6)

SET(ALL_FILES)
copy_single_file(ALL_FILES license.txt .)
copy_single_file(ALL_FILES nrpe_dh_512.pem security)
copy_single_file(ALL_FILES old-settings.map .)
copy_single_file(ALL_FILES counters.defs .)
copy_single_file(ALL_FILES nsclient.dist .)
copy_single_file(ALL_FILES ../changelog .)

ADD_CUSTOM_TARGET(copy_files DEPENDS ${ALL_FILES})
SET_TARGET_PROPERTIES(copy_files PROPERTIES FOLDER "files")
