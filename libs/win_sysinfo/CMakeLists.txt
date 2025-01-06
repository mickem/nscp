cmake_minimum_required(VERSION 3.5)
if(WIN32)
  set(TARGET winsys_info)

  include_directories(${BOOST_INCLUDE_DIRS})
  link_directories(${BOOST_LIB_DIRS})
  # INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})

  set(SRCS ${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.cpp)

  if(WIN32)
    set(SRCS ${SRCS} ${NSCP_INCLUDEDIR}/win_sysinfo/win_sysinfo.hpp)
  endif(WIN32)

  add_definitions(${NSCP_GLOBAL_DEFINES})

  add_library(${TARGET} STATIC ${SRCS})
  nscp_debug_symbols(${TARGET})

  set_target_properties(${TARGET} PROPERTIES FOLDER "libraries")

endif(WIN32)
