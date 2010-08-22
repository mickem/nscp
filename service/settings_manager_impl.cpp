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
	settings::instance_ptr get_settings() {
		return SettingsHandler::getInstance()->get();
	}
	settings::instance_ptr get_settings_no_wait() {
		return SettingsHandler::getInstance()->get_no_wait();
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
	settings::instance_raw_ptr NSCSettingsImpl::create_instance(std::wstring key) {
		net::url url = net::parse(key);
#ifdef WIN32
		if (url.protocol == _T("old")) {
			old_ = true;
			return settings::instance_raw_ptr(new settings::OLDSettings(this, key));
		} 
		if (url.protocol == _T("registry"))
			return settings::instance_raw_ptr(new settings::REGSettings(this, key));
#endif
		if (url.protocol == _T("ini"))
			return settings::instance_raw_ptr(new settings::INISettings(this, key));
		throw settings::settings_exception(_T("Undefined settings protocol: ") + url.protocol);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Boot the settings subsystem from the given file (boot.ini).
	///
	/// @param file the file to use when booting.
	///
	/// @author mickem
	void NSCSettingsImpl::boot(std::wstring file) {
		std::wstring key = file;
		if (file.empty()) {
			boot_ = mainClient.expand_path(_T("${base-path}/boot.ini"));
			key = get_boot_string(_T("settings"), _T("location"), DEFAULT_CONF_LOCATION);
		}
		set_instance(key);
	}

	bool init_settings(std::wstring context) {
		try {
			get_core()->set_logger(new settings_logger());
			get_core()->set_base(mainClient.expand_path(_T("${base-path}")));
			get_core()->boot(context);
			get_core()->register_key(SETTINGS_REG_KEY_I_GEN(settings_def::PAYLOAD_LEN, settings::settings_core::key_integer));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::ALLOWED_HOSTS, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_B_GEN(protocol_def::CACHE_ALLOWED, settings::settings_core::key_bool));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::MASTER_KEY, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::PWD, settings::settings_core::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::OBFUSCATED_PWD, settings::settings_core::key_string));
		} catch (settings::settings_exception e) {
			LOG_CRITICAL_STD(_T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			LOG_CRITICAL(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}
}
