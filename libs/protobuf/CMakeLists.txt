cmake_minimum_required(VERSION 2.6)

WRAP_PROTO(PROTO_PLUGIN_SRC plugin.proto)
WRAP_PROTO(PROTO_LOG_SRC log.proto)

SET(TARGET protobuf)

SET(SRCS
	plugin.proto
	log.proto
	
	${PROTO_PLUGIN_SRC}
	${PROTO_LOG_SRC}
)

add_library(${TARGET} ${SRCS})
SET_TARGET_PROPERTIES(${TARGET} PROPERTIES FOLDER "libraries")

target_link_libraries(${TARGET}
	${PROTOBUF_LIBRARY}
)
