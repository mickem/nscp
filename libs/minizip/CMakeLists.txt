cmake_minimum_required(VERSION 2.8)

SET(TARGET nscp_miniz)

project(${TARGET})

set(miniz_SOURCE
    ../../ext/miniz/miniz.c
    ../../ext/miniz/miniz.h
    ../../ext/miniz/miniz_zip.c
    ../../ext/miniz/miniz_zip.h
    ../../ext/miniz/miniz_tinfl.c
    ../../ext/miniz/miniz_tinfl.h
    ../../ext/miniz/miniz_tdef.c
    ../../ext/miniz/miniz_tdef.h
    )

add_library (${TARGET} STATIC ${miniz_SOURCE})

SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "core")

