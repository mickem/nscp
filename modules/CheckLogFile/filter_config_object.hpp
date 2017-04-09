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
		bool read_from_start;

		filter_config_object(std::string alias, std::string path)
			: parent(alias, path)
			, filter("${file}: ${count} (${list})", "${column1}, ${column2}, ${column3}", "NSCA")
			, column_split("\\t") 
			, read_from_start(false)
		{}

		std::string to_string() const;
		void set_files(std::string file_string);
		void set_file(std::string file_string);

		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);
	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}