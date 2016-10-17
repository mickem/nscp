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

#include "filter_config_object.hpp"

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"


namespace sh = nscapi::settings_helper;

namespace filters {
	std::string filter_config_object::to_string() const {
		std::stringstream ss;
		ss << get_alias() << "[" << get_alias() << "] = "
			<< "{tpl: " << parent::to_string() << ", filter: "  << filter.to_string() << "}";
		return ss.str();
	}

	void filter_config_object::set_datas(std::string file_string) {
		if (file_string.empty())
			return;
		data.clear();
		BOOST_FOREACH(const std::string &s, strEx::s::splitEx(file_string, std::string(","))) {
			data.push_back(s);
		}
	}
	void filter_config_object::set_data(std::string file_string) {
		if (file_string.empty())
			return;
		data.clear();
		data.push_back(file_string);
	}

	void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
		if (!get_value().empty())
			filter.filter_string = get_value();
		bool is_default = parent::is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(get_path());
		if (is_sample)
			root_path.set_sample();

		//add_oneliner_hint(proxy, oneliner, is_sample);

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias())
			;
		root_path.add_key()
			("check", sh::string_key(&check, "cpu"),
			"TYPE OF CHECK", "The type of check cpu or memory", false)
			;

		settings.register_all();
		settings.notify();

		if (check == "memory") {
			if (is_default) {
				// Populate default values!
				filter.syntax_top = "${list}";
				filter.syntax_detail = "${type} > ${used}";
			}

			root_path.add_key()
				("type", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_data, this, _1)),
				"TIME", "The time to check", false)

				("types", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_datas, this, _1)),
				"FILES", "A list of times to check (soma separated)", true)
				;

		} else {
			if (is_default) {
				// Populate default values!
				filter.syntax_top = "${list}";
				filter.syntax_detail = "${core}>${load}%";
				filter.filter_string = "core = 'total'";
			}

			root_path.add_key()
				("time", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_data, this, _1)),
				"TIME", "The time to check", false)

				("times", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_datas, this, _1)),
				"FILES", "A list of times to check (soma separated)", true)
				;
		}

		filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();
	}

}

