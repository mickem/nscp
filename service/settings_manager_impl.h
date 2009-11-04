#pragma once

#include "settings_logger_impl.hpp"
#include <settings/Settings.h>
#include <settings/settings_ini.hpp>
#include <settings/settings_old.hpp>
#include <settings/settings_registry.hpp>

namespace settings_manager {
	class NSCSettingsImpl : public Settings::SettingsHandlerImpl {
	private:
		std::wstring boot_;
		bool old_;
	public:
		NSCSettingsImpl() : old_(false) {}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string form the boot file.
		///
		/// @param section section to read a value from.
		/// @param key the key to read.
		/// @param def a default value.
		/// @return the value of the key or the default value.
		///
		/// @author mickem
		std::wstring get_boot_string(std::wstring section, std::wstring key, std::wstring def) {
			TCHAR* buffer = new TCHAR[1024];
			GetPrivateProfileString(section.c_str(), key.c_str(), def.c_str(), buffer, 1023, boot_.c_str());
			std::wstring ret = buffer;
			delete [] buffer;
			return ret;
		}
		//////////////////////////////////////////////////////////////////////////
		/// Boot the settings subsystem from the given file (boot.ini).
		///
		/// @param file the file to use when booting.
		///
		/// @author mickem
		void boot(std::wstring file = _T("boot.ini")) {
			boot_ = get_base() + _T("\\") + file;
			std::wstring subsystem = get_boot_string(_T("settings"), _T("type"), _T("old"));
			get_logger()->debug(__FILEW__, __LINE__, _T("Trying to boot: ") + subsystem + _T(" from base: ") + boot_);
			settings_type type = string_to_type(subsystem);
			std::wstring context = get_boot_string(_T("settings"), _T("context"), subsystem);
			Settings::SettingsInterface *impl = create_instance(type, context);
			if (impl == NULL)
				throw Settings::SettingsException(_T("Could not create settings instance: ") + subsystem);
			add_type_impl(type, impl);
			set_type(type);
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
		Settings::SettingsInterface* create_instance(settings_type type, std::wstring context) {
			get_logger()->debug(__FILEW__, __LINE__, _T("Trying to create: ") + SettingsCore::type_to_string(type) + _T(": ") + context);
			if (type == SettingsCore::old_ini_file) {
				old_ = true;
				return new Settings::OLDSettings(this, context);
			} 
			if (type == SettingsCore::ini_file)
				return new Settings::INISettings(this, context);
			if (type == SettingsCore::registry)
				return new Settings::REGSettings(this, context);
			throw SettingsException(_T("Undefined settings type: ") + SettingsCore::type_to_string(type));
		}

	};

	typedef Singleton<NSCSettingsImpl> SettingsHandler;

	// Alias to make handling "compatible" with old syntax
	Settings::SettingsInterface* get_settings();
	Settings::SettingsCore* get_core();
	void destroy_settings();
	bool init_settings(std::wstring path);
}
