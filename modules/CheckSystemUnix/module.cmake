# Built on Linux and macOS. The checks, filters and output builders are
# platform-neutral; each data source has a `_linux` reader (procfs, sysfs,
# systemctl) and a `_darwin` one (sysctl, Mach, libproc, IOKit, launchctl),
# picked in CMakeLists.txt.
if(WIN32)
    set(BUILD_MODULE_SKIP_REASON "Not supported on Windows")
else()
    set(BUILD_MODULE 1)
    set(CURRENT_MODULE_NAME "CheckSystem")
endif()
