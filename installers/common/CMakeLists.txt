cmake_minimum_required(VERSION 3.5)

CONFIGURE_FILE(
	re-generate.bat.in
	${CMAKE_CURRENT_SOURCE_DIR}/re-generate.bat
)
