# Try to find protocol buffers (protobuf)
#
# Use as FIND_PACKAGE(ProtocolBuffers)
#
#  PROTOBUF_FOUND - system has the protocol buffers library
#  PROTOBUF_INCLUDE_DIR - the zip include directory
#  PROTOBUF_LIBRARY - Link this to use the zip library
#  PROTOBUF_PROTOC_EXECUTABLE - executable protobuf compiler
#
# And the following command
#
#  WRAP_PROTO(VAR input1 input2 input3..)
#
# Which will run protoc on the input files and set VAR to the names of the created .cc files,
# ready to be added to ADD_EXECUTABLE/ADD_LIBRARY. E.g,
#
#  WRAP_PROTO(PROTO_SRC myproto.proto external.proto)
#  ADD_EXECUTABLE(server ${server_SRC} {PROTO_SRC})
#
# Author: Esben Mose Hansen <[EMAIL PROTECTED]>, (C) Ange Optimization ApS 2008
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (PROTOBUF_LIBRARY AND PROTOBUF_INCLUDE_DIR AND PROTOBUF_PROTOC_EXECUTABLE)
	SET(PROTOBUF_FOUND TRUE)
ELSE (PROTOBUF_LIBRARY AND PROTOBUF_INCLUDE_DIR AND PROTOBUF_PROTOC_EXECUTABLE)
	FIND_PATH(PROTOBUF_INCLUDE_DIR 
		stubs/common.h
		PATHS
			/usr/include/google/protobuf
			/usr/local/include/google/protobuf
			${PROTOBUF_ROOT}
			${PROTOBUF_ROOT}/src
	)

	FIND_LIBRARY(PROTOBUF_LIBRARY_RELEASE 
		NAMES ${PROTOBUF_LIBRARY_PREFIX}protobuf${PROTOBUF_LIBRARY_SUFFIX} ${PROTOBUF_LIBRARY_PREFIX}libprotobuf${PROTOBUF_LIBRARY_SUFFIX}
		PATHS
		${GNUWIN32_DIR}/lib
		${PROTOBUF_LIBRARYDIR_RELEASE}
		${PROTOBUF_LIBRARYDIR}
		${PROTOBUF_ROOT}/vsprojects/Release
	)
	FIND_LIBRARY(PROTOBUF_LIBRARY_DEBUG 
		NAMES ${PROTOBUF_LIBRARY_PREFIX_DEBUG}protobuf${PROTOBUF_LIBRARY_SUFFIX_DEBUG} ${PROTOBUF_LIBRARY_PREFIX_DEBUG}libprotobuf${PROTOBUF_LIBRARY_SUFFIX_DEBUG}
		PATHS
		${GNUWIN32_DIR}/lib
		${PROTOBUF_LIBRARYDIR_DEBUG}
		${PROTOBUF_LIBRARYDIR}
		${PROTOBUF_ROOT}/vsprojects/Debug
	)
	SET(PROTOBUF_LIBRARY
		debug ${PROTOBUF_LIBRARY_DEBUG}
		optimized ${PROTOBUF_LIBRARY_RELEASE}
	)

	FIND_PROGRAM(PROTOBUF_PROTOC_EXECUTABLE protoc)
	IF(NOT PROTOBUF_PROTOC_EXECUTABLE)
		FIND_PROGRAM(PROTOBUF_PROTOC_EXECUTABLE protoc 
			PATHS
				${PROTOBUF_BINARYDIR}
				${PROTOBUF_ROOT}/vsprojects/Release
				${PROTOBUF_ROOT}/vsprojects/Debug
			)
	ENDIF(NOT PROTOBUF_PROTOC_EXECUTABLE)

	INCLUDE(FindPackageHandleStandardArgs)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(protobuf DEFAULT_MSG PROTOBUF_INCLUDE_DIR PROTOBUF_LIBRARY PROTOBUF_PROTOC_EXECUTABLE)

	# ensure that they are cached
	SET(PROTOBUF_INCLUDE_DIR ${PROTOBUF_INCLUDE_DIR} CACHE INTERNAL "The protocol buffers include path")
	SET(PROTOBUF_LIBRARY ${PROTOBUF_LIBRARY} CACHE INTERNAL "The libraries needed to use protocol buffers library")
	SET(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC_EXECUTABLE} CACHE INTERNAL "The protocol buffers compiler")
ENDIF (PROTOBUF_LIBRARY AND PROTOBUF_INCLUDE_DIR AND PROTOBUF_PROTOC_EXECUTABLE)
