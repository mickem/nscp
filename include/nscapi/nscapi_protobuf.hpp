#pragma once

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4913)
#pragma warning(disable:4512)
#pragma warning(disable:4244)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#include <protobuf/plugin.pb.h>
#pragma warning(pop)
#else
#include <protobuf/plugin.pb.h>
#endif
