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

#include <nsclient/logger/base_logger_impl.hpp>

#include <vector>
#include <string>

namespace nsclient {
	namespace logging {
		namespace impl {
			class simple_console_logger : public nsclient::logging::log_driver_interface_impl {
				std::string format_;
				std::vector<char> buf_;
			public:
				simple_console_logger();
				void do_log(const std::string data);
				struct config_data {
					std::string format;
				};
				config_data do_config();
				void synch_configure();
				void asynch_configure();
			};
		}
	}
}