#pragma once

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
		ss << alias << "[" << alias << "] = "
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
		if (!value.empty())
			filter.filter_string = value;
		bool is_default = parent::is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(path);
		if (is_sample)
			root_path.set_sample();

		//add_oneliner_hint(proxy, oneliner, is_sample);

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + alias)
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

