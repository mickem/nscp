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

#include "filter.hpp"

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_filter.hpp>

#include <str/utils.hpp>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <map>
#include <string>

namespace eventlog_filter {
	struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		filter_config_object(std::string alias, std::string path)
			: parent(alias, path)
			, filter("${file}: ${count} (${list})", "${level}: ${message}", "NSCA")
			, dwLang(0)
			, truncate_(0) {}

		nscapi::settings_filters::filter_object filter;
		DWORD dwLang;
		int truncate_;
		std::list<std::string> files;

		std::string to_string() const;
		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

		static unsigned short get_language(std::string lang);

		void set_files(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
				files.push_back(s);
			}
		}
		void set_file(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			files.push_back(file_string);
		}

		void set_truncate(int truncate) {
			truncate_ = truncate;
		}
		int get_truncate() {
			return truncate_;
		}
		void set_language(std::string lang) {
			WORD wLang = get_language(lang);
			if (wLang == LANG_NEUTRAL)
				dwLang = MAKELANGID(wLang, SUBLANG_DEFAULT);
			else
				dwLang = MAKELANGID(wLang, SUBLANG_NEUTRAL);
		}
	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}