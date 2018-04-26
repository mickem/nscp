cmake_minimum_required(VERSION 2.6)

SET(TARGET WEBServer)
	
PROJECT(${TARGET})

CREATE_MODULE(SRCS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

IF(JSON_SPIRIT_FOUND)
	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	ADD_DEFINITIONS(-DHAVE_JSON_SPIRIT)
	ADD_DEFINITIONS(-DJSON_SPIRIT_STATIC_LINKED)
	SET(JSON_LIB json_spirit)
ELSE(JSON_SPIRIT_FOUND)
	SET(JSON_LIB)
ENDIF(JSON_SPIRIT_FOUND)

SET(SRCS ${SRCS}
	"${TARGET}.cpp"
  
  static_controller.cpp
  token_store.cpp
  grant_store.cpp
  session_manager_interface.cpp
  metrics_handler.cpp
  error_handler.cpp

  modules_controller.cpp
  query_controller.cpp
  scripts_controller.cpp
  legacy_command_controller.cpp
  legacy_controller.cpp
  api_controller.cpp
  log_controller.cpp
  info_controller.cpp
  settings_controller.cpp
  login_controller.cpp
  metrics_controller.cpp

	${NSCP_INCLUDEDIR}/socket/socket_helpers.cpp
	${NSCP_INCLUDEDIR}/client/simple_client.cpp
  
  

	${NSCP_INCLUDEDIR}/metrics/metrics_store_map.cpp

	${NSCP_DEF_PLUGIN_CPP}
)

add_definitions(-DMG_ENABLE_HTTP_WEBSOCKET=0)
add_definitions(-DMG_ENABLE_BROADCAST=1)
add_definitions(-DMG_ENABLE_THREADS)
ADD_DEFINITIONS(${NSCP_GLOBAL_DEFINES})
IF(OPENSSL_FOUND)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
	add_definitions(-DMG_ENABLE_SSL=1)
	add_definitions(-DMG_DISABLE_PFS=1)
	add_definitions(-DUSE_SSL)
	SET(EXTRA_LIBS ${EXTRA_LIBS} ${OPENSSL_LIBRARIES})
ENDIF(OPENSSL_FOUND)
INCLUDE_DIRECTORIES(../../libs/mongoose-cpp)

IF(WIN32)
	SET(SRCS ${SRCS}
		"${TARGET}.h"
		${NSCP_INCLUDEDIR}/client/simple_client.hpp
		${NSCP_INCLUDEDIR}/metrics/metrics_store_map.hpp
		${NSCP_INCLUDEDIR}/socket/socket_helpers.hpp

		info_controller.hpp
		settings_controller.cpp

		${NSCP_DEF_PLUGIN_HPP}
	)
ENDIF(WIN32)

add_library(${TARGET} MODULE ${SRCS})
OPENSSL_LINK_FIX(${TARGET})

target_link_libraries(${TARGET}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_DATE_TIME_LIBRARY}
	${Boost_REGEX_LIBRARY}
	${NSCP_DEF_PLUGIN_LIB}
	${EXTRA_LIBS}
	${JSON_LIB}
	nscp_mongoose
)
INCLUDE(${BUILD_CMAKE_FOLDER}/module.cmake)
