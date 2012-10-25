#pragma once

#include <settings/settings_core.hpp>
#include <settings/client/settings_client.hpp>
#include "settings_handler_impl.hpp"

namespace settings_manager {

	struct provider_interface {
		virtual std::wstring expand_path(std::wstring file) = 0;
		virtual std::wstring get_data(std::wstring key) = 0;
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
			if (ret == def) {
				std::wstring tmp = provider_->get_data(key);
				if (!tmp.empty())
					return tmp;
				return def;
			}
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

		std::wstring expand_context(std::wstring key);
		void boot(std::wstring file = BOOT_CONF_LOCATION);
		std::wstring find_file(std::wstring file, std::wstring fallback = _T(""));
		std::wstring expand_path(std::wstring file);
		settings::instance_raw_ptr create_instance(std::wstring key);
		bool check_file(std::wstring file, std::wstring tag, std::wstring &key);
		void change_context(std::wstring file);
		bool context_exists(std::wstring key);
		bool create_context(std::wstring key);
		bool has_boot_conf();
		void set_primary(std::wstring key);
	};

	// Alias to make handling "compatible" with old syntax
	settings::instance_ptr get_settings();
	settings::instance_ptr get_settings_no_wait();
	settings::settings_core* get_core();
	nscapi::settings_helper::settings_impl_interface_ptr get_proxy();
	void destroy_settings();
	bool init_settings(provider_interface *provider, std::wstring context = _T(""));
	void change_context(std::wstring context);
	bool has_boot_conf();
	bool context_exists(std::wstring key);
	bool create_context(std::wstring key);
}
