#include "StdAfx.h"

#include "settings_manager_impl.h"

#include <settings/impl/settings_ini.hpp>
#include <settings/impl/settings_dummy.hpp>
#include <settings/impl/settings_http.hpp>
#ifdef WIN32
#include <settings/impl/settings_old.hpp>
#include <settings/impl/settings_registry.hpp>
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
			throw settings::settings_exception(_T("Settings has not been initiated!"));
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
		if (url.protocol.empty()) {
			url = net::parse(key + _T("://"));
			get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("No driver specified attemtping to fake one: ") + url.to_string());
		}
		get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("Creating instance for: ") + url.to_string());
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
		if (url.protocol == _T("dummy")) {
			return settings::instance_raw_ptr(new settings::settings_dummy(this, key));
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

	std::wstring NSCSettingsImpl::expand_context(std::wstring key) {
#ifdef WIN32
		if (key == _T("old"))
			return DEFAULT_CONF_OLD_LOCATION;
		if (key == _T("registry"))
			return DEFAULT_CONF_REG_LOCATION;
#endif
		if (key == _T("ini"))
			return DEFAULT_CONF_INI_LOCATION;
		if (key == _T("dummy"))
			return _T("dummy://");
		return key;
	}

	bool NSCSettingsImpl::context_exists(std::wstring key) {
		net::wurl url = net::parse(key);
		if (url.protocol.empty())
			url = net::parse(expand_context(key));
#ifdef WIN32
		if (url.protocol == _T("old"))
			return settings::OLDSettings::context_exists(this, key);
		if (url.protocol == _T("registry"))
			return settings::REGSettings::context_exists(this, key);
#endif
		if (url.protocol == _T("ini"))
			return settings::INISettings::context_exists(this, key);
		if (url.protocol == _T("dummy"))
			return true;
		if (url.protocol == _T("http"))
			return true;
		return false;
	}

	bool NSCSettingsImpl::has_boot_conf() {
		return boost::filesystem::is_regular_file(boot_);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Boot the settings subsystem from the given file (boot.ini).
	///
	/// @param file the file to use when booting.
	///
	/// @author mickem
	void NSCSettingsImpl::boot(std::wstring key) {
		std::list<std::wstring> order;
		if (!key.empty()) {
			order.push_back(key);
		} 
		boot_ = utf8::cvt<std::string>(provider_->expand_path(BOOT_CONF_LOCATION));
		if (boost::filesystem::is_regular_file(boot_)) {
			get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("Boot.ini found in: ") + utf8::cvt<std::wstring>(boot_.string()));
			for (int i=0;i<20;i++) {
				std::wstring v = get_boot_string(_T("settings"), strEx::itos(i), _T(""));
				if (!v.empty()) 
					order.push_back(expand_context(v));
			}
		}
		if (order.size() == 0) {
			get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("No entries found looking in (adding default): ") + utf8::cvt<std::wstring>(boot_.string()));
			order.push_back(DEFAULT_CONF_OLD_LOCATION);
			order.push_back(DEFAULT_CONF_INI_LOCATION);
		}
		std::wstring boot_order;
		BOOST_FOREACH(std::wstring k, order) {
			strEx::append_list(boot_order, k, _T(", "));
		}
		get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("Boot order: ") + boot_order);
		BOOST_FOREACH(std::wstring k, order) {
			if (context_exists(k)) {
				get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("Activating: ") + k);
				set_instance(k);
				return;
			}
		}
		if (!key.empty()) {
			get_logger()->info(_T("settings"), __FILE__, __LINE__, _T("No valid settings found but one was given (using that): ") + key);
			set_instance(key);
			return;
		}

		get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("No valid settings found (tried): ") + boot_order);

		std::wstring tgt = get_boot_string(_T("main"), _T("write"), _T(""));
		if (!tgt.empty()) {
			get_logger()->debug(_T("settings"), __FILE__, __LINE__, _T("Creating new settings file: ") + tgt);
			set_instance(tgt);
			return;
		}
		get_logger()->error(_T("settings"), __FILE__, __LINE__, std::wstring(_T("Settings contexts exausted, will create a new ")) + DEFAULT_CONF_INI_LOCATION);
		set_instance(DEFAULT_CONF_INI_LOCATION);
	}

	void NSCSettingsImpl::set_primary(std::wstring key) {
		std::list<std::wstring> order;
		for (int i=0;i<20;i++) {
			std::wstring v = get_boot_string(_T("settings"), strEx::itos(i), _T(""));
			if (!v.empty()) {
				order.push_back(expand_context(v));
				set_boot_string(_T("settings"), strEx::itos(i), _T(""));
			}
		}
		order.remove(key);
		order.push_front(key);
		int i=1;
		BOOST_FOREACH(std::wstring k, order) {
			set_boot_string(_T("settings"), strEx::itos(i++), k);
		}
		set_boot_string(_T("main"), _T("write"), key);
		boot(key);
	}


	bool NSCSettingsImpl::create_context(std::wstring key) {
		try {
			change_context(key);
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}

	void NSCSettingsImpl::change_context(std::wstring context) {
		try {
			get_core()->migrate_to(context);
			set_primary(context);
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("Failed to initialize settings: ") + e.getError());
		} catch (...) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("FATAL ERROR IN SETTINGS SUBSYTEM"));
		}
	}

	bool init_settings(provider_interface *provider, std::wstring context) {
		try {
			settings_impl = new NSCSettingsImpl(provider);
			get_core()->set_base(utf8::cvt<std::string>(provider->expand_path(_T("${base-path}"))));
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("Failed to initialize settings: ") + e.getError());
			return false;
		} catch (...) {
			nsclient::logging::logger::get_logger()->error(_T("settings"), __FILE__, __LINE__, _T("FATAL ERROR IN SETTINGS SUBSYTEM"));
			return false;
		}
		return true;
	}

	void change_context(std::wstring context) {
		settings_impl->change_context(context);
	}

	bool has_boot_conf() {
		return settings_impl->has_boot_conf();
	}
	bool context_exists(std::wstring key) {
		return settings_impl->context_exists(key);
	}
	bool create_context(std::wstring key) {
		return settings_impl->create_context(key);
	}


}
