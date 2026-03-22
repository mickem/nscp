# * Find miniz zip library
#
# MINIZ_FOUND              - have cpp-netlib been found MINIZ_INCLUDE_DIR - path
# to where miniz is found
find_path(
    MINIZ_INCLUDE_DIR
    NAMES
        miniz.c
    PATHS
        ${MINIZ_INCLUDE_DIR}
)
if(MINIZ_INCLUDE_DIR)
    set(MINIZ_FOUND TRUE)
else(MINIZ_INCLUDE_DIR)
    set(MINIZ_FOUND FALSE)
endif(MINIZ_INCLUDE_DIR)
mark_as_advanced(MINIZ_INCLUDE_DIR)
