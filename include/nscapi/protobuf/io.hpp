// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Before any protobuf header: on Windows it defines PROTOBUF_USE_DLLS, and a
// protobuf header seen without it declares the library's functions with a
// different linkage than the generated headers do (C2375) - which is what
// happens when this header is included before one of the message headers.
#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4100)
#pragma warning(disable : 4913)
#pragma warning(disable : 4512)
#pragma warning(disable : 4244)
#pragma warning(disable : 4127)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#pragma warning(disable : 4996)
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#pragma warning(pop)
#else
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#endif
