# A check reuses one HTTP client for all its requests (a paged list is several),
# and reconnecting a TLS client needs asio's ssl::stream move assignment, which
# arrived in Boost 1.77. On older Boost (RHEL 9 ships 1.75) the module is not
# built rather than failing on its second request.
if(Boost_VERSION_STRING VERSION_LESS 1.77)
    set(BUILD_MODULE_SKIP_REASON
        "Needs Boost 1.77 or later (found ${Boost_VERSION_STRING})"
    )
else()
    set(BUILD_MODULE 1)
endif()
