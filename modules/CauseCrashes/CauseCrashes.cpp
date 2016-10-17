/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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