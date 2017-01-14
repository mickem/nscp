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

#include <list>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/tuple/tuple.hpp>

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
		static boost::tuple<int, std::string> parse_response(const std::string &str);
	};
}