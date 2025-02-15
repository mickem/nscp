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

#include <nscapi/nscapi_core_wrapper.hpp>

#include <boost/enable_shared_from_this.hpp>

#include <string>

#include "Vcclr.h"
#include <clr/clr_scoped_ptr.hpp>

typedef cli::array<System::Byte> protobuf_data;

std::string to_nstring(System::String^ s);
System::String^ to_mstring(const std::string &s);
std::string to_nstring(protobuf_data^ byteArray);
protobuf_data^ to_pbd(const std::string &buffer);
