CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(json_spirit)

IF(JSON_SPIRIT_FOUND)

	INCLUDE_DIRECTORIES(${JSON_SPIRIT_INCLUDE_DIR})
	SET(JSON_SPIRIT_SRCS
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_reader.cpp 
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_reader.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_value.cpp 
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_value.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_writer.cpp 
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_writer.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_error_position.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_reader_template.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_stream_reader.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_utils.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_writer_options.h
		${JSON_SPIRIT_INCLUDE_DIR}/json_spirit_writer_template.h)
 
	INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
	ADD_LIBRARY(json_spirit ${JSON_SPIRIT_SRCS})
	ADD_LIBRARY(json_spirit_static STATIC ${JSON_SPIRIT_SRCS})

	SET_TARGET_PROPERTIES(json_spirit PROPERTIES FOLDER "libraries")
	SET_TARGET_PROPERTIES(json_spirit_static PROPERTIES FOLDER "libraries")
ENDIF(JSON_SPIRIT_FOUND)
