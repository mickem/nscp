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

#include <nscapi/dll_defines.hpp>

#include <NSCAPI.h>
#include <nscapi/nscapi_protobuf.hpp>

#include <string>

namespace nscapi {
	namespace protobuf {
		namespace functions {

			NSCAPI_EXPORT Plugin::Common_ResultCode parse_nagios(const std::string &status);
			NSCAPI_EXPORT Plugin::Common_ResultCode nagios_status_to_gpb(int ret);
			NSCAPI_EXPORT int gbp_to_nagios_status(Plugin::Common::ResultCode ret);

		}
	}
}