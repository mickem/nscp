cmake_minimum_required(VERSION 2.6)

copy_single_file(copy_files license.txt .)
copy_single_file(copy_files nrpe_dh_512.pem security)
copy_single_file(copy_files old-settings.map .)
copy_single_file(copy_files counters.defs .)
copy_single_file(copy_files ../changelog .)

ADD_CUSTOM_TARGET(copy_files ALL)
SET_TARGET_PROPERTIES(copy_files PROPERTIES FOLDER "files")
