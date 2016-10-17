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

#include <list>
#include <string>

#include <boost/noncopyable.hpp>

#include <NSCAPI.h>

namespace nrdp {
	struct data : boost::noncopyable {
		enum item_type_type {
			type_service,
			type_host,
			type_command
		};
		struct item_type {
			item_type_type type;
			std::string host;
			std::string service;
			NSCAPI::nagiosReturn result;
			std::string message;
		};
		std::list<item_type> items;
		void add_host(std::string host, NSCAPI::nagiosReturn result, std::string message);
		void add_service(std::string host, std::string service, NSCAPI::nagiosReturn result, std::string message);
		void add_command(std::string command, std::list<std::string> args);
		std::string render_request() const;
	};
}