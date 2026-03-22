cmake_minimum_required(VERSION 3.10)

set(TARGET nscp_miniz)

project(${TARGET})

set(miniz_SOURCE
    ${MINIZ_INCLUDE_DIR}/miniz.c
    ${MINIZ_INCLUDE_DIR}/miniz.h
)

add_library(${TARGET} STATIC ${miniz_SOURCE})

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "core"
)
