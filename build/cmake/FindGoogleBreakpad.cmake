# -*- cmake -*-

# * Find Google BreakPad Find the Google BreakPad includes and library This
#   module defines BREAKPAD_EXCEPTION_HANDLER_INCLUDE_DIR, where to find
#   exception_handler.h, etc. BREAKPAD_EXCEPTION_HANDLER_LIBRARIES, the
#   libraries needed to use Google BreakPad. BREAKPAD_EXCEPTION_HANDLER_FOUND,
#   If false, do not try to use Google BreakPad. also defined, but not for
#   general use are BREAKPAD_EXCEPTION_HANDLER_LIBRARY, where to find the Google
#   BreakPad library.

find_path(BREAKPAD_INCLUDE_DIR google_breakpad/common/breakpad_types.h
          PATHS ${BREAKPAD_ROOT}/src
                ${CMAKE_SOURCE_DIR}/ext/google-breakpad/src)

if(NOT GoogleBreakpad_FIND_COMPONENTS)
  set(GoogleBreakpad_FIND_COMPONENTS breakpad_common breakpad)
endif(NOT GoogleBreakpad_FIND_COMPONENTS)

if(CMAKE_TRACE)
  message(STATUS "BREAKPAD_ROOT=${BREAKPAD_ROOT}")
  message(STATUS "BREAKPAD_INCLUDE_DIR=${BREAKPAD_INCLUDE_DIR}")
endif(CMAKE_TRACE)

if(BREAKPAD_INCLUDE_DIR)
  set(BREAKPAD_FOUND TRUE)
  message(
    STATUS "Looking for ${GoogleBreakpad_FIND_COMPONENTS} (${BREAKPAD_FOUND})")
  foreach(COMPONENT ${GoogleBreakpad_FIND_COMPONENTS})
    string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
    if(CMAKE_TRACE)
      message(STATUS " + Looking for: ${COMPONENT}")
      message(STATUS "    BREAKPAD_ROOT=${BREAKPAD_ROOT}")
      message(STATUS "    BREAKPAD_INCLUDE_DIR=${BREAKPAD_INCLUDE_DIR}")
    endif(CMAKE_TRACE)

    find_library(
      ${UPPERCOMPONENT}_LIBRARY_RELEASE
      NAMES ${COMPONENT}
      PATHS ${BREAKPAD_ROOT}/src/client/windows/Release/lib
            ${BREAKPAD_ROOT}/Release
            ${BREAKPAD_INCLUDE_DIR}/src/client/windows/Release/lib)

    find_library(
      ${UPPERCOMPONENT}_LIBRARY_DEBUG
      NAMES ${COMPONENT}
      PATHS ${BREAKPAD_ROOT}/src/client/windows/Debug/lib
            ${BREAKPAD_ROOT}/Debug
            ${BREAKPAD_INCLUDE_DIR}/src/client/windows/Debug/lib)
    if(${UPPERCOMPONENT}_LIBRARY_RELEASE AND ${UPPERCOMPONENT}_LIBRARY_DEBUG)
      set(${UPPERCOMPONENT}_FOUND TRUE)
      set(${UPPERCOMPONENT}_LIBRARY
          optimized ${${UPPERCOMPONENT}_LIBRARY_RELEASE} debug
          ${${UPPERCOMPONENT}_LIBRARY_DEBUG})
      set(${UPPERCOMPONENT}_LIBRARY
          ${${UPPERCOMPONENT}_LIBRARY}
          CACHE FILEPATH "The breakpad ${UPPERCOMPONENT} library")
    else(${UPPERCOMPONENT}_LIBRARY_RELEASE AND ${UPPERCOMPONENT}_LIBRARY_DEBUG)
      set(BREAKPAD_FOUND FALSE)
      set(${UPPERCOMPONENT}_FOUND FALSE)
      set(${UPPERCOMPONENT}_LIBRARY
          "${${UPPERCOMPONENT}_LIBRARY_RELEASE-NOTFOUND}")
    endif(${UPPERCOMPONENT}_LIBRARY_RELEASE AND ${UPPERCOMPONENT}_LIBRARY_DEBUG)
    if(CMAKE_TRACE)
      message(STATUS "    Found for ${UPPERCOMPONENT} ${BREAKPAD_FOUND}")
      message(
        STATUS
          "    ${UPPERCOMPONENT}_LIBRARY_RELEASE=${${UPPERCOMPONENT}_LIBRARY_RELEASE}"
      )
    endif(CMAKE_TRACE)
  endforeach(COMPONENT)
endif(BREAKPAD_INCLUDE_DIR)
if(BREAKPAD_FOUND)
  if(CMAKE_TRACE)
    message(
      STATUS
        "Looking for dump-symbols in: ${BREAKPAD_INCLUDE_DIR}/tools/windows/binaries"
    )
  endif(CMAKE_TRACE)
  message(
    STATUS "Looking for BREAKPAD_DUMPSYMS_EXE in ${BREAKPAD_ROOT}/Release")
  find_program(
    BREAKPAD_DUMPSYMS_EXE dump_syms.exe
    NAMES dump_syms dumpsyms
    PATHS ENV PATH ${BREAKPAD_ROOT}/Release
          ${BREAKPAD_ROOT}/tools/windows/binaries
          ${BREAKPAD_INCLUDE_DIR}/tools/windows/binaries)
  message(STATUS "Found BREAKPAD_DUMPSYMS_EXE=${BREAKPAD_DUMPSYMS_EXE}")
  if(CMAKE_TRACE)
    message(STATUS "Found BREAKPAD_DUMPSYMS_EXE=${BREAKPAD_DUMPSYMS_EXE}")
  endif(CMAKE_TRACE)
  if(BREAKPAD_DUMPSYMS_EXE)
    set(BREAKPAD_DUMPSYMS_EXE_FOUND TRUE)
  else(BREAKPAD_DUMPSYMS_EXE)
    set(BREAKPAD_DUMPSYMS_EXE_FOUND FALSE)
    set(BREAKPAD_FOUND FALSE)
  endif(BREAKPAD_DUMPSYMS_EXE)
endif(BREAKPAD_FOUND)
