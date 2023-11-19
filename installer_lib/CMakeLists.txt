cmake_minimum_required(VERSION 3.5)
SET(TARGET installer_lib)
PROJECT(${TARGET} CXX)

SET(NSCP_INCLUDEDIR ${CMAKE_SOURCE_DIR}/../include)
SET(BUILD_PYTHON_FOLDER "${CMAKE_SOURCE_DIR}/../build/python")

SET(Boost_USE_STATIC_LIBS		ON)
SET(Boost_USE_STATIC_RUNTIME	OFF)
FIND_PACKAGE(Boost COMPONENTS system filesystem)
find_package (Python3 COMPONENTS Interpreter)

INCLUDE_DIRECTORIES(${NSCP_INCLUDEDIR})
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}/include)
add_definitions(-DUNICODE -D_UNICODE -DBOOST_ALL_NO_LIB)


SET(VERSION_TXT ${CMAKE_CURRENT_SOURCE_DIR}/../version.txt)
EXECUTE_PROCESS(
	COMMAND ${Python3_EXECUTABLE}
		"${BUILD_PYTHON_FOLDER}/version.py" 
		--file ${VERSION_TXT} 
		--display 
		--generate-hpp ${PROJECT_BINARY_DIR}/include/version.hpp
	OUTPUT_VARIABLE TMP_VERSION_OUT
	)

CONFIGURE_FILE(
  ${NSCP_INCLUDEDIR}/config.h.in
  ${PROJECT_BINARY_DIR}/include/config.h
  @ONLY
)


SET(SRCS
	${TARGET}.cpp
	${NSCP_INCLUDEDIR}/ServiceCmd.cpp
	${NSCP_INCLUDEDIR}/error/error_w32.cpp
		
	../libs/settings_manager/settings_manager_impl.cpp
	../libs/settings_manager/settings_handler_impl.cpp

	${NSCP_INCLUDEDIR}/nscapi/nscapi_helper.cpp
	${NSCP_INCLUDEDIR}/simpleini/ConvertUTF.c
	${NSCP_INCLUDEDIR}/utf8.cpp

	"${TARGET}.def"
	installer_helper.hpp
	${NSCP_INCLUDEDIR}/ServiceCmd.h
	${NSCP_ERROR_CPP}
	${NSCP_ERROR_HPP}
	
	
	# Logger
	${NSCP_INCLUDEDIR}/nsclient/logger/log_level.cpp
	${NSCP_INCLUDEDIR}/nsclient/logger/base_logger_impl.cpp
)


ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})

ADD_LIBRARY(${TARGET} SHARED ${SRCS})
MESSAGE(STATUS "Boost_FILESYSTEM_LIBRARY: ${Boost_FILESYSTEM_LIBRARY}")
TARGET_LINK_LIBRARIES(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_SYSTEM_LIBRARY}
	Msi.lib
)
#SET_LIBRARY_OUT_FOLDER_MODULE(${TARGET})


