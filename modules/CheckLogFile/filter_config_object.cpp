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
			<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
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

	void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
		if (!value.empty())
			filter.filter_string = value;
		bool is_default = parent::is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(path);
		if (is_sample)
			root_path.set_sample();

		if (oneliner) {
			std::string::size_type pos = path.find_last_of("/");
			if (pos != std::string::npos) {
				std::string lpath = path.substr(0, pos);
				std::string key = path.substr(pos + 1);
				proxy->register_key(lpath, key, NSCAPI::key_string, alias, "Filter for " + alias + ". To configure this item add a section called: " + path, "", false, is_sample);
				proxy->set_string(lpath, key, value);
				return;
			}
		}

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + alias)
			;

		root_path.add_key()
			("file", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_file, this, _1)),
				"FILE", "The eventlog record to filter on (if set to 'all' means all enabled logs)", false)

			("files", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_files, this, _1)),
				"FILES", "The eventlog record to filter on (if set to 'all' means all enabled logs)", true)

			("column split", nscapi::settings_helper::string_key(&column_split),
				"COLUMN SPLIT", "THe character(s) to use when splitting on column level", !is_default)

			;
		filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();
	}
}