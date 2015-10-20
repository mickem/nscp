#pragma once

#include <map>
#include <string>

#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include "filter.hpp"
#include "filter_config_object.hpp"

namespace sh = nscapi::settings_helper;

namespace eventlog_filter {

	unsigned short filter_config_object::get_language(std::string lang) {
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

	std::string filter_config_object::to_string() const {
		std::stringstream ss;
		ss << alias << "[" << alias << "] = " 
			<< "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
		return ss.str();
	}

	void filter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
		if (!value.empty())
			filter.filter_string = value;
		std::string alias;
		bool is_default = parent::is_default();

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(path);
		if (is_sample)
			root_path.set_sample();

		if (oneliner) {
			std::string::size_type pos = path.find_last_of("/");
			if (pos != std::string::npos) {
				std::string kpath = path.substr(0, pos);
				std::string key = path.substr(pos+1);
				proxy->register_key(path, key, NSCAPI::key_string, alias, "Filter for " + alias + ". To configure this item add a section called: " + path, "", false, is_sample);
				proxy->set_string(path, key, value);
				return;
			}
		}

		root_path.add_path()
			("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + alias)
			;

		root_path.add_key()
			("log", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_file, this, _1)),
			"FILE", "The eventlog record to filter on (if set to 'all' means all enabled logs)", false)

			("logs", sh::string_fun_key<std::string>(boost::bind(&filter_config_object::set_files, this, _1)),
			"FILES", "The eventlog record to filter on (if set to 'all' means all enabled logs)", true)

			;

		filter.read_object(root_path, is_default);

		settings.register_all();
		settings.notify();
	}
}

