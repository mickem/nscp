#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;

namespace filters {


	static WORD get_language(std::string lang) {
		if (lang == "neutral") return LANG_NEUTRAL;
		if (lang == "arabic") return LANG_ARABIC;
		if (lang == "bulgarian") return LANG_BULGARIAN;
		if (lang == "catalan") return LANG_CATALAN;
		if (lang == "chinese") return LANG_CHINESE;
		if (lang == "czech") return LANG_CZECH;
		if (lang == "danish") return LANG_DANISH;
		if (lang == "german") return LANG_GERMAN;
		if (lang == "greek") return LANG_GREEK;
		if (lang == "english") return LANG_ENGLISH;
		if (lang == "spanish") return LANG_SPANISH;
		if (lang == "finnish") return LANG_FINNISH;
		if (lang == "french") return LANG_FRENCH;
		if (lang == "hebrew") return LANG_HEBREW;
		if (lang == "hungarian") return LANG_HUNGARIAN;
		if (lang == "icelandic") return LANG_ICELANDIC;
		if (lang == "italian") return LANG_ITALIAN;
		if (lang == "japanese") return LANG_JAPANESE;
		if (lang == "korean") return LANG_KOREAN;
		if (lang == "dutch") return LANG_DUTCH;
		if (lang == "norwegian") return LANG_NORWEGIAN;
		if (lang == "polish") return LANG_POLISH;
		if (lang == "portuguese") return LANG_PORTUGUESE;
		if (lang == "romanian") return LANG_ROMANIAN;
		if (lang == "russian") return LANG_RUSSIAN;
		if (lang == "croatian") return LANG_CROATIAN;
		if (lang == "serbian") return LANG_SERBIAN;
		if (lang == "slovak") return LANG_SLOVAK;
		if (lang == "albanian") return LANG_ALBANIAN;
		if (lang == "swedish") return LANG_SWEDISH;
		if (lang == "thai") return LANG_THAI;
		if (lang == "turkish") return LANG_TURKISH;
		if (lang == "urdu") return LANG_URDU;
		if (lang == "indonesian") return LANG_INDONESIAN;
		if (lang == "ukrainian") return LANG_UKRAINIAN;
		if (lang == "belarusian") return LANG_BELARUSIAN;
		if (lang == "slovenian") return LANG_SLOVENIAN;
		if (lang == "estonian") return LANG_ESTONIAN;
		if (lang == "latvian") return LANG_LATVIAN;
		if (lang == "lithuanian") return LANG_LITHUANIAN;
		if (lang == "farsi") return LANG_FARSI;
		if (lang == "vietnamese") return LANG_VIETNAMESE;
		if (lang == "armenian") return LANG_ARMENIAN;
		if (lang == "azeri") return LANG_AZERI;
		if (lang == "basque") return LANG_BASQUE;
		if (lang == "macedonian") return LANG_MACEDONIAN;
		if (lang == "afrikaans") return LANG_AFRIKAANS;
		if (lang == "georgian") return LANG_GEORGIAN;
		if (lang == "faeroese") return LANG_FAEROESE;
		if (lang == "hindi") return LANG_HINDI;
		if (lang == "malay") return LANG_MALAY;
		if (lang == "kazak") return LANG_KAZAK;
		if (lang == "kyrgyz") return LANG_KYRGYZ;
		if (lang == "swahili") return LANG_SWAHILI;
		if (lang == "uzbek") return LANG_UZBEK;
		if (lang == "tatar") return LANG_TATAR;
		if (lang == "punjabi") return LANG_PUNJABI;
		if (lang == "gujarati") return LANG_GUJARATI;
		if (lang == "tamil") return LANG_TAMIL;
		if (lang == "telugu") return LANG_TELUGU;
		if (lang == "kannada") return LANG_KANNADA;
		if (lang == "marathi") return LANG_MARATHI;
		if (lang == "sanskrit") return LANG_SANSKRIT;
		if (lang == "mongolian") return LANG_MONGOLIAN;
		if (lang == "galician") return LANG_GALICIAN;
		if (lang == "konkani") return LANG_KONKANI;
		if (lang == "syriac") return LANG_SYRIAC;
		if (lang == "divehi") return LANG_DIVEHI;
		return LANG_NEUTRAL;
	}

	struct filter_config_object {

		filter_config_object() : is_template(false), debug(false), severity(-1), dwLang(0), max_age_(0), next_ok_(0) {}
		filter_config_object(const filter_config_object &other) 
			: path(other.path)
			, alias(other.alias)
			, value(other.value)
			, parent(other.parent)
			, is_template(other.is_template)
			, engine(other.engine)
			, syntax(other.syntax)
			, date_format(other.date_format)
			, filter(other.filter)
			, debug(other.debug)
			, target(other.target)
			, ok_msg(other.ok_msg)
			, perf_msg(other.perf_msg)
			, severity(other.severity)
			, dwLang(other.dwLang)
			, command(other.command)
			, log_(other.log_)
			, max_age_(other.max_age_)
			, next_ok_(other.next_ok_)

		{}
		const filter_config_object& operator =(const filter_config_object &other) {
			path = other.path;
			alias = other.alias;
			value = other.value;
			parent = other.parent;
			is_template = other.is_template;
			engine = other.engine;
			syntax = other.syntax;
			date_format = other.date_format;
			filter = other.filter;
			debug = other.debug;
			target = other.target;
			ok_msg = other.ok_msg;
			perf_msg = other.perf_msg;
			severity = other.severity;
			dwLang = other.dwLang;
			command = other.command;
			log_ = other.log_;
			max_age_ = other.max_age_;
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
		eventlog_filter::filter_engine engine;
		std::string syntax;
		std::string date_format;
		std::string filter;
		bool debug;
		std::string target;
		std::string ok_msg;
		std::string perf_msg;
		NSCAPI::nagiosReturn severity;
		DWORD dwLang;
		std::string command;
		std::string log_;
		DWORD max_age_;
		DWORD next_ok_ ;


		std::string to_string() const {
			std::stringstream ss;
			ss << alias << "[" << alias << "] = " 
				<< "{filter: " << filter 
				<< ", syntax: " << syntax 
				<< ", date_format: " << date_format 
				<< ", debug: " << debug 
				<< "}";
			return ss.str();
		}

		void set_filter(std::string str) {
			if (str.empty())
				return;
			filter = str;
		}

		void set_language(std::string lang) {
			WORD wLang = get_language(lang);
			if (wLang == LANG_NEUTRAL)
				dwLang = MAKELANGID(wLang, SUBLANG_DEFAULT);
			else
				dwLang = MAKELANGID(wLang, SUBLANG_NEUTRAL);
		}
		void set_severity(std::string severity_) {
			severity = nscapi::plugin_helper::translateReturn(severity_);
		}

		void touch(DWORD now) {
			if (max_age_ == 0)
				next_ok_ = 0;
			else
				next_ok_ = now+max_age_;

		}
		void set_max_age(std::string age) {
			if (age == "none" || age == "infinite" || age == "false")
				max_age_ = 0;
			else
				max_age_ = strEx::stoi64_as_time(age)/1000;
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
			object.set_filter(utf8::cvt<std::string>(object.value));
			std::string alias;
			bool is_default = object.alias == "default";
			if (is_default) {
				// Populate default template!
				object.date_format = utf8::cvt<std::string>(DATE_FORMAT);
				object.debug = false;
				object.syntax = "%type% %source%: %message%";
				object.target = "NSCA";
			}

			nscapi::settings_helper::settings_registry settings(proxy);

			if (oneliner) {
				std::string::size_type pos = object.path.find_last_of("/");
				if (pos != std::string::npos) {
					std::string path = object.path.substr(0, pos);
					std::string key = object.path.substr(pos+1);
					proxy->register_key(path, key, NSCAPI::key_string, object.alias, "Filter for " + object.alias + ". To configure this item add a section called: " + object.path, "", false, false);
					proxy->set_string(path, key, object.value);
					return;
				}
			}

			settings.path(object.path).add_path()
				("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + object.alias)
				;

			settings.path(object.path).add_key()
				("filter", sh::string_fun_key<std::string>(boost::bind(&object_type::set_filter, &object, _1)),
				"FILTER", "The filter to match")

				("alias", sh::string_key(&alias),
				"ALIAS", "The alias (service name) to report to server", true)

				("log", sh::string_key(&object.log_, "all"),
				"LOG", "The event log record to filter on (if set to 'all' means all enabled logs)", true)

				("parent", sh::string_key(&object.parent, "default"),
				"PARENT", "The parent the target inherits from", true)

				("is template", nscapi::settings_helper::bool_key(&object.is_template, false),
				"IS TEMPLATE", "Declare this object as a template (this means it will not be available as a separate object)", true)

				("date format", nscapi::settings_helper::string_key(&object.date_format),
				"DATE FORMAT", "Format string for dates", true)

				("syntax", nscapi::settings_helper::string_key(&object.syntax),
				"SYNTAX", "Format string for dates", !is_default)

				("debug", nscapi::settings_helper::bool_key(&object.debug),
				"DEBUG", "Enable this to display debug information for this match filter", true)

				("destination", nscapi::settings_helper::string_key(&object.target),
				"DESTINATION", "The destination for intercepted messages", !is_default)

				("language", nscapi::settings_helper::string_fun_key<std::string>(boost::bind(&object_type::set_language, &object, _1)),
				"MESSAGE LANGUAGE", "The language to use for rendering message (mainly used fror testing)", true)

				("maximum age", sh::string_fun_key<std::string>(boost::bind(&object_type::set_max_age, &object, _1), "5m"),
				"MAGIMUM AGE", "How long before reporting \"ok\" (if this is set to off no ok will be reported only errors)")

				("ok message", nscapi::settings_helper::string_key(&object.ok_msg, "eventlog found no records"),
				"OK MESSAGE", "This is the message sent periodically whenever no error is discovered.", !is_default)

				("perf string", nscapi::settings_helper::string_key(&object.perf_msg, ""),
				"PERF STRING", "TODO.", true)

				("severity", nscapi::settings_helper::string_fun_key<std::string>(boost::bind(&object_type::set_severity, &object, _1)),
				"SEVERITY", "THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)", !is_default)

				("command", nscapi::settings_helper::string_key(&object.command), 
				"COMMAND NAME", "The name of the command (think nagios service name) to report up stream (defaults to alias if not set)", !is_default)

				;

			settings.register_all();
			settings.notify();
			if (!alias.empty())
				object.alias = alias;

			/*
			BOOST_FOREACH(const object_type::options_type::value_type &kvp, options) {
				if (!object.has_option(kvp.first))
					object.options[kvp.first] = kvp.second;
			}
			*/

		}
		static void apply_parent(object_type &object, object_type &parent) {
			import_string(object.syntax, parent.syntax);
			import_string(object.date_format, parent.date_format);
			import_string(object.filter, parent.filter);
			if (parent.debug)
				object.debug = parent.debug;
			import_string(object.target, parent.target);
			import_string(object.ok_msg, parent.ok_msg);
			import_string(object.perf_msg, parent.perf_msg);
			if (parent.severity != -1 && object.severity == -1)
				object.severity = parent.severity;
			if (parent.dwLang != 0 && object.dwLang != 0)
				object.dwLang = parent.dwLang;
			import_string(object.command, parent.command);
		}

	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

