if(LUA_SOURCE_FOUND OR LUA_FOUND)
    set(BUILD_MODULE 1)
else()
    message(STATUS "Disabling CheckMK since Lua was not found")
endif()

