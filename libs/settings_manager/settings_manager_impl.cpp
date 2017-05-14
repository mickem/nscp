#include "settings_manager_impl.h"

#include <settings/impl/settings_ini.hpp>
#include <settings/impl/settings_dummy.hpp>
#include <settings/impl/settings_http.hpp>
#ifdef WIN32
#include <settings/impl/settings_old.hpp>
#include <settings/impl/settings_registry.hpp>
#endif

#include <settings/client/settings_proxy.hpp>
#include <file_helpers.hpp>
#include <config.h>

#include <str/xtos.hpp>
#include <str/format.hpp>
#include <utf8.hpp>

static settings_manager::NSCSettingsImpl* settings_impl = NULL;

namespace settings_manager {
	// Alias to make handling "compatible" with old syntax

	inline NSCSettingsImpl* internal_get() {
		if (settings_impl == NULL)
			throw settings::settings_exception(__FILE__, __LINE__, "Settings has not been initiated!");
		return settings_impl;
	}
	boost::shared_ptr<nscapi::settings_helper::settings_impl_interface> get_proxy() {
		return boost::shared_ptr<nscapi::settings_helper::settings_impl_interface>(new settings_client::settings_proxy(internal_get()));
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
	settings::instance_raw_ptr NSCSettingsImpl::create_instance(std::string alias, std::string key) {
		key = expand_context(key);
		net::url url = net::parse(key);
		get_logger()->debug("settings", __FILE__, __LINE__, "Creating instance for: " + url.to_string());
#ifdef WIN32
		if (url.protocol == "old")
			return settings::instance_raw_ptr(new settings::OLDSettings(this, alias, key));
		if (url.protocol == "registry")
			return settings::instance_raw_ptr(new settings::REGSettings(this, alias, key));
#endif
		if (url.protocol == "ini")
			return settings::instance_raw_ptr(new settings::INISettings(this, alias, key));
		if (url.protocol == "dummy")
			return settings::instance_raw_ptr(new settings::settings_dummy(this, alias, key));
		if (url.protocol == "http" || url.protocol == "https")
			return settings::instance_raw_ptr(new settings::settings_http(this, alias, key));

		if (settings::INISettings::context_exists(this, key))
			return settings::instance_raw_ptr(new settings::INISettings(this, alias, key));
		if (settings::INISettings::context_exists(this, DEFAULT_CONF_INI_BASE + key))
			return settings::instance_raw_ptr(new settings::INISettings(this,alias,  DEFAULT_CONF_INI_BASE + key));
		throw settings::settings_exception(__FILE__, __LINE__, "Undefined settings protocol: " + url.protocol);
	}

	bool NSCSettingsImpl::supports_edit(const std::string key) {
		if (key.empty()) {
			return true;
		}
		net::url url = net::parse(expand_context(key));
#ifdef WIN32
		if (url.protocol == "old")
			return false;
		if (url.protocol == "registry")
			return true;
#endif
		if (url.protocol == "ini")
			return true;
		if (url.protocol == "dummy")
			return false;
		if (url.protocol == "http" || url.protocol == "https")
			return true;
		return false;
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
		if (url.protocol == "http" || url.protocol == "https")
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
			CSimpleIni boot_conf;
			boot_conf.LoadFile(boot_.string().c_str());
			get_logger()->debug("settings", __FILE__, __LINE__, "Boot.ini found in: " + boot_.string());
			for (int i = 0; i < 20; i++) {
				std::string v = utf8::cvt<std::string>(boot_conf.GetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i)).c_str(), L""));
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
			str::format::append_list(boot_order, k, ", ");
		}
		BOOST_FOREACH(std::string k, order) {
			if (context_exists(k)) {
				get_logger()->debug("settings", __FILE__, __LINE__, "Activating: " + k);
				try {
					set_instance("master", k);
					return;
				} catch (const settings::settings_exception &e) {
					get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
				} catch (const std::exception &e) {
					get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
				} catch (...) {
					get_logger()->error("settings", __FILE__, __LINE__, "Failed to activate: " + key);
				}
			}
		}
		if (!key.empty()) {
			get_logger()->info("settings", __FILE__, __LINE__, "No valid settings found but one was given (using that): " + key);
			set_instance("master", key);
			return;
		}

		get_logger()->debug("settings", __FILE__, __LINE__, "No valid settings found (tried): " + boot_order);

		get_logger()->info("settings", __FILE__, __LINE__, "Creating new settings file: " DEFAULT_CONF_INI_LOCATION);
		set_instance("master", DEFAULT_CONF_INI_LOCATION);
	}

	void NSCSettingsImpl::set_primary(std::string key) {
		std::list<std::string> order;
		CSimpleIni boot_conf;
		boot_conf.LoadFile(boot_.string().c_str());
		for (int i = 0; i < 20; i++) {
			std::string v = utf8::cvt<std::string>(boot_conf.GetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i)).c_str(), L""));
			if (!v.empty()) {
				order.push_back(expand_context(v));
				boot_conf.SetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i)).c_str(), L"");
			}
		}
		order.remove(key);
		order.push_front(key);
		int i = 1;
		BOOST_FOREACH(const std::string &k, order) {
			boot_conf.SetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i++)).c_str(), utf8::cvt<std::wstring>(k).c_str());
		}
		boot_conf.SaveFile(boot_.string().c_str());
		get_core()->create_instance("master", key)->ensure_exists();
		boot(key);
	}

	bool NSCSettingsImpl::create_context(std::string key) {
		try {
			change_context(key);
		} catch (settings::settings_exception e) {
			get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
			return false;
		} catch (...) {
			get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
			return false;
		}
		return true;
	}

	void NSCSettingsImpl::change_context(std::string context) {
		try {
			get_core()->migrate_to("master", context);
			set_primary(context);
			get_core()->boot(context);
		} catch (settings::settings_exception e) {
			get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
		} catch (...) {
			get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
		}
	}

	bool init_settings(provider_interface *provider, std::string context) {
		try {

			settings_impl = new NSCSettingsImpl(provider);
			get_core()->set_base(provider->expand_path("${base-path}"));
			get_core()->boot(context);
			get_core()->set_ready();
		} catch (const settings::settings_exception &e) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
			return false;
		} catch (const std::exception &e) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
			return false;
		} catch (...) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
			return false;
		}
		return true;
	}

	bool init_installer_settings(provider_interface *provider, std::string context) {
		try {
			settings_impl = new NSCSettingsImpl(provider);
			get_core()->set_base(provider->expand_path("${base-path}"));
			if (settings_impl->supports_edit(context)) {
				get_core()->boot(context);
				get_core()->set_ready();
				return true;
			}
		} catch (const settings::settings_exception &e) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + e.reason());
			return false;
		} catch (const std::exception &e) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
			return false;
		} catch (...) {
			get_core()->get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYTEM");
			return false;
		}
		return true;
	}

	void change_context(std::string context) {
		internal_get()->change_context(context);
	}

	bool has_boot_conf() {
		return internal_get()->has_boot_conf();
	}
	bool context_exists(std::string key) {
		return internal_get()->context_exists(key);
	}
	bool create_context(std::string key) {
		return internal_get()->create_context(key);
	}
	void ensure_exists() {}
}