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


	struct filter_config_object {

		filter_config_object() {}

		nscapi::settings_objects::template_object tpl;
		nscapi::settings_filters::filter_object filter;
		std::string column_split;
		std::string line_split;
		std::list<std::string> files;

		std::string to_string() const;
		void set_files(std::string file_string);
		void set_file(std::string file_string);
	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	struct command_reader {
		typedef filter_config_object object_type;
		static void post_process_object(object_type&) {}
		static void init_default(object_type&);
		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample);
		static void apply_parent(object_type &object, object_type &parent);
	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

