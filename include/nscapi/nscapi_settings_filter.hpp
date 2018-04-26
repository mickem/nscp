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

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/dll_defines.hpp>

#include <map>
#include <string>

namespace nscapi {
	namespace settings_filters {

		struct NSCAPI_EXPORT filter_object {

			bool debug;
			bool escape_html;
			std::string syntax_top;
			std::string syntax_detail;
			std::string target;
			std::string syntax_ok;
			std::string syntax_empty;
		private:
			std::string filter_string_;
		public:
			std::string filter_ok;
			std::string filter_warn;
			std::string filter_crit;

			std::string perf_data;
			std::string perf_config;
			NSCAPI::nagiosReturn severity;
			std::string command;
			boost::optional<boost::posix_time::time_duration> max_age;
			boost::optional<boost::posix_time::time_duration> silent_period;
			std::string target_id;
			std::string source_id;
			std::string timeout_msg;

			filter_object(std::string syntax_top, std::string syntax_detail, std::string target)
				: debug(false)
				, escape_html(false)
				, syntax_top(syntax_top)
				, syntax_detail(syntax_detail)
				, target(target)
				, severity(-1) {}

			filter_object(const filter_object &other)
				: debug(other.debug)
				, escape_html(other.escape_html)
				, syntax_top(other.syntax_top)
				, syntax_detail(other.syntax_detail)
				, target(other.target)
				, syntax_ok(other.syntax_ok)
				, syntax_empty(other.syntax_empty)
				, filter_string_(other.filter_string_)
				, filter_ok(other.filter_ok)
				, filter_warn(other.filter_warn)
				, filter_crit(other.filter_crit)
				, perf_data(other.perf_data)
				, perf_config(other.perf_config)
				, severity(other.severity) 
				, command(other.command)
				, max_age(other.max_age)
				, silent_period(other.silent_period)
				, target_id(other.target_id)
				, source_id(other.source_id)
				, timeout_msg(other.timeout_msg)
			{}

			void set_filter_string(const char* filter_string) {
				filter_string_ = filter_string;
			}
			const char* filter_string() {
				return filter_string_.c_str();
			}

			std::string to_string() const {
				std::stringstream ss;
				ss << "{TODO}";
				return ss.str();
			}

			void set_severity(std::string severity_) {
				severity = nscapi::plugin_helper::translateReturn(severity_);
			}

			inline boost::posix_time::time_duration parse_time(std::string time) {
				std::string::size_type p = time.find_first_of("sSmMhHdDwW");
				if (p == std::string::npos)
					return boost::posix_time::seconds(boost::lexical_cast<long>(time));
				long value = boost::lexical_cast<long>(time.substr(0, p));
				if ((time[p] == 's') || (time[p] == 'S'))
					return boost::posix_time::seconds(value);
				else if ((time[p] == 'm') || (time[p] == 'M'))
					return boost::posix_time::minutes(value);
				else if ((time[p] == 'h') || (time[p] == 'H'))
					return boost::posix_time::hours(value);
				else if ((time[p] == 'd') || (time[p] == 'D'))
					return boost::posix_time::hours(value * 24);
				else if ((time[p] == 'w') || (time[p] == 'W'))
					return boost::posix_time::hours(value * 24 * 7);
				return boost::posix_time::seconds(value);
			}

			void set_max_age(std::string age) {
				if (age != "none" && age != "infinite" && age != "false" && age != "off")
					max_age = parse_time(age);
			}
			void set_silent_period(std::string age) {
				if (age != "none" && age != "infinite" && age != "false" && age != "off")
					silent_period = parse_time(age);
			}

			void read_object(nscapi::settings_helper::path_extension &path, const bool is_default);
			void apply_parent(const filter_object &parent);
		};
	}
}