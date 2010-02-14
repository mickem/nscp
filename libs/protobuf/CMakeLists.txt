CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

WRAP_PROTO(PB_PLUGIN plugin.proto)

SET(TARGET protobuf)

#PROJECT(${TARGET})

SET(SRCS
	${PB_PLUGIN}
)

add_library(${TARGET} ${SRCS})

target_link_libraries(${TARGET}
	${PROTOBUF_LIBRARY}
)
