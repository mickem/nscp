if(WIN32)
    message(STATUS "Copying resources:")
    copy_single_file(icons help.ico ${CMAKE_BINARY_DIR}/res)
endif()
