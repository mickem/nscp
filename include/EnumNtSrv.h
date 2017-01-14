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

namespace services_helper {

	void init();
	std::string get_service_classification(const std::string &name);

	struct service_info {
		std::string name;
		std::string displayname;
		service_info(std::string name, std::string displayname) 
			: name(name)
			, displayname(displayname)
			, pid(0)
			, state(0)
			, start_type(0)
			, error_control(0)
			, type(0)
			, delayed(false)
			, triggers(0)
			, classification_(get_service_classification(name))
		{}
		service_info(const service_info &other)
			: name(other.name), displayname(other.displayname)
			, pid(other.pid), state(other.state), start_type(other.start_type), error_control(other.error_control), type(other.type), delayed(other.delayed), triggers(other.triggers)
			, binary_path(other.binary_path)
			, classification_(other.classification_) {}

		DWORD pid;
		DWORD state;
		DWORD start_type;
		DWORD error_control;
		DWORD type;
		bool delayed;
		int triggers;

		std::string binary_path;
		std::string classification_;

		std::string get_state_s() const;
		std::string get_legacy_state_s() const;
		std::string get_classification() const {
			return classification_;
		}
		std::string get_start_type_s() const;
		long long get_state_i() const { return state; }
		long long get_start_type_i() const { return start_type; }
		std::string get_type() const;
		std::string get_name() const { return name; }
		std::string get_desc() const { return displayname; }
		long long get_pid() const { return pid; }
		long long get_delayed() const { return delayed ? 1 : 0; }
		long long get_is_trigger() const { return triggers > 0 ? 1 : 0; }
		long long get_triggers() const { return triggers; }

		static long long parse_start_type(const std::string &s);
		static long long parse_state(const std::string &s);
	};

	DWORD parse_service_type(const std::string str = "service");
	DWORD parse_service_state(const std::string str = "all");
	std::list<service_info> enum_services(const std::string computer, DWORD dwServiceType, DWORD dwServiceState);
	service_info get_service_info(const std::string computer, const std::string service);
}
