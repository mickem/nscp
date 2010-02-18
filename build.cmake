IF(WIN32)

SET(Boost_DEBUG 1)

set(Boost_USE_STATIC_LIBS   ON)
set(BOOST_USE_MULTITHREADED ON)

SET(BOOST_INCLUDEDIR D:/source/include/)
SET(BOOST_LIBRARYDIR D:/source/lib/x86)
#SET(BOOST_LIB_SUFFIX vc80-mt)
#SET(Boost_VERSION 1.40)


SET(OPENSSL_INCLUDE_DIR D:/source/include/)
SET(CMAKE_LIBRARY_PATH D:/source/lib/x86/)

SET(PROTOBUF_INCLUDE_DIR D:/source/protobuf-2.3.0/src)
SET(PROTOBUF_LIBRARYDIR D:/source/protobuf-2.3.0/vsprojects/Debug)
SET(PROTOBUF_BINARYDIR D:/source/protobuf-2.3.0/vsprojects/Debug)

#SET(PROTOBUF_LIBRARYDIR C:/src/protobuf-2.3.0/vsprojects/Release)
#SET(PROTOBUF_BINARYDIR C:/src/protobuf-2.3.0/vsprojects/Release)


SET(CRYPTOPP_DIR C:/src/lib-src/Crypto++5.6.0)

SET(CRYPTOPP_SOURCE d:/source/libs-c/crypto++-5.6.0)

ELSE(WIN32)

ENDIF(WIN32)
SET(CRYPTOPP_SOURCE d:/source/libs-c/crypto++-5.6.0)
