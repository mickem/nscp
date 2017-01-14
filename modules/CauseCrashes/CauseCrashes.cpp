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

#include "CauseCrashes.h"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_program_options.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void CauseCrashes::crash_client(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	po::options_description desc = nscapi::program_options::create_desc(request);
	po::variables_map vm;
	if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response))
		return;
	int *foo = 0;
	*foo = 0;
	return nscapi::protobuf::functions::set_response_bad(*response, "We should have crashed now...");
}