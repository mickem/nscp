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
			, dwLang(0) {}

		nscapi::settings_filters::filter_object filter;
		DWORD dwLang;
		std::list<std::string> files;

		std::string to_string() const;
		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

		static unsigned short get_language(std::string lang);

		void set_files(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			BOOST_FOREACH(const std::string &s, strEx::s::splitEx(file_string, std::string(","))) {
				files.push_back(s);
			}
		}
		void set_file(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			files.push_back(file_string);
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