# Every data source in this module reads procfs - /proc/stat, /proc/meminfo,
# /proc/<pid>/, /proc/uptime, /proc/net/dev - and Darwin has no procfs. The code
# compiles there (the paths are just strings) and then reports nothing, or worse
# reports a zero it presents as a measurement, so it is skipped rather than
# shipped broken. A macOS build wants its own data source on top of sysctl,
# host_statistics64 and libproc; the checks, filters and output builders above
# the fetch are already platform-neutral.
if(WIN32)
    set(BUILD_MODULE_SKIP_REASON "Not supported on Windows")
elseif(APPLE)
    set(BUILD_MODULE_SKIP_REASON "Not supported on macOS (reads Linux procfs)")
else()
    set(BUILD_MODULE 1)
    set(CURRENT_MODULE_NAME "CheckSystem")
endif()
