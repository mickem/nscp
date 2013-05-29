#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;

namespace filters {

	struct file_container {
		std::string file;
		boost::uintmax_t size;
	};


	struct filter_config_object {

		filter_config_object() : is_template(false), debug(false), severity(-1) {}
		filter_config_object(const filter_config_object &other) 
			: path(other.path)
			, alias(other.alias)
			, value(other.value)
			, parent(other.parent)
			, is_template(other.is_template)
			, filter(other.filter)
			, syntax_top(other.syntax_top)
			, syntax_detail(other.syntax_detail)
			, filter_string(other.filter_string)
			, filter_ok(other.filter_ok)
			, filter_warn(other.filter_warn)
			, filter_crit(other.filter_crit)
			, debug(other.debug)
			, target(other.target)
			, empty_msg(other.empty_msg)
			, column_split(other.column_split)
			, severity(other.severity)
			, command(other.command)
			, files(other.files)
			, max_age(other.max_age)
			, next_ok_(other.next_ok_)

		{}
		const filter_config_object& operator =(const filter_config_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			is_template = other.is_template;
			filter = other.filter;
			syntax_top = other.syntax_top;
			syntax_detail = other.syntax_detail;
			filter_string = other.filter_string;
			filter_ok = other.filter_ok;
			filter_warn = other.filter_warn;
			filter_crit = other.filter_crit;
			debug = other.debug;
			target = other.target;
			empty_msg = other.empty_msg;
			column_split = other.column_split;
			severity = other.severity;
			command = other.command;
			files = other.files;
			max_age = other.max_age;
			next_ok_ = other.next_ok_;

			return *this;
		}
	
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
		NSCAPI::nagiosReturn severity;
		std::string command;
		boost::optional<boost::posix_time::time_duration> max_age;
		std::string target;
		std::string empty_msg;
		std::string column_split;

		// Runtime items
		logfile_filter::filter filter;
		boost::posix_time::ptime next_ok_;
		std::list<file_container> files;


		std::string to_string() const {
			std::stringstream ss;
			ss << alias << "[" << alias << "] = "
				<< "{filter: " << filter_string << ", "  << filter_ok << ", "  << filter_warn << ", "  << filter_crit
				<< ", syntax: " << syntax_top << ", "  << syntax_detail
				<< ", debug: " << debug 
				<< "}";
			return ss.str();
		}

		bool boot(std::string &error) {
			filter.debug = debug;
			if (!filter.build_syntax(utf8::cvt<std::string>(syntax_top), utf8::cvt<std::string>(syntax_detail), error)) {
				return false;
			}
			if (!filter.build_engines(utf8::cvt<std::string>(filter_string), utf8::cvt<std::string>(filter_ok), utf8::cvt<std::string>(filter_warn), utf8::cvt<std::string>(filter_crit), error)) {
				return false;
			}

			if (!column_split.empty()) {
				strEx::replace(column_split, "\\t", "\t");
				strEx::replace(column_split, "\\n", "\n");
			}

			if (!filter.validate(error)) {
				return false;
			}
			return true;
		}

		void set_severity(std::string severity_) {
			severity = nscapi::plugin_helper::translateReturn(severity_);
		}
		void set_files(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			BOOST_FOREACH(const std::string &s, strEx::s::splitEx(file_string, std::string(","))) {
				file_container fc;
				fc.file = s;
				fc.size = boost::filesystem::file_size(fc.file);
				files.push_back(fc);
			}
		}
		void set_file(std::string file_string) {
			if (file_string.empty())
				return;
			files.clear();
			file_container fc;
			fc.file = file_string;
			fc.size = boost::filesystem::file_size(fc.file);
			files.push_back(fc);
		}

		void touch(boost::posix_time::ptime now) {
			if (max_age)
				next_ok_ = now+ (*max_age);
			BOOST_FOREACH(file_container &fc, files) {
				fc.size = boost::filesystem::file_size(fc.file);
			}
		}

		bool has_changed() {
			BOOST_FOREACH(const file_container &fc, files) {
				if (fc.size != boost::filesystem::file_size(fc.file))
					return true;
			}
			return false;
		}

		inline boost::posix_time::time_duration parse_time(std::wstring time) {
			std::wstring::size_type p = time.find_first_of(_T("sSmMhHdDwW"));
			if (p == std::wstring::npos)
				return boost::posix_time::seconds(boost::lexical_cast<long long>(time));
			unsigned long long value = boost::lexical_cast<long long>(time.substr(0, p));
			if ( (time[p] == 's') || (time[p] == 'S') )
				return boost::posix_time::seconds(value);
			else if ( (time[p] == 'm') || (time[p] == 'M') )
				return boost::posix_time::minutes(value);
			else if ( (time[p] == 'h') || (time[p] == 'H') )
				return boost::posix_time::hours(value);
			else if ( (time[p] == 'd') || (time[p] == 'D') )
				return boost::posix_time::hours(value*24);
			else if ( (time[p] == 'w') || (time[p] == 'W') )
				return boost::posix_time::hours(value*24*7);
			return boost::posix_time::seconds(value);
		}

		void set_max_age(std::wstring age) {
			if (age != _T("none") && age != _T("infinite") && age != _T("false"))
				max_age = parse_time(age);
		} 


	};
	typedef boost::optional<filter_config_object> optional_filter_config_object;

	template<class T>
	inline void import_string(T &object, T &parent) {
		if (object.empty() && !parent.empty())
			object = parent;
	}


	struct command_reader {
		typedef filter_config_object object_type;

		static void post_process_object(object_type &object) {}


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample = false) {
			if (!object.value.empty())
				object.filter_string = object.value;
			std::string alias;
			bool is_default = object.alias == "default";
			if (is_default) {
				// Populate default template!
				object.debug = false;
				object.syntax_top = "${file}: ${count} (${lines})";
				object.syntax_detail = "${column1}, ${column2}, ${column3}";
				object.target = "NSCA";
				object.column_split = "\\t";
			}

			nscapi::settings_helper::settings_registry settings(proxy);

			if (oneliner) {
				std::string::size_type pos = object.path.find_last_of("/");
				if (pos != std::string::npos) {
					std::string path = object.path.substr(0, pos);
					std::string key = object.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.alias, "Filter for " + object.alias + ". To configure this item add a section called: " + object.path, "", false, is_sample);
					proxy->set_string(path, key, object.value);
					return;
				}
			}

			nscapi::settings_helper::path_extension root_path = settings.path(object.path);
			if (is_sample)
				root_path.set_sample();

			root_path.add_path()
				("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + object.alias)
				;

			root_path.add_key()
				("filter", sh::string_key(&object.filter_string),
				"FILTER", "Scan files for matching rows for each matching rows an OK message will be submitted")

				("warning", sh::string_key(&object.filter_warn),
				"WARNING FILTER", "If any rows match this filter severity will escalated to WARNING")

				("critical", sh::string_key(&object.filter_crit),
				"CRITCAL FILTER", "If any rows match this filter severity will escalated to CRITCAL")

				("ok", sh::string_key(&object.filter_ok),
				"OK FILTER", "If any rows match this filter severity will escalated down to OK")

				("alias", sh::string_key(&alias),
				"ALIAS", "The alias (service name) to report to server", true)

				("file", sh::string_fun_key<std::string>(boost::bind(&object_type::set_file, &object, _1)),
				"FILE", "The eventlog record to filter on (if set to 'all' means all enabled logs)", false)

				("files", sh::string_fun_key<std::string>(boost::bind(&object_type::set_files, &object, _1)),
				"FILES", "The eventlog record to filter on (if set to 'all' means all enabled logs)", true)

				("parent", nscapi::settings_helper::string_key(&object.parent, "default"),
				"PARENT", "The parent the target inherits from", true)

				("is template", nscapi::settings_helper::bool_key(&object.is_template, false),
				"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

				("top syntax", sh::string_key(&object.syntax_top),
				"SYNTAX", "Format string for dates", !is_default)

				("detail syntax", sh::string_key(&object.syntax_detail),
				"SYNTAX", "Format string for dates", !is_default)

				("debug", nscapi::settings_helper::bool_key(&object.debug),
				"DEBUG", "Enable this to display debug information for this match filter", true)

				("destination", nscapi::settings_helper::string_key(&object.target),
				"DESTINATION", "The destination for intercepted messages", !is_default)

				("maximum age", sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_max_age, &object, _1), _T("5m")),
				"MAGIMUM AGE", "How long before reporting \"ok\".\nIf this is set to \"false\" no periodic ok messages will be reported only errors.")

				("empty message", nscapi::settings_helper::string_key(&object.empty_msg, "eventlog found no records"),
				"EMPTY MESSAGE", "The message to display if nothing matches the filter (generally considered the ok state).", !is_default)

				("severity", nscapi::settings_helper::string_fun_key<std::string>(boost::bind(&object_type::set_severity, &object, _1)),
				"SEVERITY", "THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)", !is_default)

				("command", nscapi::settings_helper::string_key(&object.command), 
				"COMMAND NAME", "The name of the command (think nagios service name) to report up stream (defaults to alias if not set)", !is_default)

				("column split", nscapi::settings_helper::string_key(&object.column_split), 
				"COLUMN SPLIT", "THe character(s) to use when splitting on column level", !is_default)

				;

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.alias = alias;
		}
		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.syntax_detail, parent.syntax_detail);
			import_string(object.syntax_top, parent.syntax_top);
			import_string(object.filter_string, parent.filter_string);
			import_string(object.filter_warn, parent.filter_warn);
			import_string(object.filter_crit, parent.filter_crit);
			import_string(object.filter_ok, parent.filter_ok);
			import_string(object.column_split, parent.column_split);
			if (parent.debug)
				object.debug = parent.debug;
			import_string(object.target, parent.target);
			import_string(object.empty_msg, parent.empty_msg);
			if (parent.severity != -1 && object.severity == -1)
				object.severity = parent.severity;
			import_string(object.command, parent.command);
		}

	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

