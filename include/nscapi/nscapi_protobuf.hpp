/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4018)
#pragma warning(disable:4100)
#pragma warning(disable:4913)
#pragma warning(disable:4512)
#pragma warning(disable:4244)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#pragma warning(disable:4996)
#include <protobuf/plugin.pb.h>
#pragma warning(pop)
#else
#include <protobuf/plugin.pb.h>
#endif
