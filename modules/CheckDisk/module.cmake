# The unix data sources (check_drive_unix.cpp, check_disk_io_unix.cpp,
# check_mount.cpp) enumerate mounts through <mntent.h> and read /proc/diskstats.
# Neither exists on Darwin, which spells the same thing getmntinfo(3) and
# IOKit - so the module does not compile there yet. The checks themselves are
# platform-neutral; it is only the fetch that has to be written.
if(APPLE)
    set(BUILD_MODULE_SKIP_REASON "Not supported on macOS (mntent/diskstats)")
else()
    set(BUILD_MODULE 1)
endif()
