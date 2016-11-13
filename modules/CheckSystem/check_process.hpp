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

#pragma once

#include <nscapi/nscapi_protobuf.hpp>
#include "filter_config_object.hpp"

namespace process_checks {

	namespace realtime {

		struct proc_filter_helper_wrapper;
		struct helper {
			typedef boost::unordered_set<std::string> known_type;
			known_type known_processes_;
			proc_filter_helper_wrapper *proc_helper;

			helper(nscapi::core_wrapper *core, int plugin_id);
			void add_obj(boost::shared_ptr<filters::proc::filter_config_object> object);
			void boot();
			void check();

		};
	}

	namespace active {

		void check(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
	}
}
