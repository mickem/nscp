/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "plugin_manager.hpp"

#include <config.h>

#include <boost/unordered_map.hpp>
#include <file_helpers.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <settings/settings_core.hpp>
#include <str/format.hpp>

#include "../../libs/settings_manager/settings_manager_impl.h"
#include "dll_plugin.h"
#include "zip_plugin.h"

bool inline equals_enabled(const std::string &value) { return value == "enabled" || value == "1" || value == "true"; }
bool inline equals_disabled(const std::string &value) { return value == "disabled" || value == "0" || value == "false"; }

namespace {
// Map a module file name to the Windows installer feature that ships it.
// Mirrors installers/installer-NSCP/Product.wxs — when modules move between
// installer features, update this table to match.
//
// Returns an empty string when the module is either built-in / always
// installed, or not recognised (likely a third-party module). The hint is
// only useful on Windows, where users install via the MSI; on Unix this
// returns empty so we don't print misleading text.
std::string installer_feature_hint(const std::string &module) {
#ifndef _WIN32
  (void)module;
  return {};
#else
  // Strip the optional `.dll` extension and any directory prefix so callers
  // can pass either "NRPEServer" or "NRPEServer.dll".
  std::string name = module;
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string::npos) name = name.substr(slash + 1);
  const auto dot = name.rfind('.');
  if (dot != std::string::npos) name = name.substr(0, dot);

  struct entry {
    const char *module;
    const char *feature_title;
  };
  static const entry table[] = {
      // NRPE Support
      {"NRPEServer", "NRPE Support"},
      {"NRPEClient", "NRPE Support"},
      // Check MK Support
      {"CheckMKServer", "Check MK Support"},
      {"CheckMKClient", "Check MK Support"},
      // check_nt support
      {"NSClientServer", "check_nt support"},
      // WEB Server
      {"WEBServer", "WEB Server"},
      // NSCA plugin
      {"NSCAClient", "NSCA plugin"},
      {"NSCAServer", "NSCA plugin"},
      {"Scheduler", "NSCA plugin"},
      // NSCA-NG plugin
      {"NSCANgClient", "NSCA-NG plugin"},
      // Python Scripting
      {"PythonScript", "Python Scripting"},
      // Various client plugins
      {"GraphiteClient", "Various client plugins"},
      {"SMTPClient", "Various client plugins"},
      {"SyslogClient", "Various client plugins"},
      {"NRDPClient", "Various client plugins"},
      {"IcingaClient", "Various client plugins"},
      {"CollectdClient", "Various client plugins"},
      {"NSCPClient", "Various client plugins"},
      // Lua Scripting
      {"LUAScript", "Lua Scripting"},
      // OP5 Monitoring system
      {"Op5Client", "OP5 Monitoring system"},
      // Elastic plugin
      {"ElasticClient", "Elastic plugin"},
      // Check Plugins (the bulk of the check_* modules)
      {"CheckEventLog", "Check Plugins"},
      {"CheckExternalScripts", "Check Plugins"},
      {"CheckHelpers", "Check Plugins"},
      {"CheckSystem", "Check Plugins"},
      {"CheckWMI", "Check Plugins"},
      {"CheckNSCP", "Check Plugins"},
      {"CheckDisk", "Check Plugins"},
      {"CheckTaskSched", "Check Plugins"},
      {"SimpleCache", "Check Plugins"},
      {"SimpleFileWriter", "Check Plugins"},
      {"CheckLogFile", "Check Plugins"},
      {"CheckNet", "Check Plugins"},
  };
  for (const auto &e : table) {
    if (name == e.module) {
      return std::string(" (module '") + name + "' is part of the '" + e.feature_title +
             "' installer feature; re-run the NSClient++ installer and enable that feature, or "
             "see installers/installer-NSCP/Product.wxs for the full feature map)";
    }
  }
  return {};
#endif
}
}  // namespace

struct command_chunk {
  nsclient::commands::plugin_type plugin;
  PB::Commands::QueryRequestMessage request;
};

bool nsclient::core::plugin_manager::contains_plugin(plugin_alias_list_type &ret, std::string alias, std::string plugin) {
  for (auto v : boost::make_iterator_range(ret.equal_range(alias))) {
    if (v.second == plugin) return true;
  }
  return false;
}

nsclient::core::plugin_manager::plugin_manager(path_instance path_, logging::logger_instance log_instance)
    : path_(path_),
      log_instance_(log_instance),
      plugin_list_(log_instance_),
      commands_(log_instance_),
      channels_(log_instance_),
      metrics_fetchers_(log_instance_),
      metrics_submitters_(log_instance_),
      plugin_cache_(log_instance_),
      event_subscribers_(log_instance_) {}

nsclient::core::plugin_manager::~plugin_manager() {}

// Find all plugins on the filesystem
nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_plugins() {
  plugin_alias_list_type ret;

  settings::settings_interface::string_list list = settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION);
  for (std::string plugin : list) {
    std::string alias;
    try {
      alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, plugin, "");
    } catch (settings::settings_exception &e) {
      LOG_ERROR_CORE_STD("Exception looking for module: " + utf8::utf8_from_native(e.what()));
    }
    if (equals_enabled(plugin)) {
      plugin = alias;
      alias = "";
    } else if (equals_enabled(alias)) {
      alias = "";
    } else if (equals_disabled(plugin)) {
      plugin = alias;
      alias = "";
    } else if (equals_disabled(alias)) {
      alias = "";
    }
    if (!alias.empty()) {
      const std::string tmp = plugin;
      plugin = alias;
      alias = tmp;
    }
    if (alias.empty()) {
      LOG_DEBUG_CORE_STD("Found: " + plugin);
    } else {
      LOG_DEBUG_CORE_STD("Found: " + plugin + " as " + alias);
    }
    if (plugin.length() > 4 && plugin.substr(plugin.length() - 4) == ".dll") plugin = plugin.substr(0, plugin.length() - 4);
    ret.insert(plugin_alias_list_type::value_type(alias, plugin));
  }
  boost::filesystem::directory_iterator end_itr;  // default construction yields past-the-end
  for (boost::filesystem::directory_iterator itr(plugin_path_); itr != end_itr; ++itr) {
    if (!is_directory(itr->status())) {
      boost::filesystem::path file = itr->path().filename();
      if (is_module(plugin_path_ / file)) {
        const std::string module = file_to_module(file);
        if (!contains_plugin(ret, "", module)) ret.insert(plugin_alias_list_type::value_type("", module));
      }
    }
  }
  return ret;
}

nsclient::core::plugin_manager::plugin_status nsclient::core::plugin_manager::parse_plugin(std::string key) {
  plugin_status status(key);
  try {
    status.alias = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, key, "");
  } catch (settings::settings_exception &e) {
    LOG_DEBUG_CORE_STD("Failed to read settings: " + utf8::utf8_from_native(e.what()));
  }
  if (status.alias == "") {
    status.enabled = false;
  } else if (equals_enabled(status.plugin)) {
    status.plugin = status.alias;
    status.alias = "";
  } else if (equals_enabled(status.alias)) {
    status.alias = "";
  } else if ((status.plugin == "disabled") || (status.alias == "disabled")) {
    status.enabled = false;
  } else if ((status.plugin == "0") || (status.alias == "0")) {
    status.enabled = false;
  } else if ((status.plugin == "false") || (status.alias == "false")) {
    status.enabled = false;
  } else if (equals_disabled(status.plugin)) {
    status.plugin = status.alias;
    status.alias = "";
  } else if (equals_disabled(status.alias)) {
    status.alias = "";
  }
  if (!status.alias.empty()) {
    std::string tmp = status.plugin;
    status.plugin = status.alias;
    status.alias = tmp;
  }
  if (status.plugin.length() > 4 && status.plugin.substr(status.plugin.length() - 4) == ".dll")
    status.plugin = status.plugin.substr(0, status.plugin.length() - 4);
  return status;
}

// Find all plugins which are marked as active under the [/modules] section.
nsclient::core::plugin_manager::plugin_alias_list_type nsclient::core::plugin_manager::find_all_active_plugins() {
  plugin_alias_list_type ret;

  for (std::string plugin : settings_manager::get_settings()->get_keys(MAIN_MODULES_SECTION)) {
    plugin_status status = parse_plugin(plugin);
    if (!status.enabled) {
      continue;
    }
    if (status.alias.empty()) {
      LOG_DEBUG_CORE_STD("Found: " + status.plugin);
    } else {
      LOG_DEBUG_CORE_STD("Found: " + status.plugin + " as " + status.alias);
    }
    ret.insert(plugin_alias_list_type::value_type(status.alias, status.plugin));
  }
  return ret;
}

// Read /settings/permissions{,/policies} into permissions_. Idempotent:
// safe to call from both the boot path and from do_reload("settings"). On
// reload we clear the rule table first so deleted rules disappear, then
// re-register each policies key. The four global switches use
// register_key + get_string so the settings UI / docs see the keys even
// when the operator hasn't customised them. See
// docs/design/core-permissions.md for the wire format.
void nsclient::core::plugin_manager::load_permissions() {
  const auto core = settings_manager::get_core();
  const auto settings = settings_manager::get_settings();
  if (!core || !settings) {
    LOG_ERROR_CORE("permissions: settings not available, skipping load");
    return;
  }
  try {
    const std::string section = "/settings/permissions";
    const std::string policies_section = section + "/policies";
    core->register_path(0xffff, section, "Core permissions",
                        "Optional policy layer that gates which commands a calling module / user may execute. "
                        "Disabled by default - see docs/reference/core-permissions.md.",
                        true, false);
    core->register_key(0xffff, section, "enabled", "bool", "Enabled",
                       "Master switch. When false (default), all calls are allowed. When true, rules in /settings/permissions/policies form a "
                       "strict allow-list - calls that don't match any rule are denied.",
                       "false", true, false);
    core->register_key(0xffff, section, "log denials", "bool", "Log denials", "Log every denial at warning level.", "true", true, false);
    core->register_key(0xffff, section, "log allows", "bool", "Log allows", "Log every allowed call at trace level (noisy).", "false", true, false);
    core->register_key(0xffff, section, "allow exec", "bool", "Allow exec",
                       "Global toggle for the exec command surface (WEB scripts UI, lua/python core:simple_exec, internal CLI exec). "
                       "Per-command rules in /settings/permissions/policies apply to QUERIES ONLY; exec is gated by this single "
                       "switch. Default true: enabling the policy system does not break existing exec callers. Set to false for a "
                       "hard exec lockdown.",
                       "true", true, false);
    core->register_path(0xffff, policies_section, "Permission policies",
                        "Rule table. Each key is a subject pattern (module[:principal]); the value is a comma-separated list of "
                        "object patterns (module.command). Rules merge additively.",
                        true, false);

    permissions_.clear_rules();
    const std::string enabled = settings->get_string(section, "enabled", "false");
    permissions_.set_enabled(enabled == "true" || enabled == "1");
    permissions_.set_log_denials(settings->get_string(section, "log denials", "true") != "false");
    permissions_.set_log_allows(settings->get_string(section, "log allows", "false") == "true");
    permissions_.set_allow_exec(settings->get_string(section, "allow exec", "true") != "false");

    for (const std::string &subject : settings->get_keys(policies_section)) {
      const std::string objects = settings->get_string(policies_section, subject, "");
      permissions_.add_rule(subject, objects);
    }
    LOG_DEBUG_CORE_STD("permissions: loaded " + str::xtos(permissions_.rule_count()) + " rule(s), enabled=" + (permissions_.is_enabled() ? "true" : "false"));
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("permissions: failed to load: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("permissions: failed to load (unknown error)");
  }
}

// Load all configured (nsclient.ini) plugins.
void nsclient::core::plugin_manager::load_active_plugins() {
  if (plugin_path_.empty()) {
    throw core_exception("No plugin path found.");
  }
  for (const plugin_alias_list_type::value_type &v : find_all_active_plugins()) {
    std::string module = v.first;
    std::string alias = v.first;
    try {
      add_plugin(v.second, v.first);
    } catch (const plugin_exception &e) {
      if (e.file().find("FileLogger") != std::string::npos) {
        LOG_DEBUG_CORE_STD("Failed to load " + module + ": " + e.reason());
      } else {
        LOG_ERROR_CORE_STD("Failed to load " + module + ": " + e.reason() + installer_feature_hint(v.second));
      }
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("exception loading plugin: " + module + utf8::utf8_from_native(e.what()) + installer_feature_hint(v.second));
    } catch (...) {
      LOG_ERROR_CORE_STD("Unknown exception loading plugin: " + module + installer_feature_hint(v.second));
    }
  }
}
// Load all available plugins (from the filesystem)
void nsclient::core::plugin_manager::load_all_plugins() {
  for (const plugin_alias_list_type::value_type &v : find_all_plugins()) {
    if (v.second == "NSCPDOTNET.dll" || v.second == "NSCPDOTNET" || v.second == "NSCP.Core") continue;
    try {
      add_plugin(v.second, v.first);
    } catch (const plugin_exception &e) {
      if (e.file().find("FileLogger") != std::string::npos) {
        LOG_DEBUG_CORE_STD("Failed to register plugin: " + e.reason());
      } else {
        LOG_ERROR_CORE("Failed to register plugin " + v.second + ": " + e.reason());
      }
    } catch (...) {
      LOG_CRITICAL_CORE_STD("Failed to register plugin key: " + v.second);
    }
  }
}

bool nsclient::core::plugin_manager::load_single_plugin(const std::string &plugin, const std::string &alias, bool start) {
  try {
    const plugin_type instance = add_plugin(plugin, alias);
    if (!instance) {
      LOG_ERROR_CORE("Failed to load: " + plugin + installer_feature_hint(plugin));
      return false;
    }
    if (start) {
      instance->load_plugin(NSCAPI::normalStart);
    }
    return true;
  } catch (const plugin_exception &e) {
    LOG_ERROR_CORE_STD("Module (" + e.file() + ") was not found: " + e.reason() + installer_feature_hint(e.file()));
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found: " + utf8::utf8_from_native(e.what()) + installer_feature_hint(plugin));
  } catch (...) {
    LOG_ERROR_CORE_STD("Module (" + plugin + ") was not found..." + installer_feature_hint(plugin));
  }
  return false;
}

void nsclient::core::plugin_manager::start_plugins(NSCAPI::moduleLoadMode mode) {
  std::set<long> broken;
  for (const plugin_type &plugin : plugin_list_.get_plugins()) {
    LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->getModule())
    try {
      if (!plugin->load_plugin(mode)) {
        LOG_ERROR_CORE_STD("Plugin refused to load: " + plugin->getModule());
        broken.insert(plugin->get_id());
      }
    } catch (const plugin_exception &e) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not load plugin: " + e.reason() + ": " + e.file());
    } catch (const std::exception &e) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->get_alias() + ": " + e.what());
    } catch (...) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not load plugin: " + plugin->getModule());
    }
  }
  for (const long &plugin_id : broken) {
    purge_broken_plugin(plugin_id);
  }
}

void nsclient::core::plugin_manager::purge_broken_plugin(const unsigned long plugin_id) {
  const auto plugin = plugin_list_.find_by_id(plugin_id);
  plugin_list_.remove(plugin_id);
  commands_.remove_plugin(plugin_id);
  channels_.remove_plugin(plugin_id);
  event_subscribers_.remove_plugin(plugin_id);
  metrics_fetchers_.remove_plugin(plugin_id);
  metrics_submitters_.remove_plugin(plugin_id);
  if (plugin) {
    plugin->unload_plugin();
  }
  plugin_cache_.remove_plugin(plugin_id);
}

void nsclient::core::plugin_manager::post_start_plugins() {
  std::set<long> broken;
  for (const plugin_type &plugin : plugin_list_.get_plugins()) {
    if (!plugin->has_start()) {
      continue;
    }
    LOG_DEBUG_CORE_STD("Starting plugin: " + plugin->getModule())
    try {
      if (!plugin->start_plugin()) {
        LOG_ERROR_CORE_STD("Plugin refused to start: " + plugin->getModule());
        broken.insert(plugin->get_id());
      }
    } catch (const plugin_exception &e) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not start plugin: " + e.reason() + ": " + e.file());
    } catch (const std::exception &e) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not start plugin: " + plugin->get_alias() + ": " + e.what());
    } catch (...) {
      broken.insert(plugin->get_id());
      LOG_ERROR_CORE_STD("Could not start plugin: " + plugin->getModule());
    }
  }
  for (const long &id : broken) {
    purge_broken_plugin(id);
  }
}

/**
 * First phase of shutdown: ask every plugin that opted-in to drain its
 * background work while every peer plugin is still alive and reachable.
 *
 * Runs serially in plugin-load order. The pass happens before any plugin is
 * unloaded and before commands/channels are torn down so plugins like the
 * Scheduler can finish in-flight queries and submissions cleanly.
 */
void nsclient::core::plugin_manager::prepare_shutdown_plugins() {
  for (const plugin_type &p : plugin_list_.get_plugins()) {
    if (!p) continue;
    if (!p->has_prepare_shutdown()) continue;
    try {
      LOG_DEBUG_CORE_STD("Preparing shutdown for plugin: " + p->get_alias_or_name() + "...");
      p->prepare_shutdown_plugin();
    } catch (const plugin_exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when preparing shutdown of plugin: " + e.reason() + " in module: " + e.file());
    } catch (...) {
      LOG_ERROR_CORE("Unknown exception raised when preparing shutdown of plugin");
    }
  }
}

/**
 * Unload all plug-ins
 */
void nsclient::core::plugin_manager::stop_plugins() {
  commands_.remove_all();
  channels_.remove_all();
  event_subscribers_.remove_all();
  metrics_fetchers_.remove_all();
  metrics_submitters_.remove_all();
  for (const plugin_type &p : plugin_list_.get_plugins()) {
    try {
      if (p) {
        LOG_DEBUG_CORE_STD("Unloading plugin: " + p->get_alias_or_name() + "...");
        p->unload_plugin();
      }
    } catch (const plugin_exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
    } catch (...) {
      LOG_ERROR_CORE("Unknown exception raised when unloading plugin");
    }
  }
  plugin_list_.clear();
}

boost::optional<boost::filesystem::path> nsclient::core::plugin_manager::find_file(const std::string &file_name) {
  std::string name = file_name;
  std::list<std::string> names;
  names.push_back(file_name);
  if (name.length() > 4 && (name.substr(name.length() - 4) == ".dll" || name.substr(name.length() - 4) == ".zip")) {
    name = name.substr(0, name.length() - 4);
  }
  names.push_back(get_plugin_file(name));
  names.push_back(name + ".zip");
  names.push_back("lib" + name + ".so");

  for (const std::string &current_name : names) {
    boost::filesystem::path tmp = plugin_path_ / current_name;
    if (boost::filesystem::is_regular_file(tmp)) {
      return tmp;
    }
  }

  for (const std::string &current_name : names) {
    boost::optional<boost::filesystem::path> module = file_helpers::finder::locate_file_icase(plugin_path_, current_name);
    if (module) {
      return module;
    }
    module = file_helpers::finder::locate_file_icase(boost::filesystem::path("./modules"), current_name);
    if (module) {
      return module;
    }
  }
  LOG_ERROR_CORE("Failed to find plugin: " + file_name + " in " + plugin_path_.string());
  return {};
}

nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::only_load_module(const std::string &module, const std::string &alias,
                                                                                             bool &loaded) {
  loaded = false;
  boost::optional<boost::filesystem::path> real_file = find_file(module);
  if (!real_file) {
    return {};
  }
  LOG_DEBUG_CORE_STD("Loading module " + real_file->string() + " (" + alias + ")");
  plugin_type dup = plugin_list_.find_duplicate(*real_file, alias);
  if (dup) {
    return dup;
  }
  loaded = true;
  if (boost::algorithm::ends_with(real_file->string(), ".zip")) {
    return std::make_shared<zip_plugin>(plugin_list_.get_next_id(), real_file->lexically_normal(), alias, path_, shared_from_this(), log_instance_);
  }
  return std::make_shared<dll_plugin>(plugin_list_.get_next_id(), real_file->lexically_normal(), alias);
}

/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the
 */
nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::add_plugin(const std::string &file_name, const std::string &alias) {
  try {
    bool loaded = false;
    plugin_type plugin = only_load_module(file_name, alias, loaded);
    if (!loaded) {
      return plugin;
    }
    plugin_list_.append_plugin(plugin);
    if (plugin->hasCommandHandler()) {
      commands_.add_plugin(plugin);
    }
    if (plugin->hasNotificationHandler()) {
      channels_.add_plugin(plugin);
    }
    if (plugin->hasMetricsFetcher()) {
      metrics_fetchers_.add_plugin(plugin);
    }
    if (plugin->hasMetricsSubmitter()) {
      metrics_submitters_.add_plugin(plugin);
    }
    if (plugin->hasMessageHandler()) {
      log_instance_->add_subscriber(plugin);
    }
    if (plugin->has_on_event()) {
      event_subscribers_.add_plugin(plugin);
    }
    const auto key = alias.empty() ? plugin->getModule() : alias;
    settings_manager::get_core()->register_key(0xffff, MAIN_MODULES_SECTION, key, "string", plugin->getName(), plugin->getDescription(), "0", false, false);
    plugin_cache_.add_plugin(plugin);
    return plugin;
  } catch (const plugin_exception &e) {
    LOG_ERROR_CORE("Failed to load plugin " + e.file() + ": " + utf8::utf8_from_native(e.what()));
    return plugin_type();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Failed to load plugin " + file_name + ": " + utf8::utf8_from_native(e.what()));
    return plugin_type();
  } catch (...) {
    LOG_ERROR_CORE("Failed to load plugin " + file_name);
    return plugin_type();
  }
}

bool nsclient::core::plugin_manager::reload_plugin(const std::string &module) {
  const plugin_type plugin = plugin_list_.find_by_alias(module);
  if (plugin) {
    LOG_DEBUG_CORE_STD(std::string("Reloading: ") + plugin->get_alias_or_name());
    plugin->load_plugin(NSCAPI::reloadStart);
    return true;
  }
  LOG_ERROR_CORE("Failed to reload plugin " + module);
  return false;
}

bool nsclient::core::plugin_manager::remove_plugin(const std::string &name) {
  const plugin_type plugin = plugin_list_.find_by_module(name);
  if (!plugin) {
    LOG_ERROR_CORE("Module " + name + " was not found.");
    return false;
  }
  unsigned int plugin_id = plugin->get_id();
  plugin_list_.remove(plugin_id);
  commands_.remove_plugin(plugin_id);
  metrics_fetchers_.remove_plugin(plugin_id);
  metrics_submitters_.remove_plugin(plugin_id);
  plugin->unload_plugin();
  plugin_cache_.remove_plugin(plugin_id);
  return true;
}

int nsclient::core::plugin_manager::clone_plugin(unsigned int plugin_id) {
  const plugin_type match = plugin_list_.find_by_id(plugin_id);
  if (match) {
    const int new_id = plugin_list_.get_next_id();
    commands_.add_plugin(new_id, match);
    return new_id;
  } else {
    LOG_ERROR_CORE("Plugin not found.");
    return -1;
  }
}

std::string nsclient::core::plugin_manager::get_plugin_module_name(unsigned int plugin_id) {
  const plugin_type plugin = plugin_list_.find_by_id(plugin_id);
  if (!plugin) return "";
  return plugin->get_alias_or_name();
}

nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::find_plugin(const unsigned int plugin_id) {
  return plugin_list_.find_by_id(plugin_id);
}

::PB::Commands::QueryResponseMessage nsclient::core::plugin_manager::execute_query(const ::PB::Commands::QueryRequestMessage &req) {
  ::PB::Commands::QueryResponseMessage resp;
  std::string buffer;
  if (execute_query(req.SerializeAsString(), buffer) == NSCAPI::cmd_return_codes::isSuccess) {
    resp.ParseFromString(buffer);
  }
  return resp;
}
/**
 * Inject a command into the plug-in stack.
 *
 * @param command Command to inject
 * @param argLen Length of argument buffer
 * @param **argument Argument buffer
 * @param *returnMessageBuffer Message buffer
 * @param returnMessageBufferLen Length of returnMessageBuffer
 * @param *returnPerfBuffer Performance data buffer
 * @param returnPerfBufferLen Length of returnPerfBuffer
 * @return The command status
 */
// Resolve the calling module + principal from the request header.
// core_helper::simple_query_as stamps two metadata keys (see
// create_simple_query_request_as in include/nscapi/nscapi_core_helper.cpp):
//
//   nscp.caller_plugin_id  - numeric plugin id of the caller. Set
//                            unconditionally by core_helper, so the
//                            calling DLL cannot fake it without
//                            rewriting core_helper. Resolved here to a
//                            module name via the trusted plugin_cache.
//   nscp.principal         - sub-identity (web user, NRPE client tag,
//                            CLI OS user, ...). Optional; empty when
//                            unset.
//
// Both keys are best-effort: legacy simple_query (no _as) sends neither,
// and direct NSAPIInject invocations may send neither either. An
// unresolved caller becomes "*" so a default-deny configuration
// explicitly catches that case rather than silently allowing it under a
// bare-module pattern.
std::string nsclient::core::plugin_manager::extract_subject_from_header(const PB::Common::Header &header, nsclient::core::plugin_cache *cache) {
  std::string plugin_id_str;
  std::string principal;
  for (const auto &kv : header.metadata()) {
    if (kv.key() == "nscp.caller_plugin_id")
      plugin_id_str = kv.value();
    else if (kv.key() == "nscp.principal")
      principal = kv.value();
  }
  std::string module;
  if (!plugin_id_str.empty() && cache) {
    try {
      const unsigned int id = static_cast<unsigned int>(str::stox<long>(plugin_id_str));
      module = cache->find_plugin_alias(id);
      // find_plugin_alias returns "Failed to find plugin ..." for
      // unknown ids; treat that as "caller unknown" rather than literal
      // text and let the default-deny path catch it.
      if (module.substr(0, 21) == "Failed to find plugin") module.clear();
    } catch (...) {
      module.clear();
    }
  }
  if (module.empty()) module = "*";
  return nsclient::core::permissions::make_subject(module, principal);
}

// Per-payload denial. Builds a "permission denied" response payload that
// mirrors the existing "Unknown command" shape (set_response_bad). Used
// by execute_query (per-command).
static void emit_denied_payload(PB::Commands::QueryResponseMessage *response_message, const std::string &command, const std::string &subject) {
  PB::Commands::QueryResponseMessage::Response *payload = response_message->add_payload();
  payload->set_command(command);
  nscapi::protobuf::functions::set_response_bad(*payload, "Permission denied: " + subject + " is not allowed to run " + command);
}

NSCAPI::nagiosReturn nsclient::core::plugin_manager::execute_query(const std::string &request, std::string &response) {
  try {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryResponseMessage response_message;
    request_message.ParseFromString(request);

    typedef boost::unordered_map<int, command_chunk> command_chunk_type;
    command_chunk_type command_chunks;

    std::string missing_commands;

    // Compute the subject once per request. The header is the same for
    // every payload, so we don't pay re-extraction cost per command.
    const std::string subject = plugin_manager::extract_subject_from_header(request_message.header(), &plugin_cache_);

    if (!request_message.header().command().empty()) {
      const std::string command = request_message.header().command();
      commands::plugin_type plugin = commands_.get(command);
      if (plugin) {
        const std::string target_module = plugin->getName();
        const std::string object = permissions::make_object(target_module, command);
        if (!permissions_.is_allowed(subject, object)) {
          if (permissions_.should_log_denials()) LOG_ERROR_CORE_STD("permissions: denied " + subject + " -> " + object);
          emit_denied_payload(&response_message, command, subject);
          response = response_message.SerializeAsString();
          return NSCAPI::cmd_return_codes::isSuccess;
        }
        // Log allows at INFO so `log allows = true` is actually visible
        // without operators also having to enable global trace logging.
        // The toggle is off by default precisely because this is noisy.
        if (permissions_.should_log_allows()) LOG_INFO_CORE_STD("permissions: allowed " + subject + " -> " + object);
        const unsigned int id = plugin->get_id();
        command_chunks[id].plugin = plugin;
        command_chunks[id].request.CopyFrom(request_message);
      } else {
        str::format::append_list(missing_commands, command);
      }
    } else {
      for (int i = 0; i < request_message.payload_size(); i++) {
        ::PB::Commands::QueryRequestMessage::Request *payload = request_message.mutable_payload(i);
        payload->set_command(commands_.make_key(payload->command()));
        commands::plugin_type plugin = commands_.get(payload->command());
        if (plugin) {
          const std::string target_module = plugin->getName();
          const std::string object = permissions::make_object(target_module, payload->command());
          if (!permissions_.is_allowed(subject, object)) {
            if (permissions_.should_log_denials()) LOG_ERROR_CORE_STD("permissions: denied " + subject + " -> " + object);
            // Per-command denial: this payload does NOT get added to a
            // chunk for plugin dispatch, but we still emit a response
            // payload so the caller sees a structured error for this
            // command (and the other commands in the batch run normally).
            emit_denied_payload(&response_message, payload->command(), subject);
            continue;
          }
          if (permissions_.should_log_allows()) LOG_INFO_CORE_STD("permissions: allowed " + subject + " -> " + object);
          const unsigned int id = plugin->get_id();
          if (command_chunks.find(id) == command_chunks.end()) {
            command_chunks[id].plugin = plugin;
            command_chunks[id].request.mutable_header()->CopyFrom(request_message.header());
          }
          command_chunks[id].request.add_payload()->CopyFrom(*payload);
        } else {
          str::format::append_list(missing_commands, payload->command());
        }
      }
    }

    if (command_chunks.size() == 0) {
      // Three reasons we got here with no chunks to dispatch:
      //   1. Some commands were unknown (missing_commands non-empty).
      //      Original behaviour: log + add an "Unknown command(s)"
      //      payload to the response.
      //   2. Every command was policy-denied. response_message already
      //      has structured denial payloads from emit_denied_payload;
      //      we just need to ship them as-is - logging
      //      "Unknown command(s):" with an empty list here would be
      //      noisy and misleading.
      //   3. Empty request (no header command, no payloads). Log it
      //      once as a warning and return; the caller gets an empty
      //      response_message, which is what they implicitly asked for.
      if (!missing_commands.empty()) {
        LOG_ERROR_CORE("Unknown command(s): " + missing_commands + " available commands: " + commands_.to_string());
        PB::Commands::QueryResponseMessage::Response *payload = response_message.add_payload();
        payload->set_command(missing_commands);
        nscapi::protobuf::functions::set_response_bad(*payload, "Unknown command(s): " + missing_commands);
      } else if (response_message.payload_size() == 0) {
        LOG_DEBUG_CORE("Empty query request (no header command and no payloads); returning empty response");
      }
      response = response_message.SerializeAsString();
      return NSCAPI::cmd_return_codes::isSuccess;
    }

    for (command_chunk_type::value_type &v : command_chunks) {
      std::string local_response;
      int ret = v.second.plugin->handleCommand(v.second.request.SerializeAsString(), local_response);
      if (ret != NSCAPI::cmd_return_codes::isSuccess) {
        LOG_ERROR_CORE("Failed to execute command");
      } else {
        PB::Commands::QueryResponseMessage local_response_message;
        local_response_message.ParseFromString(local_response);
        if (!response_message.has_header()) {
          response_message.mutable_header()->CopyFrom(local_response_message.header());
        }
        for (int i = 0; i < local_response_message.payload_size(); i++) {
          response_message.add_payload()->CopyFrom(local_response_message.payload(i));
        }
      }
    }
    response = response_message.SerializeAsString();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Failed to process command: " + utf8::utf8_from_native(e.what()));
    return NSCAPI::cmd_return_codes::hasFailed;
  } catch (...) {
    LOG_ERROR_CORE("Failed to process command: ");
    return NSCAPI::cmd_return_codes::hasFailed;
  }
  return NSCAPI::cmd_return_codes::isSuccess;
}

int nsclient::core::plugin_manager::load_and_run(std::string module, run_function fun, std::list<std::string> &errors) {
  if (!module.empty()) {
    const plugin_type match = plugin_list_.find_by_module(module);
    if (match) {
      LOG_DEBUG_CORE_STD("Found module: " + match->get_alias_or_name() + "...");
      try {
        return fun(match);
      } catch (const plugin_exception &e) {
        errors.push_back("Could not execute command: " + e.reason() + " in " + e.file());
        return 1;
      }
    }
    try {
      const plugin_type plugin = add_plugin(module, "");
      if (plugin) {
        LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->get_alias_or_name() + "...");
        plugin->load_plugin(NSCAPI::dontStart);
        return fun(plugin);
      } else {
        errors.push_back("Failed to load: " + module);
        return 1;
      }
    } catch (const plugin_exception &e) {
      errors.push_back("Module (" + e.file() + ") was not found: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      errors.push_back(std::string("Module (") + module + ") was not found: " + utf8::utf8_from_native(e.what()));
      return 1;
    } catch (...) {
      errors.push_back("Module (" + module + ") was not found...");
      return 1;
    }
  } else {
    errors.push_back("No module was specified...");
  }
  return 1;
}

int exec_helper(nsclient::core::plugin_manager::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request,
                std::list<std::string> *responses) {
  std::string response;
  if (!plugin || !plugin->has_command_line_exec()) return -1;
  const int ret = plugin->commandLineExec(true, request, response);
  if (ret != NSCAPI::cmd_return_codes::returnIgnored && !response.empty()) responses->push_back(response);
  return ret;
}

int nsclient::core::plugin_manager::simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
  std::string request;
  std::list<std::string> responses;
  std::list<std::string> errors;
  std::string module;
  std::string::size_type pos = command.find('.');
  if (pos != std::string::npos) {
    module = command.substr(0, pos);
    command = command.substr(pos + 1);
  }
  nscapi::protobuf::functions::create_simple_exec_request(module, command, arguments, request);
  int ret = load_and_run(
      module, [command, arguments, request, &responses](auto plugin) { return exec_helper(plugin, command, arguments, request, &responses); }, errors);

  for (std::string &r : responses) {
    try {
      ret = nscapi::protobuf::functions::parse_simple_exec_response(r, resp);
    } catch (std::exception &e) {
      resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
      LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
      return NSCAPI::cmd_return_codes::hasFailed;
    }
  }
  for (const std::string &e : errors) {
    LOG_ERROR_CORE_STD(e);
    resp.push_back(e);
  }
  return ret;
}
int query_helper(nsclient::core::plugin_manager::plugin_type plugin, std::string command, std::vector<std::string> arguments, std::string request,
                 std::list<std::string> *responses) {
  return NSCAPI::cmd_return_codes::returnIgnored;
  // 	std::string response;
  // 	if (!plugin->hasCommandHandler())
  // 		return NSCAPI::returnIgnored;
  // 	int ret = plugin->handleCommand(command.c_str(), request, response);
  // 	if (ret != NSCAPI::returnIgnored && !response.empty())
  // 		responses->push_back(response);
  // 	return ret;
}

int nsclient::core::plugin_manager::simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp) {
  std::string request;
  std::list<std::string> responses;
  std::list<std::string> errors;
  nscapi::protobuf::functions::create_simple_query_request(command, arguments, request);
  int ret = load_and_run(
      module, [command, arguments, request, &responses](auto plugin) { return query_helper(plugin, command, arguments, request, &responses); }, errors);

  commands::plugin_type plugin = commands_.get(command);
  if (!plugin) {
    LOG_ERROR_CORE("No handler for command: " + command + " available commands: " + commands_.to_string());
    resp.push_back("No handler for command: " + command);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  std::string response;
  ret = plugin->handleCommand(request, response);
  try {
    std::string msg, perf;
    ret = nscapi::protobuf::functions::parse_simple_query_response(response, msg, perf, static_cast<std::size_t>(-1));
    resp.push_back(perf.empty() ? msg : msg + "|" + perf);
  } catch (std::exception &e) {
    resp.push_back("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
    LOG_ERROR_CORE_STD("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  for (const std::string &e : errors) {
    LOG_ERROR_CORE_STD(e);
    resp.push_back(e);
  }
  return ret;
}

NSCAPI::nagiosReturn nsclient::core::plugin_manager::exec_command(const char *raw_target, std::string request, std::string &response) {
  std::string target = raw_target;
  LOG_DEBUG_CORE_STD("Executing command is target for: " + target);
  bool match_any = false;
  bool match_all = false;
  if (target == "any")
    match_any = true;
  else if (target == "all" || target == "*")
    match_all = true;

  // Exec is gated by a single global toggle (/settings/permissions/allow exec),
  // not by the per-command rule table. Rationale: the internal exec chain
  // (lua/python -> core_helper::exec_simple_command, see nscapi_core_helper.cpp)
  // does not propagate caller identity, so a per-command policy decision on
  // exec degenerates to subject "*" and is unreliable. Per-command policy is
  // therefore queries-only; exec gets a coarse on/off switch. The default is
  // "on" so enabling the policy system does not silently break exec callers.
  if (!permissions_.is_exec_allowed()) {
    if (permissions_.should_log_denials()) {
      std::string subject = "*";
      std::string command = "(unknown)";
      try {
        PB::Commands::ExecuteRequestMessage exec_request;
        if (exec_request.ParseFromString(request)) {
          subject = plugin_manager::extract_subject_from_header(exec_request.header(), &plugin_cache_);
          if (exec_request.payload_size() > 0) command = exec_request.payload(0).command();
        }
      } catch (...) {
        // Best-effort logging - parse failure just means we log a less
        // informative line, never a reason to drop the denial itself.
      }
      LOG_ERROR_CORE_STD("permissions: denied (allow exec=false) " + subject + " -> exec " + target + "." + command);
    }
    PB::Commands::ExecuteResponseMessage denied_response;
    PB::Commands::ExecuteResponseMessage::Response *r = denied_response.add_payload();
    r->set_command("");
    r->set_result(PB::Common::ResultCode::CRITICAL);
    r->set_message("Permission denied: exec is globally disabled (/settings/permissions/allow exec = false)");
    denied_response.SerializeToString(&response);
    return NSCAPI::cmd_return_codes::isSuccess;
  }

  std::list<std::string> responses;
  bool found = false;
  for (plugin_type p : plugin_list_.get_plugins()) {
    if (p && p->has_command_line_exec()) {
      IS_LOG_TRACE_CORE() { LOG_TRACE_CORE("Trying : " + p->get_alias_or_name()); }
      try {
        if (match_all || match_any || p->get_alias() == target || p->get_alias_or_name().find(target) != std::string::npos) {
          std::string respbuffer;
          LOG_DEBUG_CORE_STD("Executing command in: " + p->getName());
          NSCAPI::nagiosReturn r = p->commandLineExec(!(match_all || match_any), request, respbuffer);
          if (r != NSCAPI::cmd_return_codes::returnIgnored && !respbuffer.empty()) {
            LOG_DEBUG_CORE_STD("Module handled execution request: " + p->getName());
            found = true;
            if (match_any) {
              response = respbuffer;
              return NSCAPI::exec_return_codes::returnOK;
            }
            responses.push_back(respbuffer);
          }
        }
      } catch (plugin_exception &e) {
        LOG_ERROR_CORE_STD("Could not execute command: " + e.reason() + " in " + e.file());
      }
    }
  }

  PB::Commands::ExecuteResponseMessage response_message;

  for (std::string current_response : responses) {
    PB::Commands::ExecuteResponseMessage tmp;
    tmp.ParseFromString(current_response);
    for (int i = 0; i < tmp.payload_size(); i++) {
      PB::Commands::ExecuteResponseMessage::Response *r = response_message.add_payload();
      r->CopyFrom(tmp.payload(i));
    }
  }
  response_message.SerializeToString(&response);
  if (found) return NSCAPI::cmd_return_codes::isSuccess;
  return NSCAPI::cmd_return_codes::returnIgnored;
}

void nsclient::core::plugin_manager::register_submission_listener(unsigned int plugin_id, const char *channel) {
  channels_.register_listener(plugin_id, channel);
}

NSCAPI::errorReturn nsclient::core::plugin_manager::send_notification(const char *channel, std::string &request, std::string &response) {
  std::string schannel = channel;
  bool found = false;
  for (std::string cur_chan : str::utils::split_lst(schannel, std::string(","))) {
    if (cur_chan == "noop") {
      found = true;
      nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", response);
      continue;
    }
    if (cur_chan == "log") {
      PB::Commands::SubmitRequestMessage msg;
      msg.ParseFromString(request);
      for (int i = 0; i < msg.payload_size(); i++) {
        LOG_INFO_CORE("Notification " + str::xtos(msg.payload(i).result()) + ": " +
                      nscapi::protobuf::functions::query_data_to_nagios_string(msg.payload(i), nscapi::protobuf::functions::no_truncation));
      }
      found = true;
      nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", response);
      continue;
    }
    try {
      for (nsclient::plugin_type p : channels_.get(cur_chan)) {
        try {
          p->handleNotification(cur_chan.c_str(), request, response);
        } catch (...) {
          LOG_ERROR_CORE("Plugin throw exception: " + p->get_alias_or_name());
        }
        found = true;
      }
    } catch (nsclient::plugins_list_exception &e) {
      LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
      return NSCAPI::api_return_codes::hasFailed;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE("No handler for channel: " + schannel + ": " + utf8::utf8_from_native(e.what()));
      return NSCAPI::api_return_codes::hasFailed;
    } catch (...) {
      LOG_ERROR_CORE("No handler for channel: " + schannel);
      return NSCAPI::api_return_codes::hasFailed;
    }
  }
  if (!found) {
    LOG_ERROR_CORE("No handler for channel: " + schannel + " active channels: " + channels_.to_string());
    return NSCAPI::api_return_codes::hasFailed;
  }
  return NSCAPI::api_return_codes::isSuccess;
}

NSCAPI::errorReturn nsclient::core::plugin_manager::emit_event(const std::string &request) {
  PB::Commands::EventMessage em;
  em.ParseFromString(request);
  for (const PB::Commands::EventMessage::Request &r : em.payload()) {
    bool has_matched = false;
    try {
      for (const nsclient::plugin_type &p : event_subscribers_.get(r.event())) {
        try {
          p->on_event(request);
          has_matched = true;
        } catch (const std::exception &e) {
          LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
        } catch (...) {
          LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": UNKNOWN EXCEPTION");
        }
      }
    } catch (nsclient::plugins_list_exception &e) {
      LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
      return NSCAPI::api_return_codes::hasFailed;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE("No handler for event: " + utf8::utf8_from_native(e.what()));
      return NSCAPI::api_return_codes::hasFailed;
    } catch (...) {
      LOG_ERROR_CORE("No handler for event");
      return NSCAPI::api_return_codes::hasFailed;
    }
    if (!has_matched) {
      LOG_DEBUG_CORE("No handler for event: " + r.event());
    }
  }
  return NSCAPI::api_return_codes::isSuccess;
}

struct metrics_fetcher {
  PB::Metrics::MetricsMessage result;
  std::string buffer;
  metrics_fetcher() { result.add_payload(); }

  PB::Metrics::MetricsMessage::Response *get_root() { return result.mutable_payload(0); }
  void add_bundle(const PB::Metrics::MetricsBundle &b) { get_root()->add_bundles()->CopyFrom(b); }
  void fetch(nsclient::plugin_type p) {
    std::string local_buffer;
    p->fetchMetrics(local_buffer);
    PB::Metrics::MetricsMessage payload;
    payload.ParseFromString(local_buffer);
    for (const PB::Metrics::MetricsMessage::Response &r : payload.payload()) {
      for (const PB::Metrics::MetricsBundle &b : r.bundles()) {
        add_bundle(b);
      }
    }
  }
  void render() {
    get_root()->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
    buffer = result.SerializeAsString();
  }
  void digest(nsclient::plugin_type p) const { p->submitMetrics(buffer); }
};

bool nsclient::core::plugin_manager::is_enabled(const std::string module) { return parse_plugin(module).enabled; }

void nsclient::core::plugin_manager::process_metrics(PB::Metrics::MetricsBundle bundle) {
  metrics_fetcher f;
  metrics_fetchers_.do_all([&f](auto key) { return f.fetch(key); });
  f.get_root()->add_bundles()->CopyFrom(bundle);
  f.render();
  metrics_submitters_.do_all([&f](auto key) { return f.digest(key); });
}

bool nsclient::core::plugin_manager::enable_plugin(std::string name) {
  try {
    settings_manager::get_settings()->set_string(MAIN_MODULES_SECTION, name, "enabled");
  } catch (settings::settings_exception &e) {
    LOG_DEBUG_CORE_STD("Failed to read settings: " + utf8::utf8_from_native(e.what()));
    return false;
  }
  return true;
}

bool nsclient::core::plugin_manager::disable_plugin(std::string name) {
  try {
    settings_manager::get_settings()->set_string(MAIN_MODULES_SECTION, name, "disabled");
  } catch (settings::settings_exception &e) {
    LOG_DEBUG_CORE_STD("Failed to read settings: " + utf8::utf8_from_native(e.what()));
    return false;
  }
  return true;
}

boost::filesystem::path nsclient::core::plugin_manager::get_filename(boost::filesystem::path folder, std::string module) {
  return dll::dll_impl::fix_module_name(folder / module);
}

void nsclient::core::plugin_manager::set_path(boost::filesystem::path path) { plugin_path_ = file_helpers::meta::make_preferred(path); }
