cmake_minimum_required(VERSION 3.10)

set(TARGET icinga-client)

project(${TARGET})

CREATE_ZIP_MODULE(${TARGET} ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_target(${TARGET} ALL DEPENDS ${BUILD_TARGET_LIB_PATH}/${TARGET}.zip)
