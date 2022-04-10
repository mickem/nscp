cmake_minimum_required(VERSION 2.8.12)

CONFIGURE_FILE(
	re-generate.bat.in
	${CMAKE_CURRENT_SOURCE_DIR}/re-generate.bat
)
