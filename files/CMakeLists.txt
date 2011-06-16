cmake_minimum_required(VERSION 2.6)

copy_single_file(files license.txt .)
copy_single_file(files nrpe_dh_512.pem security)
copy_single_file(files old-settings.map .)
copy_single_file(files counters.defs .)
copy_single_file(files ../changelog .)
