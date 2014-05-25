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
			throw settings::settings_exception("Settings has not been initiated!");
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

	std::string NSCSettingsImpl::find_file(std::string file, std::string fallback) {
		// @todo: replace this with a proper parser!
		if (file.size() == 0)
			file = fallback;
		return provider_->expand_path(file);
	}
	std::string NSCSettingsImpl::expand_path(std::string file) {
		return provider_->expand_path(file);
	}


	std::string NSCSettingsImpl::expand_context(const std::string &key) {
#ifdef WIN32
		if (key == "old")
			return DEFAULT_CONF_OLD_LOCATION;
		if (key == "registry" || key == "reg")
			return DEFAULT_CONF_REG_LOCATION;
#endif
		if (key == "ini")
			return DEFAULT_CONF_INI_LOCATION;
		if (key == "dummy")
			return "dummy://";
		return key;
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
	settings::instance_raw_ptr NSCSettingsImpl::create_instance(std::string key) {
		key = expand_context(key);
		net::url url = net::parse(key);
		get_logger()->debug("settings", __FILE__, __LINE__, "Creating instance for: " + url.to_string());
#ifdef WIN32
		if (url.protocol == "old")
			return settings::instance_raw_ptr(new settings::OLDSettings(this, key));
		if (url.protocol == "registry")
			return settings::instance_raw_ptr(new settings::REGSettings(this, key));
#endif
		if (url.protocol == "ini")
			return settings::instance_raw_ptr(new settings::INISettings(this, key));
		if (url.protocol == "dummy")
			return settings::instance_raw_ptr(new settings::settings_dummy(this, key));
		if (url.protocol == "http")
			return settings::instance_raw_ptr(new settings::settings_http(this, key));

		if (settings::INISettings::context_exists(this, key))
			return settings::instance_raw_ptr(new settings::INISettings(this, key));
		if (settings::INISettings::context_exists(this, DEFAULT_CONF_INI_BASE + key))
			return settings::instance_raw_ptr(new settings::INISettings(this, DEFAULT_CONF_INI_BASE + key));
		throw settings::settings_exception("Undefined settings protocol: " + url.protocol);
	}


	bool NSCSettingsImpl::context_exists(std::string key) {
		key = expand_context(key);
		net::url url = net::parse(key);
#ifdef WIN32
		if (url.protocol == "old")
			return settings::OLDSettings::context_exists(this, key);
		if (url.protocol == "registry")
			return settings::REGSettings::context_exists(this, key);
#endif
		if (url.protocol == "ini")
			return settings::INISettings::context_exists(this, key);
		if (url.protocol == "dummy")
			return true;
		if (url.protocol == "http")
			return true;
		if (settings::INISettings::context_exists(this, key))
			return true;
		if (settings::INISettings::context_exists(this, DEFAULT_CONF_INI_BASE + key))
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
	void NSCSettingsImpl::boot(std::string key) {
		std::list<std::string> order;
		if (!key.empty()) {
			order.push_back(key);
		} 
		boot_ = provider_->expand_path(BOOT_CONF_LOCATION);
		if (boost::filesystem::is_regular_file(boot_)) {
			get_logger()->debug("settings", __FILE__, __LINE__, "Boot.ini found in: " + boot_.string());
			for (int i=0;i<20;i++) {
				std::string v = get_boot_string("settings", strEx::s::xtos(i), "");
				if (!v.empty()) 
					order.push_back(expand_context(v));
			}
		}
		if (order.size() == 0) {
			get_logger()->debug("settings", __FILE__, __LINE__, "No entries found looking in (adding default): " + boot_.string());
			order.push_back(DEFAULT_CONF_OLD_LOCATION);
			order.push_back(DEFAULT_CONF_INI_LOCATION);
		}
		std::string boot_order;
		BOOST_FOREACH(const std::string &k, order) {
			strEx::append_list(boot_order, k, ", ");
		}
		BOOST_FOREACH(std::string k, order) {
			if (context_exists(k)) {
				get_logger()->debug("settings", __FILE__, __LINE__, "Activating: " + k);
				set_instance(k);
				return;
			}
		}
		if (!key.empty()) {
			get_logger()->info("settings", __FILE__, __LINE__, "No valid settings found but one was given (using that): " + key);
			set_instance(key);
			return;
		}

		get_logger()->debug("settings", __FILE__, __LINE__, "No valid settings found (tried): " + boot_order);

		std::string tgt = get_boot_string("main", "write", "");
		if (!tgt.empty()) {
			get_logger()->debug("settings", __FILE__, __LINE__, "Creating new settings file: " + tgt);
			set_instance(tgt);
			return;
		}
		get_logger()->error("settings", __FILE__, __LINE__, "Settings contexts exhausted, will create a new default context: " DEFAULT_CONF_INI_LOCATION);
		set_instance(DEFAULT_CONF_INI_LOCATION);
	}

	void NSCSettingsImpl::set_primary(std::string key) {
		std::list<std::string> order;
		for (int i=0;i<20;i++) {
			std::string v = get_boot_string("settings", strEx::s::xtos(i), "");
			if (!v.empty()) {
				order.push_back(expand_context(v));
				set_boot_string("settings", strEx::s::xtos(i), "");
			}
		}
		order.remove(key);
		order.push_front(key);
		int i=1;
		BOOST_FOREACH(const std::string &k, order) {
			set_boot_string("settings", strEx::s::xtos(i++), k);
		}
		get_core()->create_instance(key)->ensure_exists();
		set_boot_string("main", "write", key);
		boot(key);
	}


	bool NSCSettingsImpl::create_context(std::string key) {
		try {
			change_context(key);
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
			return false;
		} catch (...) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
			return false;
		}
		return true;
	}

	void NSCSettingsImpl::change_context(std::string context) {
		try {
			get_core()->migrate_to(context);
			set_primary(context);
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
		} catch (...) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
		}
	}

	bool init_settings(provider_interface *provider, std::string context) {
		try {
			settings_impl = new NSCSettingsImpl(provider);
			get_core()->set_base(provider->expand_path("${base-path}"));
			get_core()->boot(context);
			get_core()->set_ready();
		} catch (settings::settings_exception e) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
			return false;
		} catch (...) {
			nsclient::logging::logger::get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
			return false;
		}
		return true;
	}

	void change_context(std::string context) {
		settings_impl->change_context(context);
	}

	bool has_boot_conf() {
		return settings_impl->has_boot_conf();
	}
	bool context_exists(std::string key) {
		return settings_impl->context_exists(key);
	}
	bool create_context(std::string key) {
		return settings_impl->create_context(key);
	}
	void ensure_exists() {
	}

}
