#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <settings/client/settings_client.hpp>
#include <nscapi/settings_proxy.hpp>
#include <nscapi/settings_object.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;

namespace filters {


	WORD get_language(std::string lang) {
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

		filter_config_object() : is_template(false), debug(false) {}
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

			return *this;
		}
	
		// Object keys (managed by object handler)
		std::wstring path;
		std::wstring alias;
		std::wstring value;
		std::wstring parent;
		bool is_template;

		// Command keys
		eventlog_filter::filter_engine engine;
		std::wstring syntax;
		std::wstring date_format;
		std::wstring filter;
		bool debug;
		// TODO:

		std::wstring target;
		std::wstring ok_msg;
		std::wstring perf_msg;
		NSCAPI::nagiosReturn severity;
		DWORD dwLang;


		std::wstring to_wstring() const {
			std::wstringstream ss;
			ss << alias << _T("[") << alias << _T("] = ") 
				<< _T("{filter: ") << filter 
				<< _T(", syntax: ") << syntax 
				<< _T(", date_format: ") << date_format 
				<< _T(", debug: ") << debug 
				<< _T("}");
			return ss.str();
		}

		void set_filter(std::wstring str) {
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
		void set_severity(std::wstring severity) {
			severity = nscapi::plugin_helper::translateReturn(severity);
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


		static void read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object) {
			object.set_filter(object.value);
			std::wstring alias;
			bool is_default = object.alias == _T("default");
			if (is_default) {
				// Populate default template!
				object.date_format = DATE_FORMAT;
				object.debug = false;
				object.syntax = _T("%type% %source%: %message%");
				object.target = _T("NSCA");
			}

			nscapi::settings_helper::settings_registry settings(proxy);

			settings.path(object.path).add_path()
				(_T("REAL TIME FILTER DEFENITION"), _T("Definitation for real time filter: ") + object.alias)
				;

			settings.path(object.path).add_key()
				(_T("filter"), sh::string_fun_key<std::wstring>(boost::bind(&object_type::set_filter, &object, _1)),
				_T("FILTER"), _T("The filter to match"))

				(_T("alias"), sh::wstring_key(&alias),
				_T("ALIAS"), _T("The alias (service name) to report to server"), true)

				(_T("parent"), nscapi::settings_helper::wstring_key(&object.parent, _T("default")),
				_T("PARENT"), _T("The parent the target inherits from"), true)

				(_T("is template"), nscapi::settings_helper::bool_key(&object.is_template, false),
				_T("IS TEMPLATE"), _T("Declare this object as a template (this means it will not be available as a separate object)"), true)

				(_T("date format"), nscapi::settings_helper::wstring_key(&object.date_format),
				_T("DATE FORMAT"), _T("Format string for dates"), true)

				(_T("syntax"), nscapi::settings_helper::wstring_key(&object.syntax),
				_T("SYNTAX"), _T("Format string for dates"), !is_default)

				(_T("debug"), nscapi::settings_helper::bool_key(&object.debug),
				_T("DEBUG"), _T("Enable this to display debug information for this match filter"), true)

				(_T("destination"), nscapi::settings_helper::wstring_key(&object.target),
				_T("DESTINATION"), _T("The destination for intercepted messages"), !is_default)

				(_T("language"), nscapi::settings_helper::string_fun_key<std::string>(boost::bind(&object_type::set_language, &object, _1)),
				_T("MESSAGE LANGUAGE"), _T("The language to use for rendering message (mainly used fror testing)"), true)

				(_T("ok message"), nscapi::settings_helper::wstring_key(&object.ok_msg, _T("eventlog found no records")),
				_T("OK MESSAGE"), _T("This is the message sent periodically whenever no error is discovered."), !is_default)

				(_T("perf string"), nscapi::settings_helper::wstring_key(&object.perf_msg, _T("")),
				_T("PERF STRING"), _T("TODO."), true)

				(_T("severity"), nscapi::settings_helper::string_fun_key<std::wstring>(boost::bind(&object_type::set_severity, &object, _1)),
				_T("SEVERITY"), _T("THe severity of this message (OK, WARNING, CRITICAL, UNKNOWN)"), !is_default)

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
			import_string(object.date_format, parent.date_format);
			import_string(object.syntax, parent.syntax);
			import_string(object.filter, parent.filter);
			if (parent.debug)
				object.debug = parent.debug;
		}

	};
	typedef nscapi::settings_objects::object_handler<filter_config_object, command_reader> filter_config_handler;
}

