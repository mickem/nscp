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

#include "filter_config_object.hpp"

#include "filter.hpp"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <str/utils.hpp>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

#include <map>
#include <string>


namespace sh = nscapi::settings_helper;

namespace filters {
	std::string filter_config_object::to_string() const {
		std::stringstream ss;
		ss << get_alias() << "[" << get_alias() << "] = "
			<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
		return ss.str();
	}

	void filter_config_object::set_files(std::string file_string) {
		if (file_string.empty())
			return;
		files.clear();
		BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
			files.push_back(s);
		}
	}
	void filter_config_object::set_file(std::string file_string) {
		if (file_string.empty())
			return;
		files.clear();
		files.push_back(file_string);
	}

	void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
		if (!get_value().empty())
			filter.set_filter_string(get_value().c_str());
		bool is_default = parent::is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(get_path());
		if (is_sample)
			root_path.set_sample();

		if (oneliner)
			return;

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias())
			;

		root_path.add_key()
			("file", sh::string_fun_key(boost::bind(&filter_config_object::set_file, this, _1)),
				"FILE", "The eventlog record to filter on (if set to 'all' means all enabled logs)", false)

			("files", sh::string_fun_key(boost::bind(&filter_config_object::set_files, this, _1)),
				"FILES", "The eventlog record to filter on (if set to 'all' means all enabled logs)", true)

			("column split", nscapi::settings_helper::string_key(&column_split),
			"COLUMN SPLIT", "THe character(s) to use when splitting on column level", !is_default)

			("column-split", nscapi::settings_helper::string_key(&column_split),
			"COLUMN SPLIT", "Alias for column split", true)

			("read entire file", nscapi::settings_helper::bool_key(&read_from_start),
			"read entire file", "Set to true to always read the entire file not just new data", true)

			;
		filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();
	}
}