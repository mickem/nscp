cmake_minimum_required(VERSION 2.8)

FILE (GLOB scripts_BAT "*.bat")
FILE (GLOB scripts_VBS "*.vbs")
FILE (GLOB scripts_PS1 "*.ps1")
FILE (GLOB scripts_LIB "lib/*.vbs")

SET(script_ALL ${scripts_BAT} ${scripts_VBS} ${scripts_PS1})

INSTALL(FILES ${script_ALL} DESTINATION "scripts")
INSTALL(FILES ${scripts_LIB} DESTINATION "scripts/lib")

