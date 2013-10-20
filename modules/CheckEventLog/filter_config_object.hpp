#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_filter.hpp>

#include "filter.hpp"

namespace eventlog_filter {

	struct filter_config_object {

		filter_config_object() : dwLang(0) {}
	
		nscapi::settings_objects::template_object tpl;
		nscapi::settings_filters::filter_object filter;
		DWORD dwLang;
		std::list<std::string> files;

		std::string to_string() const;

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

	struct command_reader {
		typedef filter_config_object object_type;
		static void post_process_object(object_type &object) {}
		static void command_reader::init_default(object_type& object);
		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample);
		static void apply_parent(object_type &object, object_type &parent);
	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

