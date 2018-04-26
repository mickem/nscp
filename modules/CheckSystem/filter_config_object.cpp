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
	namespace mem {

		std::string filter_config_object::to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << get_alias() << "] = "
				<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
			return ss.str();
		}

		void filter_config_object::set_data(std::string file_string) {
			if (file_string.empty())
				return;
			BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
				data.push_back(s);
			}
		}

		void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			if (!get_value().empty())
				filter.set_filter_string(get_value().c_str());
			bool is_default = parent::is_default();

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_path()
				("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias())
				;
			root_path.add_key()
				("type", sh::string_fun_key(boost::bind(&filter_config_object::set_data, this, _1)),
					"MEMORY TYPE", "The type of memory to check: physical, committed or virtual", false)
				;

			filter.read_object(root_path, is_default);

			settings.register_all();
			settings.notify();

		}
	}

	namespace cpu {

		std::string filter_config_object::to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << get_alias() << "] = "
				<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
			return ss.str();
		}

		void filter_config_object::set_data(std::string file_string) {
			if (file_string.empty())
				return;
			BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
				data.push_back(s);
			}
		}

		void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			if (!get_value().empty())
				filter.set_filter_string(get_value().c_str());
			bool is_default = parent::is_default();

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			if (is_default) {
				filter.set_filter_string("core = 'total'");
			}


			root_path.add_path()
				("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias())
				;
			root_path.add_key()
				("time", sh::string_fun_key(boost::bind(&filter_config_object::set_data, this, _1)),
					"TIME", "A list of times to check (coma separated)", true)
				;

			filter.read_object(root_path, is_default);

			settings.register_all();
			settings.notify();

		}
	}
	namespace proc {

		std::string filter_config_object::to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << get_alias() << "] = "
				<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
			return ss.str();
		}

		void filter_config_object::set_data(std::string file_string) {
			if (file_string.empty())
				return;
			BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
				data.push_back(s);
			}
		}

		void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
			if (!get_value().empty())
				filter.set_filter_string(get_value().c_str());
			bool is_default = parent::is_default();

			nscapi::settings_helper::settings_registry settings(proxy);
			nscapi::settings_helper::path_extension root_path = settings.path(get_path());
			if (is_sample)
				root_path.set_sample();

			root_path.add_path()
				("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias())
				;
			root_path.add_key()
				("process", sh::string_fun_key(boost::bind(&filter_config_object::set_data, this, _1)),
					"PROCESS", "A list of processes to check (or * for all)", false)
				;

			filter.read_object(root_path, is_default);

			settings.register_all();
			settings.notify();
		}
	}


	namespace legacy {

		std::string filter_config_object::to_string() const {
			std::stringstream ss;
			ss << get_alias() << "[" << get_alias() << "] = "
				<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
			return ss.str();
		}

		void filter_config_object::set_datas(std::string file_string) {
			if (file_string.empty())
				return;
			data.clear();
			BOOST_FOREACH(const std::string &s, str::utils::split_lst(file_string, std::string(","))) {
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
				filter.set_filter_string(get_value().c_str());
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
					"TYPE OF CHECK", "The type of check cpu, memory or process", false)
				;

			settings.register_all();
			settings.notify();

			if (check == "memory") {
				root_path.add_key()
					("type", sh::string_fun_key(boost::bind(&filter_config_object::set_data, this, _1)),
						"MEMORY TYPE", "The type of memory to check: physical, committed or virtual", false)

					("types", sh::string_fun_key(boost::bind(&filter_config_object::set_datas, this, _1)),
						"MEMORY TYPES", "A list of types to check: physical, committed or virtual", true)
					;
			} else {

				root_path.add_key()
					("time", sh::string_fun_key(boost::bind(&filter_config_object::set_data, this, _1)),
						"TIME", "The time to check", false)

					("times", sh::string_fun_key(boost::bind(&filter_config_object::set_datas, this, _1)),
						"FILES", "A list of times to check (soma separated)", true)
					;
			}

			filter.read_object(root_path, is_default);

			settings.register_all();
			settings.notify();
		}
	}

}