// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

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
#include <protobuf/settings.pb.h>
#pragma warning(pop)
#else
#include <protobuf/settings.pb.h>
#endif
