#pragma once

#include <boost/shared_ptr.hpp>

#include <settings/settings_core.hpp>
#include <settings/client/settings_client_interface.hpp>
#include "settings_handler_impl.hpp"

namespace settings_manager {
	struct provider_interface {
		virtual std::string expand_path(std::string file) = 0;
		virtual std::string get_data(std::string key) = 0;
		virtual nsclient::logging::logger_instance get_logger() const = 0;
	};

	class NSCSettingsImpl : public settings::settings_handler_impl {
	private:
		boost::filesystem::path boot_;
		provider_interface *provider_;
	public:
		NSCSettingsImpl(provider_interface *provider) : settings::settings_handler_impl(provider->get_logger()), provider_(provider) {}
		virtual ~NSCSettingsImpl() {}
		//////////////////////////////////////////////////////////////////////////
		/// Get a string form the boot file.
		///
		/// @param section section to read a value from.
		/// @param key the key to read.
		/// @param def a default value.
		/// @return the value of the key or the default value.
		///
		/// @author mickem
		std::string get_boot_string(std::string section, std::string key, std::string def) {
#ifdef WIN32
			wchar_t* buffer = new wchar_t[1024];
			GetPrivateProfileString(utf8::cvt<std::wstring>(section).c_str(), utf8::cvt<std::wstring>(key).c_str(), utf8::cvt<std::wstring>(def).c_str(), buffer, 1023, boot_.wstring().c_str());
			std::string ret = utf8::cvt<std::string>(buffer);
			delete[] buffer;
			if (ret == def) {
				std::string tmp = utf8::cvt<std::string>(provider_->get_data(key));
				if (!tmp.empty())
					return tmp;
				return def;
			}
			return ret;
#else
			return def;
#endif
		}
		void set_boot_string(std::string section, std::string key, std::string val) {
#ifdef WIN32
			WritePrivateProfileString(utf8::cvt<std::wstring>(section).c_str(), utf8::cvt<std::wstring>(key).c_str(), utf8::cvt<std::wstring>(val).c_str(), boot_.wstring().c_str());
#else
#endif
		}

		std::string expand_simple_context(const std::string &key);
		void boot(std::string file);
		std::string find_file(std::string file, std::string fallback = "");
		std::string expand_path(std::string file);
		std::string expand_context(const std::string &key);

		settings::instance_raw_ptr create_instance(std::string key);
		void change_context(std::string file);
		bool context_exists(std::string key);
		bool create_context(std::string key);
		bool has_boot_conf();
		void set_primary(std::string key);
	};

	// Alias to make handling "compatible" with old syntax
	settings::instance_ptr get_settings();
	settings::instance_ptr get_settings_no_wait();
	settings::settings_core* get_core();
	boost::shared_ptr<nscapi::settings_helper::settings_impl_interface>  get_proxy();
	void destroy_settings();
	bool init_settings(provider_interface *provider, std::string context = "");
	void change_context(std::string context);
	bool has_boot_conf();
	bool context_exists(std::string key);
	bool create_context(std::string key);
}
