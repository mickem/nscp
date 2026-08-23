// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "settings_manager_impl.h"

#include <settings/impl/settings_dummy.hpp>
#include <settings/impl/settings_http.hpp>
#include <settings/impl/settings_ini.hpp>
#ifdef WIN32
#include <settings/impl/settings_old.hpp>
#include <settings/impl/settings_registry.hpp>
#endif

#include <config.h>

#include <atomic>
#include <file_helpers.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscp/path_defaults.hpp>
#include <settings/client/settings_proxy.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <utility>

static settings_manager::NSCSettingsImpl *settings_impl = nullptr;

namespace {
// boot.ini entries are echoed to the log on every boot, and a settings url is
// free to carry a credential in its parameters (".../cfg.php?token=..."). The
// string-level counterpart of net::url::to_log_safe_string(): identifying the
// source does not need the query, so drop it rather than write it to disk in
// clear text. Only urls that actually have a query are touched, so a plain
// context key ("master", an ini path, a registry root) is logged verbatim.
std::string to_log_safe_context(const std::string &key) {
  const std::string::size_type pos = key.find('?');
  if (pos == std::string::npos) return key;
  return key.substr(0, pos);
}
}  // namespace

namespace settings_manager {
// Alias to make handling "compatible" with old syntax

inline NSCSettingsImpl *internal_get() {
  if (settings_impl == nullptr) throw settings::settings_exception(__FILE__, __LINE__, "Settings has not been initiated!");
  return settings_impl;
}
std::shared_ptr<nscapi::settings_helper::settings_impl_interface> get_proxy() {
  return std::shared_ptr<nscapi::settings_helper::settings_impl_interface>(new settings_client::settings_proxy(internal_get()));
}
settings::instance_ptr get_settings() { return internal_get()->get(); }
settings::instance_ptr get_settings_no_wait() { return internal_get()->get_no_wait(); }
settings::settings_core *get_core() { return internal_get(); }
void destroy_settings() {
  settings_manager::NSCSettingsImpl *old = settings_impl;
  settings_impl = nullptr;
  delete old;
}

std::string NSCSettingsImpl::find_file(std::string file, std::string fallback) {
  // @todo: replace this with a proper parser!
  if (file.size() == 0) file = fallback;
  return provider_->expand_path(file);
}
std::string NSCSettingsImpl::expand_path(std::string file) { return provider_->expand_path(file); }

std::string NSCSettingsImpl::expand_context_alias(const std::string &key) {
#ifdef WIN32
  if (key == "old") return DEFAULT_CONF_OLD_LOCATION;
  if (key == "registry" || key == "reg") return DEFAULT_CONF_REG_LOCATION;
#endif
  if (key == "ini") return DEFAULT_CONF_INI_LOCATION;
  if (key == "dummy") return "dummy://";
  return key;
}

std::string NSCSettingsImpl::expand_context(const std::string &key) {
  // Host name placeholders, so an included file can be per host (issue #458):
  //
  //   [/includes]
  //   client = ${host}-nsclient.ini
  //
  // This is the same expansion http(s) settings urls take, and it happens here
  // rather than in the ini backend so every store which chains children -
  // [/includes] in an ini file or in the registry, and the [settings] entries
  // in boot.ini - resolves a context the same way. The stored string keeps its
  // placeholder: only the context we are about to open is expanded.
  //
  // Note this deliberately does not go through expand_hostname: its "auto"
  // shorthands would rewrite a context which merely happens to be named auto.
  // And it is the sanitizing _in_path variant, since a context names a file
  // this process opens: the host name is not fully under the operator's
  // control, and must not be able to smuggle a separator or ".." into it.
  //
  // Anything which *stores* a context (set_primary rewriting boot.ini) must use
  // expand_context_alias instead, or the placeholder is replaced by this host's
  // name in the file - the opposite of what a fleet-wide boot.ini is for.
  return socket_helpers::expand_hostname_placeholders_in_path(expand_context_alias(key));
}

//////////////////////////////////////////////////////////////////////////
/// Create an instance of a given type.
/// Used internally to create instances of various settings types.
///
/// @param type the type to create
/// @param context the context to use
/// @return a new instance of given type.
settings::instance_raw_ptr NSCSettingsImpl::create_instance(std::string alias, std::string key) {
  key = expand_context(key);
  net::url url = net::parse(key);
  get_logger()->debug("settings", __FILE__, __LINE__, "Creating instance for: " + url.to_log_safe_string());
#ifdef WIN32
  if (url.protocol == "old") return settings::instance_raw_ptr(new settings::OLDSettings(this, alias, key));
  if (url.protocol == "registry") return settings::instance_raw_ptr(new settings::REGSettings(this, alias, key));
#endif
  if (url.protocol == "ini") return settings::instance_raw_ptr(new settings::INISettings(this, alias, key));
  if (url.protocol == "dummy") return settings::instance_raw_ptr(new settings::settings_dummy(this, alias, key));
  if (url.protocol == "http" || url.protocol == "https") return settings::instance_raw_ptr(new settings::settings_http(this, alias, key));

  if (settings::INISettings::context_exists(this, key)) return settings::instance_raw_ptr(new settings::INISettings(this, alias, key));
  if (settings::INISettings::context_exists(this, DEFAULT_CONF_INI_BASE + key))
    return settings::instance_raw_ptr(new settings::INISettings(this, alias, DEFAULT_CONF_INI_BASE + key));
  // boot() logs what this throws, so keep the query out of it as well.
  throw settings::settings_exception(__FILE__, __LINE__, "Undefined settings protocol: " + url.protocol + ", key=" + to_log_safe_context(key));
}

bool NSCSettingsImpl::supports_edit(const std::string key) {
  if (key.empty()) {
    return true;
  }
  net::url url = net::parse(expand_context(key));
#ifdef WIN32
  if (url.protocol == "old") return false;
  if (url.protocol == "registry") return true;
#endif
  if (url.protocol == "ini") return true;
  if (url.protocol == "dummy") return false;
  if (url.protocol == "http" || url.protocol == "https") return false;
  return false;
}
bool NSCSettingsImpl::context_exists(std::string key) {
  key = expand_context(key);
  net::url url = net::parse(key);
#ifdef WIN32
  if (url.protocol == "old") return settings::OLDSettings::context_exists(this, key);
  if (url.protocol == "registry") return settings::REGSettings::context_exists(this, key);
#endif
  if (url.protocol == "ini") return settings::INISettings::context_exists(this, key);
  if (url.protocol == "dummy") return true;
  if (url.protocol == "http" || url.protocol == "https") return true;
  if (settings::INISettings::context_exists(this, key)) return true;
  if (settings::INISettings::context_exists(this, DEFAULT_CONF_INI_BASE + key)) return true;
  return false;
}

bool NSCSettingsImpl::has_boot_conf() { return boost::filesystem::is_regular_file(boot_); }
void NSCSettingsImpl::write_boot_ini_key(std::string section, std::string key, std::string value) {
  std::list<std::string> order;
  CSimpleIni boot_conf;
  boot_conf.LoadFile(boot_.string().c_str());
  boot_conf.SetValue(utf8::cvt<std::wstring>(section).c_str(), utf8::cvt<std::wstring>(key).c_str(), utf8::cvt<std::wstring>(value).c_str());
  if (boot_conf.SaveFile(boot_.string().c_str()) < 0) {
    get_logger()->error("settings", __FILE__, __LINE__, "Failed to write boot.ini: " + boot_.string());
    throw settings::settings_exception(__FILE__, __LINE__, "Failed to write boot.ini: " + boot_.string());
  }
}

//////////////////////////////////////////////////////////////////////////
/// Boot the settings subsystem from the given file (boot.ini).
///
/// @param file the file to use when booting.
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
      if (!v.empty()) order.push_back(expand_context(v));
    }
    tls_version_ = utf8::cvt<std::string>(boot_conf.GetValue(L"tls", L"version", utf8::cvt<std::wstring>(tls_version_).c_str()));
    tls_verify_mode_ = utf8::cvt<std::string>(boot_conf.GetValue(L"tls", L"verify mode", utf8::cvt<std::wstring>(tls_verify_mode_).c_str()));
    tls_ca_ = utf8::cvt<std::string>(boot_conf.GetValue(L"tls", L"ca", utf8::cvt<std::wstring>(tls_ca_).c_str()));
    proxy_url_ = utf8::cvt<std::string>(boot_conf.GetValue(L"proxy", L"url", L""));
    no_proxy_ = utf8::cvt<std::string>(boot_conf.GetValue(L"proxy", L"no_proxy", L""));

    // [layout] selects the on-disk layout, and has to be applied before the
    // [paths] overrides below: it changes what ${shared-path} defaults to, and
    // an explicit override of one folder should win over that default rather
    // than race it.
    const std::string layout_mode = utf8::cvt<std::string>(boot_conf.GetValue(L"layout", L"mode", L""));
    if (!nscp::paths::is_known_layout(layout_mode)) {
      // Do not guess. Falling back to the layout the host already has is the
      // only safe reading of a mode we do not understand.
      get_logger()->warning("settings", __FILE__, __LINE__,
                            "Unknown [layout] mode '" + layout_mode + "' in " + boot_.string() + "; keeping the legacy layout.");
    }
    provider_->apply_layout(layout_mode);

    // [paths] overrides. Applied before opening the main settings store so
    // they take effect for the main INI's own location lookup. Boot.ini's
    // own location was resolved above with defaults only - that
    // chicken-and-egg is by design.
    std::map<std::string, std::string> path_overrides;
    CSimpleIni::TNamesDepend path_keys;
    if (boot_conf.GetAllKeys(L"paths", path_keys)) {
      for (const auto &k : path_keys) {
        const std::string okey = utf8::cvt<std::string>(k.pItem);
        const std::string val = utf8::cvt<std::string>(boot_conf.GetValue(L"paths", k.pItem, L""));
        if (!val.empty()) path_overrides[okey] = val;
      }
    }
    if (!path_overrides.empty()) {
      get_logger()->debug("settings", __FILE__, __LINE__, "Applying " + str::xtos(path_overrides.size()) + " path override(s) from boot.ini");
      provider_->apply_path_overrides(std::move(path_overrides));
    }
  }
  // The folder everything below writes into has to exist, and be locked down,
  // before the first write - which is the trust store export immediately after
  // this. Runs after the [paths] overrides so it acts on the final answer.
  provider_->prepare_shared_folder();

  // Everything below opens the master settings store, and for an http(s)://
  // source that means an immediate network fetch. Give the provider its chance
  // to lay down the trust material that fetch verifies against first - after
  // the [paths] overrides above, so it lands where the operator asked.
  provider_->prepare_trust_store();

  if (order.size() == 0) {
    get_logger()->debug("settings", __FILE__, __LINE__, "No entries found looking in (adding default): " + boot_.string());
#ifdef WIN32
    order.push_back(DEFAULT_CONF_OLD_LOCATION);
#endif
    order.push_back(DEFAULT_CONF_INI_LOCATION);
  }
  std::string boot_order;
  for (const std::string &k : order) {
    str::format::append_list(boot_order, to_log_safe_context(k), ", ");
  }
  for (std::string k : order) {
    if (context_exists(k)) {
      get_logger()->debug("settings", __FILE__, __LINE__, "Activating: " + to_log_safe_context(k));
      try {
        set_instance("master", k);
        return;
      } catch (const settings::settings_exception &e) {
        get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
      } catch (const std::exception &e) {
        get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
      } catch (...) {
        get_logger()->error("settings", __FILE__, __LINE__, "Failed to activate: " + to_log_safe_context(key));
      }
    }
  }
  if (!key.empty()) {
    get_logger()->info("settings", __FILE__, __LINE__, "No valid settings found but one was given (using that): " + to_log_safe_context(key));
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
  // Every entry read here is written back below, so only the protocol aliases
  // are resolved: a host name placeholder has to survive being reordered, or
  // switching context once would bake this host's name into a boot.ini meant
  // for every machine (issue #458).
  key = expand_context_alias(key);
  for (int i = 0; i < 20; i++) {
    std::string v = utf8::cvt<std::string>(boot_conf.GetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i)).c_str(), L""));
    if (!v.empty()) {
      order.push_back(expand_context_alias(v));
      boot_conf.SetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i)).c_str(), L"");
    }
  }
  order.remove(key);
  order.push_front(key);
  int i = 1;
  for (const std::string &k : order) {
    boot_conf.SetValue(L"settings", utf8::cvt<std::wstring>(str::xtos(i++)).c_str(), utf8::cvt<std::wstring>(k).c_str());
  }
  boot_conf.SaveFile(boot_.string().c_str());
  get_core()->create_instance("master", key)->ensure_exists();
  boot(key);
}

bool NSCSettingsImpl::create_context(const std::string &key) {
  try {
    change_context(key);
  } catch (settings::settings_exception &e) {
    get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
    return false;
  }
  return true;
}

void NSCSettingsImpl::change_context(const std::string &context) {
  try {
    get_core()->migrate_to("master", context);
    set_primary(context);
    get_core()->boot(context);
  } catch (settings::settings_exception &e) {
    get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
  }
}

bool init_settings(provider_interface *provider, const std::string &context) {
  try {
    settings_impl = new NSCSettingsImpl(provider);
    get_core()->set_base(provider->expand_path("${base-path}"));
    get_core()->boot(context);
    get_core()->set_ready();
  } catch (const settings::settings_exception &e) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (const std::exception &e) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
    return false;
  }
  return true;
}

bool init_installer_settings(provider_interface *provider, const std::string &context, std::string tls_version, std::string tls_verify_mode,
                             std::string tls_ca) {
  try {
    settings_impl = new NSCSettingsImpl(provider, std::move(tls_version), std::move(tls_verify_mode), std::move(tls_ca));
    get_core()->set_base(provider->expand_path("${base-path}"));
    get_core()->boot(context);
    get_core()->set_ready();
  } catch (const settings::settings_exception &e) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (const std::exception &e) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "Failed to initialize settings: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    get_core()->get_logger()->error("settings", __FILE__, __LINE__, "FATAL ERROR IN SETTINGS SUBSYSTEM");
    return false;
  }
  return true;
}

void change_context(const std::string &context) { internal_get()->change_context(context); }
void set_boot_ini_primary(const std::string &context) { internal_get()->set_primary(context); }

bool has_boot_conf() { return internal_get()->has_boot_conf(); }
void write_boot_ini_key(std::string section, std::string key, std::string value) { return internal_get()->write_boot_ini_key(section, key, value); }
bool context_exists(const std::string &key) { return internal_get()->context_exists(key); }
bool create_context(std::string key) { return internal_get()->create_context(key); }

bool has_local_configuration() {
  // Last answer we were actually able to work out.
  //
  // Both lookups below can fail for reasons that say nothing about the
  // configuration: get_no_wait() try-locks and throws when the settings
  // instance is busy, and get_local_sections takes a five-second timed lock
  // that throws on timeout. This is called from the fleet sync thread, which
  // also asks for the reloads that hold those locks - so "could not tell right
  // now" is a normal outcome, and answering `false` for it would tell the fleet
  // server this host has no local overrides when it may well have.
  //
  // Whether a host has local configuration changes only when somebody edits it,
  // so the previous answer is a far better guess than a default.
  static std::atomic<bool> last_known(false);
  try {
    const settings::instance_ptr settings = get_settings_no_wait();
    if (!settings) return last_known.load();
    // Root sections of THIS store only - get_sections would fold in the
    // fleet-managed include and answer "yes" for every enrolled host.
    bool local = false;
    for (const std::string &section : settings->get_local_sections("")) {
      // The include itself is how the fleet configuration arrives, not local
      // configuration that competes with it.
      if (section != "/includes") {
        local = true;
        break;
      }
    }
    last_known.store(local);
    return local;
  } catch (const std::exception &) {
    // Never let a probe of the configuration break the caller - enrollment and
    // the state report both carry on - but do not invent an answer either.
    return last_known.load();
  } catch (...) {
    return last_known.load();
  }
}

void ensure_exists() {}
}  // namespace settings_manager
