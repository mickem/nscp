# A diagnostic module, not a monitoring one: `crash_client` dereferences a null
# pointer on purpose, so a release artifact that ships it carries a
# one-request denial of service for anyone who ends up with an over-broad
# command grant or a misconfigured allow-list. It is not loaded by default and
# its own module.json warns that it is dangerous, but shipping a remote-crash
# primitive in signed packages is an unnecessary footgun.
#
# Off by default, so no package built from a plain `cmake ..` contains it.
# Build it deliberately when you need it:
#
#   cmake -DBUILD_TESTING_MODULES=ON ..
option(
    BUILD_TESTING_MODULES
    "Build diagnostic modules that deliberately misbehave (CauseCrashes)"
    OFF
)
if(BUILD_TESTING_MODULES)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE 0)
    set(BUILD_MODULE_SKIP_REASON
        "Diagnostic module, excluded from packages; enable with -DBUILD_TESTING_MODULES=ON"
    )
endif()
