#include "StdAfx.h"

#include "settings_manager_impl.h"
#include <config.h>
#include "NSClient++.h"

#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::wstring)msg).c_str())
#define LOG_CRITICAL(msg) \
	NSAPIMessage(NSCAPI::critical, __FILEW__, __LINE__, msg)

extern NSClient mainClient;

namespace settings_manager {
	// Alias to make handling "compatible" with old syntax
	boost::shared_ptr<settings::settings_interface> get_settings() {
		return SettingsHandler::getInstance()->get();
	}
	settings::settings_core* get_core() {
		return SettingsHandler::getInstance();
	}
	void destroy_settings() {
		SettingsHandler::destroyInstance();
	}

	std::wstring NSCSettingsImpl::find_file(std::wstring file, std::wstring fallback) {
		// @todo: replace this with a proper parser!
		if (file.size() == 0)
			file = fallback;
		return mainClient.expand_path(file);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Create an instance of a given type.
	/// Used internally to create instances of various settings types.
	///
	/// @param type the type to create
	/// @param context the context to use
	/// @return a new instance of given type.
	///
	/// @author mickem
	settings::instance_ptr NSCSettingsImpl::create_instance(std::wstring key) {
		net::url url = net::parse(key);
		get_logger()->debug(__FILEW__, __LINE__, _T("Trying to create: ") + url.protocol + _T(": ") + key);
#ifdef WIN32
		if (url.protocol == _T("old")) {
			old_ = true;
			return settings::instance_ptr(new settings::OLDSettings(this, key));
		} 
		if (url.protocol == _T("registry"))
			return settings::instance_ptr(new settings::REGSettings(this, key));
#endif
		if (url.protocol == _T("ini"))
			return settings::instance_ptr(new settings::INISettings(this, key));
		throw settings_exception(_T("Undefined settings protocol: ") + url.protocol);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Boot the settings subsystem from the given file (boot.ini).
	///
	/// @param file the file to use when booting.
	///
	/// @author mickem
	void NSCSettingsImpl::boot(std::wstring file) {
		boot_ = get_base() / file;
		std::wstring key = get_boot_string(_T("settings"), _T("location"), DEFAULT_CONF_LOCATION);
		get_logger()->debug(__FILEW__, __LINE__, _T("Trying to boot: ") + key + _T(" from base: ") + boot_.string());
		set_instance(key);
	}

	bool init_settings() {
		try {
			get_core()->set_logger(new settings_logger());
			get_core()->set_base(mainClient.expand_path(_T("${base-path}")));
			get_core()->boot(_T("boot.ini"));
			get_core()->register_key(SETTINGS_REG_KEY_I_GEN(settings_def::PAYLOAD_LEN, settings::settings_core::key_integer));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::ALLOWED_HOSTS, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_B_GEN(protocol_def::CACHE_ALLOWED, settings::settings_core::key_bool));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::MASTER_KEY, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::PWD, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::OBFUSCATED_PWD, settings::settings_core::key_string));
			LOG_CRITICAL_STD(_T("Loaded: ") + get_core()->to_string());

		} catch (settings_exception e) {
			LOG_CRITICAL_STD(_T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			LOG_CRITICAL(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}
}
