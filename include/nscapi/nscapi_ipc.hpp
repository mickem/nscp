#pragma once

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4913)
#pragma warning(disable:4512)
#pragma warning(disable:4244)
#include <protobuf/ipc.pb.h>
#pragma warning(pop)
#else
#include <protobuf/ipc.pb.h>
#endif
