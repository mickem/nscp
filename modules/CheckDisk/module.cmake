# The Linux data sources (check_drive_linux.cpp, check_disk_io_linux.cpp,
# check_mount.cpp) enumerate mounts through <mntent.h> and read /proc/diskstats.
# Neither exists on Darwin, which spells the same thing getmntinfo(3) and
# IOKit - so the module has no data source there yet. The checks themselves are
# platform-neutral; it is only the fetch that has to be written, as
# check_drive_darwin.cpp / check_disk_io_darwin.cpp in the APPLE branch of
# CMakeLists.txt.
if(APPLE)
    set(BUILD_MODULE_SKIP_REASON "Not supported on macOS (mntent/diskstats)")
else()
    set(BUILD_MODULE 1)
endif()
