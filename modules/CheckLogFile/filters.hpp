#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;

namespace filters {

	struct file_container {
		std::wstring file;
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
		std::wstring path;
		std::wstring alias;
		std::wstring value;
		std::wstring parent;
		bool is_template;

		// Command keys
		bool debug;

		std::wstring syntax_top;
		std::wstring syntax_detail;
		std::wstring filter_string;
		std::wstring filter_ok;
		std::wstring filter_warn;
		std::wstring filter_crit;
		NSCAPI::nagiosReturn severity;
		std::wstring command;
		boost::optional<boost::posix_time::time_duration> max_age;
		std::wstring target;
		std::wstring empty_msg;
		std::wstring column_split;

		// Runtime items
		logfile_filter::filter filter;
		boost::posix_time::ptime next_ok_;
		std::list<file_container> files;


		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << alias << _T("[") << alias << _T("] = ") 
				<< _T("{filter: ") << filter_string << _T(", ")  << filter_ok << _T(", ")  << filter_warn << _T(", ")  << filter_crit
				<< _T(", syntax: ") << syntax_top << _T(", ")  << syntax_detail
				<< _T(", debug: ") << debug 
				<< _T("}");
			return ss.str();
		}

		bool boot(std::string &error) {
			filter.debug = debug;
			if (!filter.build_syntax(utf8::cvt<std::string>(syntax_top), utf8::cvt<std::string>(syntax_detail), error)) {
				return false;
			}
			filter.build_engines(utf8::cvt<std::string>(filter_string), utf8::cvt<std::string>(filter_ok), utf8::cvt<std::string>(filter_warn), utf8::cvt<std::string>(filter_crit));

			if (!column_split.empty()) {
				strEx::replace(column_split, _T("\\t"), _T("\t"));
				strEx::replace(column_split, _T("\\n"), _T("\n"));
			}

			if (!filter.validate(error)) {
				return false;
			}
			return true;
		}

		void set_severity(std::wstring severity_) {
			severity = nscapi::plugin_helper::translateReturn(severity_);
		}
		void set_files(std::wstring file_string) {
			if (file_string.empty())
				return;
			files.clear();
			BOOST_FOREACH(const std::wstring &s, strEx::splitEx(file_string, _T(","))) {
				file_container fc;
				fc.file = s;
				fc.size = boost::filesystem::file_size(fc.file);
				files.push_back(fc);
			}
		}
		void set_file(std::wstring file_string) {
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


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner) {
			if (!object.value.empty())
				object.filter_string = object.value;
			std::wstring alias;
			bool is_default = object.alias == _T("default");
			if (is_default) {
				// Populate default template!
				object.debug = false;
				object.syntax_top = _T("${file}: ${count} (${lines})");
				object.syntax_detail = _T("${column1}, ${column2}, ${column3}");
				object.target = _T("NSCA");
				object.column_split = _T("\\t");
			}

			nscapi::settings_helper::settings_registry settings(proxy);

			if (oneliner) {
				std::wstring::size_type pos = object.path.find_last_of(_T("/"));
				if (pos != std::wstring::npos) {
					std::wstring path = object.path.substr(0, pos);
					std::wstring key = object.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.alias, _T("Filter for ") + object.alias + _T(". To configure this item add a section called: ") + object.path, _T(""), false);
					proxy->set_string(path, key, object.value);
					return;
				}
			}

			settings.path(object.path).add_path()
				(_T("REAL TIME FILTER DEFENITION"), _T("Definitation for real time filter: ") + object.alias)
				;

			settings.path(object.path).add_key()
				(_T("filter"), sh::wstring_key(&object.filter_string),
				_T("FILTER"), _T("Scan files for matching rows for each matching rows an OK message will be submitted"))

				(_T("warning"), sh::wstring_key(&object.filter_warn),
				_T("WARNING FILTER"), _T("If any rows match this filter severity will escalated to WARNING"))

				(_T("critical"), sh::wstring_key(&object.filter_crit),
				_T("CRITCAL FILTER"), _T("If any rows match this filter severity will escalated to CRITCAL"))

				(_T("ok"), sh::wstring_key(&object.filter_ok),
				_T("OK FILTER"), _T("If any rows match this filter severity will escalated down to OK"))

				(_T("alias"), sh::wstring_key(&alias),
				_T("ALIAS"), _T("The alias (service name) to report to server"), true)

				(_T("file"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_file, &object, _1)),
				_T("FILE"), _T("The eventlog record to filter on (if set to 'all' means all enabled logs)"), false)

				(_T("files"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_files, &object, _1)),
				_T("FILES"), _T("The eventlog record to filter on (if set to 'all' means all enabled logs)"), true)

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("PARENT"), _T("The parent the target inherits from"), true)

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be available as a separate object)"), true)

				(_T("top syntax"), nscapi::settings_helper::wstring_key(&object.syntax_top),
				_T("SYNTAX"), _T("Format string for dates"), !is_default)

				(_T("detail syntax"), nscapi::settings_helper::wstring_key(&object.syntax_detail),
				_T("SYNTAX"), _T("Format string for dates"), !is_default)

				(_T("debug"), nscapi::settings_helper::bool_key(&object.debug),
				_T("DEBUG"), _T("Enable this to display debug information for this match filter"), true)

				(_T("destination"), nscapi::settings_helper::wstring_key(&object.target),
				_T("DESTINATION"), _T("The destination for intercepted messages"), !is_default)

				(_T("maximum age"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_max_age, &object, _1), _T("5m")),
				_T("MAGIMUM AGE"), _T("How long before reporting \"ok\" (if this is set to off no ok will be reported only errors)"))

				(_T("empty message"), nscapi::settings_helper::wstring_key(&object.empty_msg, _T("eventlog found no records")),
				_T("EMPTY MESSAGE"), _T("The message to display if nothing matches the filter (generally considered the ok state)."), !is_default)

				(_T("severity"), nscapi::settings_helper::string_fun_key<std::wstring>(boost::bind(&object_type::set_severity, &object, _1)),
				_T("SEVERITY"), _T("THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)"), !is_default)

				(_T("command"), nscapi::settings_helper::wstring_key(&object.command), 
				_T("COMMAND NAME"), _T("The name of the command (think nagios service name) to report up stream (defaults to alias if not set)"), !is_default)

				(_T("column split"), nscapi::settings_helper::wstring_key(&object.column_split), 
				_T("COLUMN SPLIT"), _T("THe character(s) to use when spliting on column level"), !is_default)

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

