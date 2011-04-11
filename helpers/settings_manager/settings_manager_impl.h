#pragma once

#include <settings/settings_core.hpp>
#include <settings/client/settings_client.hpp>
#include <settings/settings_handler_impl.hpp>

namespace settings_manager {

	struct provider_interface {
		virtual std::wstring expand_path(std::wstring file) = 0;
		virtual void log_fatal_error(std::wstring error) = 0;
		virtual settings::logger_interface* create_logger() = 0;
	};

	class NSCSettingsImpl : public settings::settings_handler_impl {
	private:
		boost::filesystem::wpath boot_;
		bool old_;
		provider_interface *provider_;
	public:
		NSCSettingsImpl(provider_interface *provider) : old_(false), provider_(provider) {}
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
		void set_boot_string(std::wstring section, std::wstring key, std::wstring val) {
#ifdef WIN32
			WritePrivateProfileString(section.c_str(), key.c_str(), val.c_str(), boot_.string().c_str());
#else
#endif
		}

		void boot(std::wstring file = _T("boot.ini"));
		std::wstring find_file(std::wstring file, std::wstring fallback = _T(""));
		std::wstring expand_path(std::wstring file);
		settings::instance_raw_ptr create_instance(std::wstring key);
		bool check_file(std::wstring file, std::wstring tag, std::wstring &key);
		void change_context(std::wstring file);
	};

	//typedef Singleton<NSCSettingsImpl> SettingsHandler;

	// Alias to make handling "compatible" with old syntax
	settings::instance_ptr get_settings();
	settings::instance_ptr get_settings_no_wait();
	settings::settings_core* get_core();
	nscapi::settings_helper::settings_impl_interface_ptr get_proxy();
	void destroy_settings();
	bool init_settings(provider_interface *provider, std::wstring context = _T(""));
	void change_context(std::wstring context);
}
