cmake_minimum_required(VERSION 3.5)

set(TARGET nscp_miniz)

project(${TARGET})

set(miniz_SOURCE
    ../../ext/miniz/miniz.c
    ../../ext/miniz/miniz.h
    ../../ext/miniz/miniz_zip.c
    ../../ext/miniz/miniz_zip.h
    ../../ext/miniz/miniz_tinfl.c
    ../../ext/miniz/miniz_tinfl.h
    ../../ext/miniz/miniz_tdef.c
    ../../ext/miniz/miniz_tdef.h)

add_library(${TARGET} STATIC ${miniz_SOURCE})

set_target_properties(${TARGET} PROPERTIES FOLDER "core")
