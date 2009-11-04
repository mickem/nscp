#include "stdafx.h"

#include "settings_manager_impl.h"

#define LOG_CRITICAL_STD(msg) LOG_CRITICAL(((std::wstring)msg).c_str())
#define LOG_CRITICAL(msg) \
	NSAPIMessage(NSCAPI::critical, _T(__FILE__), __LINE__, msg)

namespace settings_manager {
	// Alias to make handling "compatible" with old syntax
	Settings::SettingsInterface* get_settings() {
		return SettingsHandler::getInstance()->get();
	}
	Settings::SettingsCore* get_core() {
		return SettingsHandler::getInstance();
	}
	void destroy_settings() {
		SettingsHandler::destroyInstance();
	}
	bool init_settings(std::wstring path) {
		try {
			get_core()->set_logger(new settings_logger());
			get_core()->set_base(path);
			get_core()->boot(_T("boot.ini"));
			get_core()->register_key(SETTINGS_REG_KEY_I_GEN(settings_def::PAYLOAD_LEN, Settings::SettingsCore::key_integer));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::ALLOWED_HOSTS, Settings::SettingsCore::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_B_GEN(protocol_def::CACHE_ALLOWED, Settings::SettingsCore::key_bool));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::MASTER_KEY, Settings::SettingsCore::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::PWD, Settings::SettingsCore::key_string));
			get_core()->register_key(SETTINGS_REG_KEY_S_GEN(protocol_def::OBFUSCATED_PWD, Settings::SettingsCore::key_string));

		} catch (SettingsException e) {
			LOG_CRITICAL_STD(_T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			LOG_CRITICAL(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}
}
