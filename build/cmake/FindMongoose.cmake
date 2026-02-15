# * Find mongoose source/include folder This module finds mongoose if it is
#   installed and determines where the files are. This code sets the following
#   variables:
#
# MONGOOSE_FOUND       - have mongoose been found
# MONGOOSE_INCLUDE_DIR - The directory where mongoose.h is found
find_path(
    MONGOOSE_INCLUDE_DIR
    NAMES
        mongoose.h
    PATHS
        ${MONGOOSE_SOURCE_DIR}
        ${MONGOOSE_INCLUDE_DIR}
        ${NSCP_INCLUDEDIR}
)

if(MONGOOSE_INCLUDE_DIR)
    set(MONGOOSE_FOUND TRUE)
    set(MONGOOSE_LIBRARY _mongoose)
else(MONGOOSE_INCLUDE_DIR)
    set(MONGOOSE_FOUND FALSE)
    set(MONGOOSE_LIBRARY)
endif(MONGOOSE_INCLUDE_DIR)
mark_as_advanced(
    MONGOOSE_INCLUDE_DIR
    MONGOOSE_LIBRARY
)
