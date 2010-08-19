#pragma once

#include "settings_logger_impl.hpp"
#include <settings/settings_core.hpp>
#include <settings/settings_ini.hpp>
#ifdef WIN32
#include <settings/settings_old.hpp>
#include <settings/settings_registry.hpp>
#endif

namespace settings_manager {
	class NSCSettingsImpl : public settings::settings_handler_impl {
	private:
		boost::filesystem::wpath boot_;
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
#ifdef WIN32
			wchar_t* buffer = new wchar_t[1024];
			GetPrivateProfileString(section.c_str(), key.c_str(), def.c_str(), buffer, 1023, boot_.string().c_str());
			std::wstring ret = buffer;
			delete [] buffer;
			return ret;
#else
			return def;
#endif
		}

		void boot(std::wstring file = _T("boot.ini"));
		std::wstring find_file(std::wstring file, std::wstring fallback = _T(""));
		std::wstring expand_path(std::wstring file);
		settings::instance_ptr create_instance(std::wstring key);
	};

	typedef Singleton<NSCSettingsImpl> SettingsHandler;

	// Alias to make handling "compatible" with old syntax
	settings::instance_ptr get_settings();
	settings::settings_core* get_core();
	void destroy_settings();
	bool init_settings();
}
