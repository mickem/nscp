# PythonScript embeds CPython via Boost.Python, so it needs BOTH the Python
# development libraries (Python3_Development) and the matching Boost.Python
# component. Either one missing disables the module rather than failing the
# build. Boost.Python is requested as an optional component in
# dependencies.cmake, so its presence is reported via Boost_<version>_FOUND.
string(TOUPPER "${NSCP_BOOST_PYTHON_VERSION}" _nscp_boost_python_upper)
if(Python3_Development_FOUND AND Boost_${_nscp_boost_python_upper}_FOUND)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE_SKIP_REASON
        "Python3 development libraries or Boost.Python not found"
    )
endif()
