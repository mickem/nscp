cmake_minimum_required(VERSION 3.10)

# Boost.JSON, compiled exactly once.
#
# `include/json/use_json.cpp` is a single `#include <boost/json/src.hpp>`, i.e.
# the whole of Boost.JSON. Ten modules, the onboarding library and the service
# each compiled their own copy of it (~660 KB of object code apiece). Boost
# supports separate compilation of the library for precisely this reason: the
# one translation unit that includes `src.hpp` defines BOOST_JSON_SOURCE, which
# turns BOOST_JSON_DECL into an export, and every consumer that sees
# BOOST_JSON_DYN_LINK without BOOST_JSON_SOURCE gets the matching import.
#
# The define is PUBLIC so it reaches consumers through the link: a consumer
# that gets the declarations without it would try to resolve the definitions
# locally and fail to link.

set(TARGET nscp_json)

include_directories(${BOOST_INCLUDE_DIRS})
link_directories(${BOOST_LIB_DIRS})
add_definitions(${NSCP_GLOBAL_DEFINES})

set(SRCS ${NSCP_INCLUDEDIR}/json/use_json.cpp)

NSCP_MAKE_LIBRARY(${TARGET} "${SRCS}")

if(NOT USE_STATIC_RUNTIME)
    # The static-runtime (XP) build links everything into each binary, so
    # Boost.JSON stays header-only there and must not be annotated at all.
    target_compile_definitions(${TARGET} PUBLIC BOOST_JSON_DYN_LINK)
endif()

set_target_properties(
    ${TARGET}
    PROPERTIES
        FOLDER
            "libraries"
)
