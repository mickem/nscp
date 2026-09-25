# The unit test covers check_hyperv_internal.hpp and hyperv_facts.cpp, which
# are platform-neutral: the record building, the state decoding and the facts
# records carry no WMI or PDH. So it is registered on every platform - from
# CMakeLists.txt with the module on Windows, and from module.cmake everywhere
# else, where the module itself is not built - which puts it in front of the
# Linux and sanitizer jobs too. Included, not add_subdirectory'd, so paths are
# relative to this file.
NSCP_CREATE_TEST(
    check_hyperv_test
    SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/check_hyperv_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/hyperv_facts_test.cpp
        ${CMAKE_CURRENT_LIST_DIR}/hyperv_facts.cpp
        ${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
        ${NSCP_INCLUDEDIR}/str/utf8.cpp
    LIBRARIES
        GTest::gtest_main
        nscp_protobuf
        ${PLUGIN_API_TARGET}
        ${Boost_FILESYSTEM_LIBRARY}
        ${Boost_THREAD_LIBRARY}
        ${Boost_DATE_TIME_LIBRARY}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_REGEX_LIBRARY}
    INCLUDES
        ${NSCP_INCLUDEDIR}
        ${CMAKE_CURRENT_LIST_DIR}
)
