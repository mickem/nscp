#pragma once

#include <map>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/optional.hpp>
#include <boost/date_time.hpp>

#include <NSCAPI.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>

#include "filter.hpp"

namespace filters {

	struct file_container {
		std::string file;
		boost::uintmax_t size;
	};


	struct filter_config_object {

		filter_config_object() : is_template(false), debug(false), severity(-1) {}

		// Object keys (managed by object handler)
		std::string path;
		std::string alias;
		std::string value;
		std::string parent;
		bool is_template;

		// Command keys
		bool debug;

		std::string syntax_top;
		std::string syntax_detail;
		std::string filter_string;
		std::string filter_ok;
		std::string filter_warn;
		std::string filter_crit;
		std::string perf_data;
		NSCAPI::nagiosReturn severity;
		std::string command;
		boost::optional<boost::posix_time::time_duration> max_age;
		std::string target;
		std::string empty_msg;
		std::string column_split;
		std::string line_split;

		std::list<std::string> files;


		std::string to_string() const;

		bool boot(std::string &error);
		void set_severity(std::string severity_);
		void set_files(std::string file_string);
		void set_file(std::string file_string);
		void touch(boost::posix_time::ptime now);
		bool has_changed();
		boost::posix_time::time_duration parse_time(std::string time);
		void set_max_age(std::string age);
	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	struct command_reader {
		typedef filter_config_object object_type;
		static void post_process_object(object_type&) {}
		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample);
		static void apply_parent(object_type &object, object_type &parent);
	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

