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

#include <map>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/optional.hpp>
#include <boost/date_time.hpp>

#include <NSCAPI.h>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_filter.hpp>

#include "filter.hpp"

namespace filters {
	struct file_container {
		std::string file;
		boost::uintmax_t size;
	};

	struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		nscapi::settings_filters::filter_object filter;
		std::string column_split;
		std::string line_split;
		std::list<std::string> files;

		filter_config_object(std::string alias, std::string path)
			: parent(alias, path)
			, filter("${file}: ${count} (${list})", "${column1}, ${column2}, ${column3}", "NSCA")
			, column_split("\\t") {}

		std::string to_string() const;
		void set_files(std::string file_string);
		void set_file(std::string file_string);

		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);
	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}