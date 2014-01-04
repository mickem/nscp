#include "filter_config_object.hpp"

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"


namespace sh = nscapi::settings_helper;

namespace filters {
	std::string filter_config_object::to_string() const {
		std::stringstream ss;
		ss << tpl.alias << "[" << tpl.alias << "] = "
			<< "{tpl: " << tpl.to_string() << ", filter: "  << filter.to_string() << "}";
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
	void command_reader::init_default(object_type& object) {
		// Populate default template!
		object.filter.debug = false;
		object.filter.target = "NSCA";
	}

	void command_reader::read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
		if (!object.tpl.value.empty())
			object.filter.filter_string = object.tpl.value;

		bool is_default = object.tpl.is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
		if (is_sample)
			root_path.set_sample();

		object.tpl.add_oneliner_hint(proxy, oneliner, is_sample);

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + object.tpl.alias)
			;
		root_path.add_key()
			("check", sh::string_key(&object.check, "cpu"),
			"TYPE OF CHECK", "The type of check cpu or memory", false)
			;

		object.tpl.read_object(root_path);
		object.filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();

		if (object.check == "memory") {
			if (is_default) {
				// Populate default values!
				object.filter.syntax_top = "${list}";
				object.filter.syntax_detail = "${type} > ${used}";
			}

			root_path.add_key()
				("type", sh::string_fun_key<std::string>(boost::bind(&object_type::set_data, &object, _1)),
				"TIME", "The time to check", false)

				("types", sh::string_fun_key<std::string>(boost::bind(&object_type::set_datas, &object, _1)),
				"FILES", "A list of times to check (soma separated)", true)
				;

		} else {
			if (is_default) {
				// Populate default values!
				object.filter.syntax_top = "${list}";
				object.filter.syntax_detail = "${core}>${load}%";
				object.filter.filter_string = "core = 'total'";
			}

			root_path.add_key()
				("time", sh::string_fun_key<std::string>(boost::bind(&object_type::set_data, &object, _1)),
				"TIME", "The time to check", false)

				("times", sh::string_fun_key<std::string>(boost::bind(&object_type::set_datas, &object, _1)),
				"FILES", "A list of times to check (soma separated)", true)
				;
		}
		object.tpl.read_object(root_path);
		object.filter.read_object(root_path, is_default);
		settings.register_all();
		settings.notify();
	}
	void command_reader::apply_parent(object_type &object, object_type &parent) {
		object.filter.apply_parent(parent.filter);
	}
}

