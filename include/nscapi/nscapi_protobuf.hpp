#pragma once
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4913)
#pragma warning(disable:4512)
#pragma warning(disable:4244)
#include <protobuf/plugin.pb.h>
#pragma warning(pop)
#else
#include <protobuf/plugin.pb.h>
#endif
