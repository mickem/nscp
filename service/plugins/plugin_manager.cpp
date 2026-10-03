// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "plugin_manager.hpp"

#include <config.h>

#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/unordered_map.hpp>
#include <chrono>
#include <file_helpers.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nscp/name_safety.hpp>
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
      // Python Scripting
      {"PythonScript", "Python Scripting"},
      // Various client plugins
      {"GraphiteClient", "Various client plugins"},
      {"SMTPClient", "Various client plugins"},
      {"SyslogClient", "Various client plugins"},
      {"NRDPClient", "Various client plugins"},
      {"IcingaClient", "Various client plugins"},
      {"GearmanClient", "Various client plugins"},
      {"CollectdClient", "Various client plugins"},
      {"NSCPClient", "Various client plugins"},
      // Lua Scripting
      {"LUAScript", "Lua Scripting"},
      // .NET plugin support
      {"DotnetPlugins", ".NET plugin support"},
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
      {"CheckWindowsApps", "Check Plugins"},
      {"CheckHyperV", "Check Plugins"},
      {"CheckSecurity", "Check Plugins"},
      {"CheckActiveDirectory", "Check Plugins"},
      {"CheckMSSQL", "Check Plugins"},
      // Its own feature, not part of "Check Plugins": it is the one check
      // module that carries a third-party runtime library (libmariadb.dll).
      {"CheckMySQL", "MySQL Support"},
      {"SimpleCache", "Check Plugins"},
      {"SimpleFileWriter", "Check Plugins"},
      {"CheckLogFile", "Check Plugins"},
      {"CheckNet", "Check Plugins"},
      {"CheckDocker", "Check Plugins"},
      {"CheckKubernetes", "Check Plugins"},
  };
  // Not in the MSI at all: antivirus engines flag the DLL with generic
  // machine-learning verdicts, so it ships in the zip only (see the NSCANg
  // component in Product.wxs). No feature to point at, so say where it is.
  if (name == "NSCANgClient") {
    return std::string(" (module '") + name +
           "' is not part of the Windows installer; copy modules/NSCANgClient.dll from the "
           "NSClient++ zip of the same version into the modules folder, see the NSCANgClient "
           "reference documentation)";
  }
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
      facts_fetchers_(log_instance_),
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
// safe to call from both the boot path and from do_reload("settings"). The
// whole table is rebuilt aside and published in one step, so deleted rules
// disappear without a window in which there are none. The four global
// switches use register_key + get_string so the settings UI / docs see the
// keys even when the operator hasn't customised them. See
// docs/design/core-permissions.md for the wire format.
void nsclient::core::plugin_manager::load_permissions() {
  const auto core = settings_manager::get_core();
  const auto settings = settings_manager::get_settings();
  if (!core || !settings) {
    LOG_ERROR_CORE("permissions: settings not available, skipping load");
    return;
  }
  const std::string section = "/settings/permissions";
  const std::string policies_section = section + "/policies";
  // Documentation metadata for the settings UI and the reference docs. It
  // does not decide the policy, so failing to register it (a registry-lock
  // timeout) must not count as a failed load.
  try {
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
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("permissions: failed to register the settings keys: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("permissions: failed to register the settings keys (unknown error)");
  }

  // Build the table aside and publish it in one step: checks keep flowing
  // on the server pools during a reload, and filling permissions_ rule by
  // rule left it empty (deny-all, with the policy enabled) in between.
  // Declared outside the try so a failed load can still publish it (below),
  // with the two switches it may not have got as far as reading.
  nsclient::core::permissions fresh;
  bool enabled_read = false;
  bool exec_read = false;
  try {
    const std::string enabled = settings->get_string(section, "enabled", "false");
    fresh.set_enabled(enabled == "true" || enabled == "1");
    enabled_read = true;
    fresh.set_log_denials(settings->get_string(section, "log denials", "true") != "false");
    fresh.set_log_allows(settings->get_string(section, "log allows", "false") == "true");
    fresh.set_allow_exec(settings->get_string(section, "allow exec", "true") != "false");
    exec_read = true;

    for (const std::string &subject : settings->get_keys(policies_section)) {
      const std::string objects = settings->get_string(policies_section, subject, "");
      fresh.add_rule(subject, objects);
    }
    permissions_.replace_with(fresh);
    LOG_DEBUG_CORE_STD("permissions: loaded " + str::xtos(permissions_.rule_count()) + " rule(s), enabled=" + (permissions_.is_enabled() ? "true" : "false"));
    return;
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("permissions: failed to load: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("permissions: failed to load (unknown error)");
  }
  // A load that failed part-way publishes what it read rather than keeping
  // the previous table, which kept every rule the operator had just removed
  // (and the old exec switch) in force. An enabled policy fails closed: exec
  // denied unless it was read, and no rule that was not read. Whether the
  // policy is on at all is taken from what was read, or else left as it is:
  // a host that never enabled it is not turned into deny-all by a transient
  // settings-lock timeout. At boot that means a policy whose `enabled` could
  // not be read stays off until a reload reads it.
  fresh.complete_failed_load(enabled_read, exec_read, permissions_.is_enabled());
  permissions_.replace_with(fresh);
  if (fresh.is_enabled()) {
    LOG_ERROR_CORE_STD("permissions: enforcing the " + str::xtos(fresh.rule_count()) +
                       " rule(s) read before the failure; every other call is denied until the policy loads");
  } else if (!enabled_read) {
    LOG_ERROR_CORE("permissions: could not read whether the policy is enabled; it stays disabled until the policy loads");
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
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  try {
    const plugin_type instance = add_plugin(plugin, alias);
    if (!instance) {
      LOG_ERROR_CORE("Failed to load: " + plugin + installer_feature_hint(plugin));
      return false;
    }
    if (start) {
      if (!instance->load_plugin(NSCAPI::normalStart)) {
        // Keep a module whose loadModuleEx failed out of the list: it stays
        // mapped with loaded_ == false, and the next exec targeting `any` or
        // `all` would otherwise call into it.
        LOG_ERROR_CORE("Failed to load: " + plugin);
        purge_broken_plugin(instance->get_id());
        return false;
      }
      // A plugin loaded into an already running agent never sees
      // post_start_plugins, so start it here: modules which defer work until
      // every peer is available (Scheduler's run-on-startup schedules,
      // LUAScript's on-start hook) would otherwise never get going.
      if (instance->has_start()) {
        instance->start_plugin();
      }
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
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  std::set<long> broken;
  for (const plugin_type &plugin : plugin_list_.get_plugins()) {
    LOG_DEBUG_CORE_STD("Loading plugin: " + plugin->getModule())
    try {
      if (!plugin->load_plugin(mode)) {
        LOG_ERROR_CORE_STD("Plugin refused to load: " + plugin->getModule());
        broken.insert(plugin->get_id());
      } else if (plugin->reload_raced()) {
        LOG_ERROR_CORE_STD("Reloaded " + plugin->get_alias_or_name() + " while calls into it were still running: a check held it for over 5s");
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

std::vector<nsclient::core::plugin_manager::walk_closing> nsclient::core::plugin_manager::close_walks(const unsigned long plugin_id) {
  std::vector<simple_plugins_list::closing> closings;
  for (simple_plugins_list *list : {&metrics_fetchers_, &metrics_submitters_, &facts_fetchers_}) {
    const simple_plugins_list::closing c = list->close_plugin(plugin_id);
    if (c) closings.push_back(c);
  }
  const std::vector<bool> drained = simple_plugins_list::drain_each(closings, std::chrono::seconds(10));
  std::vector<walk_closing> walks;
  for (std::size_t i = 0; i < closings.size(); ++i) walks.push_back(walk_closing{closings[i], drained[i]});
  return walks;
}

void nsclient::core::plugin_manager::purge_broken_plugin(const unsigned long plugin_id) {
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  plugin_type plugin = plugin_list_.find_by_id(plugin_id);
  // The load that failed was a reload driven from inside the module itself.
  // The core defers or refuses those (NSClientT::reload), so this is the
  // backstop. Whatever loadModuleEx tore down before it failed - scripts
  // unloaded, a server stopped - is gone, so the module cannot be left listed
  // as loaded: it would answer nothing and could never be loaded fresh. It is
  // taken out of every registry as below, but neither unloaded nor released:
  // unloading would run the module's teardown under the thread still inside
  // it, and dropping the last reference would destroy the instance - and unmap
  // the library - under that thread. It is parked until stop_plugins, when
  // nothing can still be inside it.
  //
  // A log line, or a metrics or facts round, still inside the module when
  // the wait for it runs out takes the same path: a purge cannot refuse, but
  // it need not unload under the call either. The waits are per module, so
  // what they report is inside this one. stop_plugins waits for it again
  // and leaves it alone if it is still there.
  const std::vector<walk_closing> walks = close_walks(plugin_id);
  // Not in the master list any more, but still in a walk list: that list's
  // handle is the module, and the checks and the unload below need it - a
  // handle dropped unchecked could be the last one, and run the module's
  // destructor here, under a thread that may still be inside it.
  for (const walk_closing &w : walks) {
    if (plugin) break;
    plugin = w.slot.plugin();
  }
  const bool inside = plugin && plugin->is_dispatching_on_this_thread();
  // Each list is finished with its own outcome: a round stuck in the facts
  // list leaves only that slot closed, holding the module for stop_plugins
  // to wait for again; the lists it came clear of let it go. The handles
  // finish() hands back are kept to the end of this call, alongside
  // `plugin`, so none of them is dropped while the module is still in use.
  bool walked = false;
  std::vector<plugin_type> released;
  for (const walk_closing &w : walks) {
    if (!w.drained) walked = true;
    released.push_back(w.slot.finish(w.drained));
  }
  const bool delivering = plugin && log_instance_->remove_subscriber(plugin).delivering;
  const bool parked = inside || walked || delivering;
  if (inside) {
    LOG_ERROR_CORE_STD("Removing " + plugin->get_alias_or_name() +
                       " after its failed reload; it is unloaded at shutdown, as the reload was requested from inside a call it is serving");
  } else if (walked) {
    LOG_ERROR_CORE_STD("Removing " + plugin->get_alias_or_name() +
                       " after its failed reload; it is unloaded at shutdown, as a metrics or facts round is still running inside it after 10 s");
  } else if (delivering) {
    LOG_ERROR_CORE_STD("Removing " + plugin->get_alias_or_name() +
                       " after its failed reload; it is unloaded at shutdown, as a log line is still being handled by it after 5 s");
  }
  if (parked) retired_plugins_.push_back(plugin);
  plugin_list_.remove(plugin_id);
  commands_.remove_plugin(plugin_id);
  channels_.remove_plugin(plugin_id);
  event_subscribers_.remove_plugin(plugin_id);
  // Whatever this module contributed to the inventory goes with it: a frozen
  // fact set from a module that is no longer running is worse than none.
  if (facts_) facts_->remove_owned_by(static_cast<unsigned int>(plugin_id));
  if (plugin && !parked) {
    try {
      plugin->unload_plugin();
    } catch (const plugin_exception &e) {
      // The module refused because calls into it are still running. It has
      // already been removed from every registry above, so let the purge
      // finish; the library stays mapped and the module stays loaded until the
      // process ends. Throwing here instead aborted the caller - start_plugins
      // purges outside any try - and left the cache saying it was loaded.
      LOG_ERROR_CORE_STD("Failed to unload broken plugin: " + e.reason());
    }
  }
  plugin_cache_.remove_plugin(plugin_id);
}

void nsclient::core::plugin_manager::post_start_plugins() {
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
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
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
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

// Plugins stop_plugins could not tear down because a log line or a metrics or
// facts round was still inside them. Allocated once and never freed on purpose: a static
// list would destroy its plugins at exit, after their libraries' own statics
// are gone - the crash on process exit plugin_manager.hpp describes.
std::list<nsclient::plugin_type> &nsclient::core::plugin_manager::abandoned_plugins() {
  static std::list<plugin_type> *const abandoned = new std::list<plugin_type>();
  return *abandoned;
}

/**
 * Unload all plug-ins
 */
void nsclient::core::plugin_manager::stop_plugins() {
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  // The log subscriptions go first, and under the lifecycle lock, so this
  // cannot interleave with a remove_plugin on another thread: that call has
  // either finished (and a refused unload has re-added its subscription,
  // which goes here) or not started (and then this clear has already waited
  // for the deliveries, so the module it finds nothing left to drop for has
  // no line inside it either).
  //
  // The logger and the walk lists report the modules a log line or a round
  // is still inside after their waits - the parked ones included, which the
  // logger keeps draining and the walk lists keep closed. remove_plugin
  // refuses exactly this; shutdown cannot, but it need not tear a module
  // down under the call either. Such a module is neither unloaded nor
  // released: unloading would run the teardown under the call, and the
  // last reference going would unmap the library under it. It is handed to
  // a holder that is never freed, so the library stays mapped until the
  // process ends - the same leak unload_plugin chooses for a call that will
  // not come back.
  const std::vector<logging::logging_subscriber_instance> still_handling = log_instance_->clear_subscribers();
  std::vector<plugin_type> still_walked;
  for (simple_plugins_list *list : {&metrics_fetchers_, &metrics_submitters_, &facts_fetchers_}) {
    const std::vector<plugin_type> stuck = list->remove_all();
    still_walked.insert(still_walked.end(), stuck.begin(), stuck.end());
  }
  commands_.remove_all();
  channels_.remove_all();
  event_subscribers_.remove_all();
  const auto leave_in_use = [&](const plugin_type &p) {
    if (std::find(still_handling.begin(), still_handling.end(), p) != still_handling.end()) {
      LOG_ERROR_CORE_STD("Leaving " + p->get_alias_or_name() + " loaded at shutdown: a log line is still being handled by it after 5 s");
    } else if (std::find(still_walked.begin(), still_walked.end(), p) != still_walked.end()) {
      LOG_ERROR_CORE_STD("Leaving " + p->get_alias_or_name() + " loaded at shutdown: a metrics or facts round is still running inside it");
    } else {
      return false;
    }
    abandoned_plugins().push_back(p);
    return true;
  };
  for (const plugin_type &p : plugin_list_.get_plugins()) {
    try {
      if (p) {
        if (leave_in_use(p)) continue;
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
  // Parked by purge_broken_plugin while something was still inside them.
  // The dispatch that parked one has long returned by now; a log line or a
  // round may not have, which leave_in_use asks the waits above about.
  for (const plugin_type &p : retired_plugins_) {
    try {
      if (leave_in_use(p)) continue;
      p->unload_plugin();
    } catch (const plugin_exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when unloading plugin: " + e.reason() + " in module: " + e.file());
    } catch (...) {
      LOG_ERROR_CORE("Unknown exception raised when unloading plugin");
    }
  }
  retired_plugins_.clear();
}

boost::optional<boost::filesystem::path> nsclient::core::plugin_manager::find_file(const std::string &file_name) {
  // A module name is a single filename inside the module path, never a path.
  //
  // The right-hand side of a [/modules] entry, and the name in a settings
  // Control.LOAD request, both arrive here as-is. An absolute value replaced
  // plugin_path_ outright and `..` walked out of it, so `/tmp/evil.so = enabled`
  // or `C:\Users\Public\evil.dll = enabled` loaded an arbitrary shared object
  // into the SYSTEM or root process. Both writers are already
  // code-execution-equivalent (a settings.put grant plus a reload; a script
  // plugin issuing a registry query), but nothing about "name a module" should
  // also mean "name a file anywhere on this host".
  //
  // The rule the REST module routes have applied for a while, shared with them
  // rather than restated: one segment, no separators, no drive letter, no
  // `.`/`..`, not starting with `-`, and only alphanumerics plus `._-`.
  if (!name_safety::is_safe_module_name(file_name)) {
    LOG_ERROR_CORE("Refusing to load plugin '" + file_name + "': a module is named by a single file name inside the module path, not by a path.");
    return {};
  }
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

  // `${exe-path}/modules`, not `./modules`. The relative form resolved against
  // the process's current directory, so `nscp client` or `nscp test` run by an
  // administrator from a user-writable folder would load a library planted
  // there whenever the real module was missing. The directory beside the
  // executable is what the fallback was always meant to name.
  // getFolder("exe-path") is the public spelling of getBasePath(); resolved
  // once here rather than per candidate name, since it cannot change between
  // iterations.
  const boost::filesystem::path exe_path = path_->getFolder("exe-path");

  for (const std::string &current_name : names) {
    boost::optional<boost::filesystem::path> module = file_helpers::finder::locate_file_icase(plugin_path_, current_name);
    if (module) {
      return module;
    }
    if (!exe_path.empty()) {
      module = file_helpers::finder::locate_file_icase(exe_path / "modules", current_name);
      if (module) {
        return module;
      }
    }
  }
  LOG_ERROR_CORE("Failed to find plugin: " + file_name + " in " + plugin_path_.string());
  return {};
}

nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::only_load_module(const std::string &module, const std::string &alias,
                                                                                             bool &loaded) {
  // Held here as well as in add_plugin, so the duplicate check and the append
  // that follows it in add_plugin cannot be split by another loader: two
  // threads both passing find_duplicate for the same file ended up with two
  // instances of one module - two listeners on one port, two collectors - and
  // only one of them findable by name afterwards.
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  loaded = false;
  boost::optional<boost::filesystem::path> real_file = find_file(module);
  if (!real_file) {
    return {};
  }
  LOG_DEBUG_CORE_STD("Loading module " + real_file.value().string() + " (" + alias + ")");
  plugin_type dup = plugin_list_.find_duplicate(real_file.value(), alias);
  if (dup) {
    return dup;
  }
  loaded = true;
  if (boost::algorithm::ends_with(real_file.value().string(), ".zip")) {
    return std::make_shared<zip_plugin>(plugin_list_.get_next_id(), real_file.value().lexically_normal(), alias, path_, shared_from_this(), log_instance_);
  }
  return std::make_shared<dll_plugin>(plugin_list_.get_next_id(), real_file.value().lexically_normal(), alias);
}

/**
 * Load and add a plugin to various internal structures
 * @param plugin The plug-in instance to load. The pointer is managed by the
 */
nsclient::core::plugin_manager::plugin_type nsclient::core::plugin_manager::add_plugin(const std::string &file_name, const std::string &alias) {
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
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
    if (plugin->hasFactsFetcher()) {
      facts_fetchers_.add_plugin(plugin);
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
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  const plugin_type plugin = plugin_list_.find_by_alias(module);
  if (plugin) {
    LOG_DEBUG_CORE_STD(std::string("Reloading: ") + plugin->get_alias_or_name());
    plugin->load_plugin(NSCAPI::reloadStart);
    if (plugin->reload_raced()) {
      LOG_ERROR_CORE_STD("Reloaded " + plugin->get_alias_or_name() + " while calls into it were still running: a check held it for over 5s");
    }
    return true;
  }
  LOG_ERROR_CORE("Failed to reload plugin " + module);
  return false;
}

bool nsclient::core::plugin_manager::is_dispatching_on_this_thread(const std::string &module) {
  if (!module.empty()) {
    const plugin_type plugin = plugin_list_.find_by_alias(module);
    return plugin && plugin->is_dispatching_on_this_thread();
  }
  for (const plugin_type &plugin : plugin_list_.get_plugins()) {
    if (plugin && plugin->is_dispatching_on_this_thread()) return true;
  }
  return false;
}

bool nsclient::core::plugin_manager::remove_plugin(const std::string &name) {
  const boost::recursive_mutex::scoped_lock lifecycle(lifecycle_mutex_);
  const plugin_type plugin = plugin_list_.find_by_module(name);
  if (!plugin) {
    LOG_ERROR_CORE("Module " + name + " was not found.");
    return false;
  }
  // The request asking for this is being served by the module it names: the
  // web server unloading WEBServer, a script unloading the module hosting it.
  // Removing it now drops the last reference, and the destructor unmaps the
  // library while this thread is still executing inside it - the call returns
  // to an address that is no longer mapped. The module also has to stop its
  // own listener from that listener's thread on the way, which joins the
  // calling thread with itself.
  if (plugin->is_dispatching_on_this_thread()) {
    LOG_ERROR_CORE_STD("Refused to unload " + plugin->get_alias_or_name() + ": the request asking for it is being served by that module");
    return false;
  }
  unsigned int plugin_id = plugin->get_id();
  // Unload before deregistering from the registries a refusal could not put
  // back: commands, channels and event subscriptions are what the module
  // itself registered while loading. The module can refuse - calls into it
  // may still be in flight - and it then keeps running and serving.
  // Deregistering those first meant such a refusal left it half removed:
  // still loaded and answering nothing, gone from every registry, no longer
  // findable by name to retry, and still listed as loaded in the cache
  // because the throw skipped that line.
  //
  // The walk lists (metrics fetchers and submitters, facts fetchers) and the
  // log subscription are the exception, and go first: they are closed, so
  // no round or log line reaches into the module after it is unloaded, and
  // the waits see out the ones already inside it. Unloading first let a
  // round call an unloaded module, log "Library is not loaded" for it, and
  // on the facts path mark it failing and later fixed. The waits are per
  // module - a round stuck in another module, or blocked on the lifecycle
  // lock this call holds, is not inside this one and is not waited for -
  // and the three lists share one deadline. A refusal reopens them,
  // so the module keeps its place in every list.
  const std::vector<walk_closing> walks = close_walks(plugin_id);
  const auto restore_walks = [&walks]() {
    for (const walk_closing &w : walks) w.slot.reopen();
  };
  for (const walk_closing &w : walks) {
    if (w.drained) continue;
    restore_walks();
    LOG_ERROR_CORE_STD("Refused to unload " + name + ": a metrics or facts round is still running inside it after 10 s");
    return false;
  }
  // A log line is not a dispatch, so unload_plugin does not wait for one,
  // and the unloaded flag it sets only stops lines that have not reached the
  // module yet (dll_plugin::handleMessage). close_subscriber closes the
  // module's subscription in place and waits until the logger's delivery
  // thread is no longer inside it, so the line has left before the module
  // is torn down; one that does not leave within the wait refuses the
  // unload, as a dispatch that does not finish would. A refusal reopens the
  // subscription where it was, before the refusal is logged, so the module -
  // still loaded, and still serving - sees that line too, and its place in
  // the fan-out is kept. A refused close leaves it closed in place, so a
  // retried unload - or a purge, or shutdown - waits for that line again.
  // What this cannot see is the module unloading itself from inside its own
  // log handler: the unload then runs on the delivery thread, which does not
  // wait for itself, and is_dispatching_on_this_thread() does not count log
  // lines, so such a module is torn down under its handler as it always was.
  // clear_subscribers() at shutdown runs under lifecycle_mutex_
  // (stop_plugins), so it cannot slip in between.
  const logging::unsubscribe_result unsubscribed = log_instance_->close_subscriber(plugin);
  const auto restore = [&]() {
    if (unsubscribed.removed) log_instance_->reopen_subscriber(plugin);
    restore_walks();
  };
  if (unsubscribed.delivering) {
    restore();
    LOG_ERROR_CORE_STD("Refused to unload " + name + ": a log line is still being handled by it after 5 s");
    return false;
  }
  try {
    plugin->unload_plugin();
  } catch (const plugin_exception &e) {
    restore();
    LOG_ERROR_CORE_STD("Failed to unload " + name + ": " + e.reason());
    return false;
  }
  if (unsubscribed.removed) log_instance_->drop_subscriber(plugin);
  std::vector<plugin_type> released;
  for (const walk_closing &w : walks) released.push_back(w.slot.finish(true));
  plugin_list_.remove(plugin_id);
  commands_.remove_plugin(plugin_id);
  channels_.remove_plugin(plugin_id);
  event_subscribers_.remove_plugin(plugin_id);
  // Whatever this module contributed to the inventory goes with it: a frozen
  // fact set from a module that is no longer running is worse than none.
  if (facts_) facts_->remove_owned_by(static_cast<unsigned int>(plugin_id));
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
//                            unconditionally by core_helper. Resolved
//                            here to a module name via the trusted
//                            plugin_cache.
//   nscp.principal         - sub-identity (web user, NRPE client tag,
//                            CLI OS user, ...). Optional; empty when
//                            unset.
//
// Both keys are only trustworthy for a request built *in process*: a
// module calling through core_helper cannot set them to anything else
// without rewriting core_helper. They are NOT trustworthy for a request
// whose bytes came off the wire - whoever composed the protobuf composed
// its header too. No endpoint hands a caller-supplied QueryRequestMessage
// to core->query any more (the raw-protobuf route that did was removed);
// every HTTP path now builds the message itself and stamps the keys from
// the authenticated session, as query_controller::stamp_identity does.
// Anything that reintroduces a pass-through must do the same, or refuse a
// request that carries them - otherwise the caller picks its own subject.
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
  // Denials come back as an ordinary UNKNOWN payload (execute_query still
  // returns isSuccess), which callers that FORWARD results somewhere else -
  // Scheduler's run_schedules, CheckHelpers' check_and_forward - must be able
  // to tell apart from a check that legitimately returned UNKNOWN: a denial
  // must never be submitted to a monitoring channel as if it were the
  // check's result. The message text is not a contract, so stamp a
  // machine-readable marker in the response header instead (one entry per
  // denied command; batch queries can mix denied and dispatched payloads).
  auto *marker = response_message->mutable_header()->add_metadata();
  marker->set_key("nscp.query_denied");
  marker->set_value(command);
}

namespace {
// How deep a single caller-initiated query may re-enter the plugin stack.
//
// A check that dispatches another check (CheckHelpers' check_multi,
// check_and_forward, check_timeout, the Scheduler's on-demand runs) calls back
// into the core on the SAME OS thread, and nothing used to bound that. A
// caller who is allowed to pass arguments could hand check_multi an argument
// that nests check_multi a few thousand levels deep in one string; every level
// pushes a full handler frame - protobuf messages, an options_description, the
// filter machinery - so the thread stack runs out and the process dies. Stack
// exhaustion is not a catchable exception on Windows, so there is no recovering
// from it after the fact: the depth has to be refused before the frame is
// pushed. Same failure class as the filter-expression depth cap.
//
// 16 is far above anything real (a wrapper around a wrapper is depth 2-3) and
// far below what any thread stack has trouble with.
const unsigned int max_query_depth = 16;

// Depth of the query dispatch currently running on THIS thread. Thread-local
// because the nesting is per-thread recursion: two unrelated checks running
// concurrently on different threads must not see each other's depth, and a
// check that hands work to a new thread (check_timeout) legitimately starts
// over - that thread carries its own stack.
thread_local unsigned int query_depth = 0;

struct query_depth_guard {
  query_depth_guard() { ++query_depth; }
  ~query_depth_guard() { --query_depth; }
  query_depth_guard(const query_depth_guard &) = delete;
  query_depth_guard &operator=(const query_depth_guard &) = delete;
};

// Name the commands a request asks for, for the refusal message.
std::string describe_requested_commands(const PB::Commands::QueryRequestMessage &request_message) {
  if (!request_message.header().command().empty()) return request_message.header().command();
  std::string commands;
  for (int i = 0; i < request_message.payload_size(); i++) {
    str::format::append_list(commands, request_message.payload(i).command());
  }
  return commands.empty() ? std::string("(no command)") : commands;
}
}  // namespace

NSCAPI::nagiosReturn nsclient::core::plugin_manager::execute_query(const std::string &request, std::string &response) {
  try {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryResponseMessage response_message;
    request_message.ParseFromString(request);

    // Before anything is dispatched: the frames this call would push are the
    // resource being protected, so the check has to come first.
    const query_depth_guard depth_guard;
    if (query_depth > max_query_depth) {
      const std::string commands = describe_requested_commands(request_message);
      LOG_ERROR_CORE_STD("Refusing to dispatch '" + commands + "': commands nested more than " + str::xtos(max_query_depth) +
                         " deep. A check that runs other checks (check_multi, check_and_forward, check_timeout) re-enters the core on this thread, and an "
                         "unbounded nesting would exhaust the thread stack.");
      PB::Commands::QueryResponseMessage::Response *payload = response_message.add_payload();
      payload->set_command(commands);
      nscapi::protobuf::functions::set_response_bad(
          *payload, "Command nesting too deep (limit " + str::xtos(max_query_depth) + "): a check that runs other checks cannot recurse indefinitely");
      response = response_message.SerializeAsString();
      return NSCAPI::cmd_return_codes::isSuccess;
    }

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
  // Only to give a named module a chance to be loaded on demand: the query
  // itself is dispatched through the command registry below (query_helper is a
  // no-op). Without a module there is nothing to load, and running it anyway
  // appended a bogus "No module was specified..." line to the result of every
  // `nscp client --query <command>` invocation that did not name one.
  // Always overwritten below (the command registry decides the outcome); this
  // is just a defined starting value.
  int ret = NSCAPI::cmd_return_codes::isSuccess;
  if (!module.empty()) {
    ret = load_and_run(
        module, [command, arguments, request, &responses](auto plugin) { return query_helper(plugin, command, arguments, request, &responses); }, errors);
  }

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
              // isSuccess, as every other handled request below: this used to
              // be exec_return_codes::returnOK, which is 0 - hasFailed in this
              // API - so a caller asking `any` module read its success as a
              // failure.
              return NSCAPI::cmd_return_codes::isSuccess;
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
  // One reply string per handler invocation. Handing every handler the same
  // `response` string would let each one overwrite the previous handler's
  // reply, so with channel=NSCA,GRAPHITE an NSCA failure was silently masked
  // by GRAPHITE's OK - the caller has to see every handler's verdict to be
  // able to report a partial failure.
  std::list<std::string> replies;
  for (std::string cur_chan : str::utils::split_lst(schannel, std::string(","))) {
    if (cur_chan == "noop") {
      found = true;
      std::string reply;
      nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", reply);
      replies.push_back(reply);
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
      std::string reply;
      nscapi::protobuf::functions::create_simple_submit_response_ok(cur_chan, "TODO", "seems ok", reply);
      replies.push_back(reply);
      continue;
    }
    try {
      for (nsclient::plugin_type p : channels_.get(cur_chan)) {
        std::string reply;
        try {
          p->handleNotification(cur_chan.c_str(), request, reply);
        } catch (...) {
          LOG_ERROR_CORE("Plugin throw exception: " + p->get_alias_or_name());
        }
        if (!reply.empty()) replies.push_back(reply);
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
  if (replies.size() == 1) {
    // The single-handler case keeps the reply byte-for-byte as the handler
    // built it (header included) - the overwhelmingly common case, and the
    // one parse_simple_submit_response is written for.
    response = replies.front();
  } else {
    // Multiple handlers: merge every reply's payloads into one message so no
    // verdict is lost. Callers submitting to a comma list of channels parse
    // this with parse_multi_submit_response.
    PB::Commands::SubmitResponseMessage merged;
    for (const std::string &reply : replies) {
      PB::Commands::SubmitResponseMessage msg;
      if (!msg.ParseFromString(reply)) continue;
      if (!merged.has_header() && msg.has_header()) merged.mutable_header()->CopyFrom(msg.header());
      for (int i = 0; i < msg.payload_size(); i++) merged.add_payload()->CopyFrom(msg.payload(i));
    }
    response = merged.SerializeAsString();
  }
  return NSCAPI::api_return_codes::isSuccess;
}

std::list<nsclient::core::plugin_manager::plugin_type> nsclient::core::plugin_manager::event_subscribers_for(
    const PB::Commands::EventMessage &message, const std::function<std::list<plugin_type>(const std::string &)> &lookup,
    std::list<std::string> *unmatched) {
  std::list<plugin_type> ret;
  std::set<unsigned int> seen;
  for (const PB::Commands::EventMessage::Request &r : message.payload()) {
    bool matched = false;
    for (const plugin_type &p : lookup(r.event())) {
      if (!p) continue;
      matched = true;
      if (seen.insert(p->get_id()).second) ret.push_back(p);
    }
    if (!matched && unmatched) unmatched->push_back(r.event());
  }
  return ret;
}

NSCAPI::errorReturn nsclient::core::plugin_manager::emit_event(const std::string &request) {
  PB::Commands::EventMessage em;
  em.ParseFromString(request);
  std::list<plugin_type> subscribers;
  std::list<std::string> unmatched;
  try {
    subscribers = event_subscribers_for(em, [this](const std::string &event) { return event_subscribers_.get(event); }, &unmatched);
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
  for (const std::string &event : unmatched) {
    LOG_DEBUG_CORE("No handler for event: " + event);
  }
  // Once per subscriber: each one gets the whole message and walks its lines.
  for (const plugin_type &p : subscribers) {
    try {
      p->on_event(request);
    } catch (const std::exception &e) {
      LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE("Failed to emit event to " + p->get_alias_or_name() + ": UNKNOWN EXCEPTION");
    }
  }
  return NSCAPI::api_return_codes::isSuccess;
}

struct metrics_fetcher {
  PB::Metrics::MetricsMessage result;
  std::string buffer;
  nsclient::logging::logger_instance log_;
  explicit metrics_fetcher(nsclient::logging::logger_instance log) : log_(std::move(log)) { result.add_payload(); }

  PB::Metrics::MetricsMessage::Response *get_root() { return result.mutable_payload(0); }
  void add_bundle(const PB::Metrics::MetricsBundle &b) { get_root()->add_bundles()->CopyFrom(b); }
  // One module failing must not cost the round: the walk runs on a copy of
  // the fetcher list, so a module unloaded since the copy was taken is still
  // in it, and its fetchMetrics throws "Library is not loaded". Let that
  // escape and every fetcher after it is skipped and no submitter runs.
  void fetch(nsclient::plugin_type p) {
    std::string local_buffer;
    try {
      p->fetchMetrics(local_buffer);
    } catch (const nsclient::core::plugin_exception &e) {
      log_->error("core", __FILE__, __LINE__, "Failed to fetch metrics from " + p->get_alias_or_name() + ": " + e.reason());
      return;
    } catch (const std::exception &e) {
      log_->error("core", __FILE__, __LINE__, "Failed to fetch metrics from " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
      return;
    }
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
  void digest(nsclient::plugin_type p) const {
    try {
      p->submitMetrics(buffer);
    } catch (const nsclient::core::plugin_exception &e) {
      log_->error("core", __FILE__, __LINE__, "Failed to submit metrics to " + p->get_alias_or_name() + ": " + e.reason());
    } catch (const std::exception &e) {
      log_->error("core", __FILE__, __LINE__, "Failed to submit metrics to " + p->get_alias_or_name() + ": " + utf8::utf8_from_native(e.what()));
    }
  }
};

bool nsclient::core::plugin_manager::is_enabled(const std::string module) { return parse_plugin(module).enabled; }

PB::Metrics::MetricsMessage nsclient::core::plugin_manager::process_metrics(PB::Metrics::MetricsBundle bundle) {
  metrics_fetcher f(log_instance_);
  metrics_fetchers_.do_all([&f](auto key) { return f.fetch(key); });
  f.get_root()->add_bundles()->CopyFrom(bundle);
  f.render();
  metrics_submitters_.do_all([&f](auto key) { return f.digest(key); });
  return f.result;
}

namespace {
std::string utc_now() { return boost::posix_time::to_iso_extended_string(boost::posix_time::second_clock::universal_time()) + "Z"; }
}  // namespace

void nsclient::core::plugin_manager::log_fact_problem_once(const std::string &key, const std::string &message, std::set<std::string> &failing) {
  failing.insert(key);
  {
    const boost::mutex::scoped_lock lock(fact_errors_mutex_);
    const std::map<std::string, std::string>::const_iterator it = logged_fact_errors_.find(key);
    if (it != logged_fact_errors_.end() && it->second == message) return;
    logged_fact_errors_[key] = message;
  }
  LOG_WARN_CORE_STD("facts: " + key + ": " + message);
}

void nsclient::core::plugin_manager::forget_fixed_fact_problems(const std::set<std::string> &failing) {
  const boost::mutex::scoped_lock lock(fact_errors_mutex_);
  for (std::map<std::string, std::string>::iterator it = logged_fact_errors_.begin(); it != logged_fact_errors_.end();) {
    if (failing.count(it->first) > 0) {
      ++it;
    } else {
      it = logged_fact_errors_.erase(it);
    }
  }
}

std::string nsclient::core::plugin_manager::apply_facts_response(const std::string &response, const unsigned int plugin_id, fact_repository &facts,
                                                                 std::map<std::string, std::string> &errors, std::set<std::string> &produced) {
  // An empty buffer is not an empty inventory: a module that answered with
  // nothing at all has not told us it stopped producing anything, so this
  // reads as a failed round and prunes nothing.
  if (response.empty()) return "returned no facts document";
  PB::Facts::FactsMessage message;
  if (!message.ParseFromString(response)) return "returned facts that are not a valid facts message";
  if (message.payload_size() == 0) return "returned a facts message with no payload";
  const PB::Facts::FactsMessage::Response &payload = message.payload(0);

  // A module that could not collect at all says so through the result - the
  // generated glue does this when a producer throws. Nothing it holds is
  // pruned, because "I failed" is not "I no longer produce this".
  if (payload.result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
    const std::string reported = payload.result().message();
    return reported.empty() ? "reported a failed facts round" : reported;
  }

  for (const PB::Facts::FactSet &set : payload.sets()) {
    const std::string &id = set.id();
    if (id.empty()) continue;
    // Removed drops the set (the docker socket went away), and is
    // deliberately not a claim to produce it.
    if (set.removed()) {
      facts.remove(id);
      continue;
    }
    // Every other mention is a claim to produce the set, including one that
    // only carries an error: that is what keeps a set which is enabled but
    // failing from being pruned along with the ones that were switched off.
    produced.insert(id);
    // What the producer could not collect this round. Reported next to the
    // document so a consumer can tell "not collected" from "nothing to
    // report".
    if (!set.error().empty()) errors[id] = set.error();
    // An error with no document is "keep what you have": the set is not
    // replaced with the empty object the message would otherwise hand us -
    // but it may still carry a fresher age for what we already hold.
    if (!set.has_facts()) {
      facts.mark_gathered(id, set.gathered());
      continue;
    }
    std::string error;
    if (facts.set(id, plugin_id, set.facts(), error) == fact_repository::set_result::rejected) {
      errors[id] = error;
      continue;
    }
    // Whatever set() decided - stored or unchanged - the producer has just
    // told us how old these values are, and an unchanged set is the case
    // this matters most for: a cached snapshot keeps its original age
    // instead of looking as fresh as the round that delivered it.
    facts.mark_gathered(id, set.gathered());
  }
  return "";
}

void nsclient::core::plugin_manager::collect_facts_from(const plugin_type &plugin, const std::string &request, std::map<std::string, std::string> &errors,
                                                        std::set<std::string> &failing) {
  const std::string module = plugin->get_alias_or_name();
  std::string response;
  try {
    plugin->fetchFacts(request, response);
  } catch (const plugin_exception &e) {
    log_fact_problem_once("module " + module, e.reason(), failing);
    return;
  } catch (const std::exception &e) {
    log_fact_problem_once("module " + module, utf8::utf8_from_native(e.what()), failing);
    return;
  }

  std::map<std::string, std::string> reported;
  std::set<std::string> produced;
  const std::string failure = apply_facts_response(response, plugin->get_id(), *facts_, reported, produced);
  if (!failure.empty()) {
    log_fact_problem_once("module " + module, failure, failing);
    return;
  }
  // The round completed, so what this module did not mention it no longer
  // produces: that is how a fact set turned off in a module's configuration
  // leaves the document.
  facts_->retain_only(plugin->get_id(), produced);
  for (const std::pair<const std::string, std::string> &problem : reported) {
    errors[problem.first] = problem.second;
    log_fact_problem_once(problem.first, problem.second, failing);
  }
}

std::vector<std::string> nsclient::core::plugin_manager::get_loaded_modules() {
  std::vector<std::string> modules;
  for (const plugin_type &plugin : plugin_list_.get_plugins()) {
    if (plugin) modules.push_back(plugin->getModule());
  }
  return modules;
}

void nsclient::core::plugin_manager::process_facts(const std::string &reason) {
  if (!facts_) return;
  // Every producer is asked; what it returns is what its own configuration
  // says it may produce. `reason` (startup, scheduled, reload, manual) is all
  // the core has to say, and lets an expensive collector hand back its last
  // snapshot instead of collecting again.
  PB::Facts::FactsQueryMessage request;
  request.add_payload()->set_reason(reason);
  const std::string request_string = request.SerializeAsString();
  std::map<std::string, std::string> errors;
  std::set<std::string> failing;
  facts_fetchers_.do_all([this, &request_string, &errors, &failing](plugin_type plugin) { collect_facts_from(plugin, request_string, errors, failing); });
  forget_fixed_fact_problems(failing);
  facts_->set_errors(errors);
  facts_->mark_collected(utc_now());
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
