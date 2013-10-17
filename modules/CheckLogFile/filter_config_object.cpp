#pragma once

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

	void filter_config_object::set_files(std::string file_string) {
		if (file_string.empty())
			return;
		files.clear();
		BOOST_FOREACH(const std::string &s, strEx::s::splitEx(file_string, std::string(","))) {
			files.push_back(s);
		}
	}
	void filter_config_object::set_file(std::string file_string) {
		if (file_string.empty())
			return;
		files.clear();
		files.push_back(file_string);
	}

	void command_reader::read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
		if (!object.tpl.value.empty())
			object.filter.filter_string = object.tpl.value;
		std::string alias;
		bool is_default = object.tpl.is_default();
		if (is_default) {
			// Populate default template!
			object.filter.debug = false;
			object.filter.syntax_top = "${file}: ${count} (${list})";
			object.filter.syntax_detail = "${column1}, ${column2}, ${column3}";
			object.filter.target = "NSCA";
			object.column_split = "\\t";
		}

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
		if (is_sample)
			root_path.set_sample();

		if (oneliner) {
			std::string::size_type pos = object.tpl.path.find_last_of("/");
			if (pos != std::string::npos) {
				std::string path = object.tpl.path.substr(0, pos);
				std::string key = object.tpl.path.substr(pos+1);
				proxy->register_key(path, key, NSCAPI::key_string, object.tpl.alias, "Filter for " + object.tpl.alias + ". To configure this item add a section called: " + object.tpl.path, "", false, is_sample);
				proxy->set_string(path, key, object.tpl.value);
				return;
			}
		}

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + object.tpl.alias)
			;

		root_path.add_key()
			("file", sh::string_fun_key<std::string>(boost::bind(&object_type::set_file, &object, _1)),
			"FILE", "The eventlog record to filter on (if set to 'all' means all enabled logs)", false)

			("files", sh::string_fun_key<std::string>(boost::bind(&object_type::set_files, &object, _1)),
			"FILES", "The eventlog record to filter on (if set to 'all' means all enabled logs)", true)

			("column split", nscapi::settings_helper::string_key(&object.column_split), 
			"COLUMN SPLIT", "THe character(s) to use when splitting on column level", !is_default)

			;
		object.tpl.read_object(root_path);
		object.filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();
		if (!alias.empty())
			object.tpl.alias = alias;
	}
	void command_reader::apply_parent(object_type &object, object_type &parent) {
		using namespace nscapi::settings_objects;
		object.filter.apply_parent(parent.filter);
		import_string(object.column_split, parent.column_split);
		import_string(object.line_split, parent.line_split);
	}
}

