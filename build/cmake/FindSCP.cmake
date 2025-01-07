# -*- cmake -*-
#
# Find the OpenSSH scp ("secure copy") or Putty pscp command.
#
# Input variables: SCP_FIND_REQUIRED - set this if configuration should fail
# without scp
#
# Output variables:
#
# SCP_FOUND - set if scp was found SCP_EXECUTABLE - path to scp or pscp
# executable SCP_BATCH_FLAG - how to put scp/pscp into batch mode

set(SCP_EXECUTABLE)
if(WINDOWS)
  find_program(SCP_EXECUTABLE NAMES pscp pscp.exe)
else(WINDOWS)
  find_program(SCP_EXECUTABLE NAMES scp scp.exe)
endif(WINDOWS)

if(SCP_EXECUTABLE)
  set(SCP_FOUND ON)
else(SCP_EXECUTABLE)
  set(SCP_FOUND OFF)
endif(SCP_EXECUTABLE)

if(SCP_FOUND)
  get_filename_component(_scp_name ${SCP_EXECUTABLE} NAME_WE)
  if(_scp_name STREQUAL scp)
    set(SCP_BATCH_FLAG -B)
  else(_scp_name STREQUAL scp)
    set(SCP_BATCH_FLAG -batch)
  endif(_scp_name STREQUAL scp)
else(SCP_FOUND)
  if(SCP_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find scp or pscp executable")
  endif(SCP_FIND_REQUIRED)
endif(SCP_FOUND)

mark_as_advanced(SCP_EXECUTABLE SCP_FOUND SCP_BATCH_FLAG)
