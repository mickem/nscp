#include "StdAfx.h"

#include "settings_manager_impl.h"

#include <settings/settings_ini.hpp>
#include <settings/settings_http.hpp>
#ifdef WIN32
#include <settings/settings_old.hpp>
#include <settings/settings_registry.hpp>
#endif

#include <settings/client/settings_proxy.hpp>
#include <settings/client/settings_client.hpp>
#include <file_helpers.hpp>
#include <config.h>


static settings_manager::NSCSettingsImpl* settings_impl = NULL;

namespace settings_manager {
	// Alias to make handling "compatible" with old syntax

	inline NSCSettingsImpl* internal_get() {
		if (settings_impl == NULL)
			throw "Settings has not been initiated!";
		return settings_impl;
	}
	nscapi::settings_helper::settings_impl_interface_ptr get_proxy() {
		return nscapi::settings_helper::settings_impl_interface_ptr(new settings_client::settings_proxy(internal_get()));
	}
	settings::instance_ptr get_settings() {
		return internal_get()->get();
	}
	settings::instance_ptr get_settings_no_wait() {
		return internal_get()->get_no_wait();
	}
	settings::settings_core* get_core() {
		return internal_get();
	}
	void destroy_settings() {
		settings_manager::NSCSettingsImpl* old = settings_impl;
		settings_impl = NULL;
		delete old;
	}

	std::wstring NSCSettingsImpl::find_file(std::wstring file, std::wstring fallback) {
		// @todo: replace this with a proper parser!
		if (file.size() == 0)
			file = fallback;
		return provider_->expand_path(file);
	}
	std::wstring NSCSettingsImpl::expand_path(std::wstring file) {
		return provider_->expand_path(file);
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
		net::wurl url = net::parse(key);
		get_logger()->debug(__FILE__, __LINE__, _T("Creating instance for: ") + url.to_string());
		if (url.host.empty() && url.path.empty()) 
			key = _T("");
#ifdef WIN32
		if (url.protocol == _T("old")) {
			old_ = true;
			if (key.empty())
				key = DEFAULT_CONF_OLD_LOCATION;
			return settings::instance_raw_ptr(new settings::OLDSettings(this, key));
		}
		if (url.protocol == _T("registry")) {
			if (key.empty())
				key = DEFAULT_CONF_REG_LOCATION;
			return settings::instance_raw_ptr(new settings::REGSettings(this, key));
		}
#endif
		if (url.protocol == _T("ini")) {
			if (key.empty())
				key = DEFAULT_CONF_INI_LOCATION;
			return settings::instance_raw_ptr(new settings::INISettings(this, key));
		}
		if (url.protocol == _T("http")) {
			return settings::instance_raw_ptr(new settings::settings_http(this, key));
		}
		throw settings::settings_exception(_T("Undefined settings protocol: ") + url.protocol);
	}

	bool NSCSettingsImpl::check_file(std::wstring file, std::wstring tag, std::wstring &key) {
		std::wstring tmp = provider_->expand_path(file);
		if (file_helpers::checks::exists(tmp)) {
			key = tag;
			return true;
		}
		return false;
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
			boot_ = provider_->expand_path(BOOT_CONF_LOCATION);
			get_logger()->debug(__FILE__, __LINE__, _T("---> ") + boot_.string());
			if (file_helpers::checks::exists(boot_.string())) {
				key = get_boot_string(_T("settings"), _T("location"), DEFAULT_CONF_LOCATION);
				get_logger()->debug(__FILE__, __LINE__, _T("---> ") + key);
			} else {
				if (!check_file(DEFAULT_CONF_OLD_LOCATION, _T("old"), key))
					if (!check_file(DEFAULT_CONF_INI_LOCATION, _T("ini"), key))
						key = DEFAULT_CONF_LOCATION;
			}
		}
		set_instance(key);
	}

	void NSCSettingsImpl::change_context(std::wstring context) {
		try {
			get_core()->migrate_to(context);
			set_boot_string(_T("settings"), _T("location"), context);
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			provider_->log_fatal_error(_T("Failed to initialize settings: ") + e.getError());
		} catch (...) {
			provider_->log_fatal_error(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
		}
	}

	bool init_settings(provider_interface *provider, std::wstring context) {
		try {
			settings_impl = new NSCSettingsImpl(provider);
			get_core()->set_logger(provider->create_logger());
			get_core()->set_base(provider->expand_path(_T("${base-path}")));
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			provider->log_fatal_error(_T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			provider->log_fatal_error(_T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}

	void change_context(std::wstring context) {
		settings_impl->change_context(context);
	}

}
