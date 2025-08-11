// #define _WIN32_WINNT 0x0500

// clang-format off
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
// clang-format on

#include <Sddl.h>
#include <config.h>
#include <msi.h>
#include <msiquery.h>

#include <boost/algorithm/string.hpp>
#include <error/error.hpp>
#include <file_helpers.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/utils.hpp>
#include <str/wstring.hpp>
#include <str/xtos.hpp>
#include <string>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "installer_helper.hpp"
#include "keys.hpp"

const UINT COST_SERVICE_INSTALL = 2000;

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description,
             std::wstring service_deps);
bool uninstall(msi_helper &h, std::wstring service_name);

void copy_file(msi_helper &h, std::wstring source, std::wstring target) {
  if (boost::filesystem::is_regular_file(utf8::cvt<std::string>(source))) {
    h.logMessage(L"Copying: " + source + L" to " + target);
    if (!CopyFile(source.c_str(), target.c_str(), FALSE)) {
      h.errorMessage(L"Failed to copy file: " + utf8::cvt<std::wstring>(error::lookup::last_error()));
    }
  } else {
    h.logMessage(L"Copying failed: " + source + L" to " + target + L" source was not found.");
  }
}

class msi_logger : public nsclient::logging::logger {
  std::wstring error_;
  std::list<std::wstring> log_;
  msi_helper *h;

 public:
  msi_logger(msi_helper *h) : h(h) {}

  bool should_trace() const { return false; }
  bool should_debug() const { return false; }
  bool should_info() const { return true; }
  bool should_warning() const { return true; }
  bool should_error() const { return true; }
  bool should_critical() const { return true; }

  virtual void set_log_level(std::string level) {
    // ignored
  }
  std::string get_log_level() const { return "info"; }

  void debug(const std::string &module, const char *file, const int line, const std::string &message) { do_log("debug: " + message); }
  void trace(const std::string &module, const char *file, const int line, const std::string &message) {}
  void info(const std::string &module, const char *file, const int line, const std::string &message) { do_log("info: " + message); }
  void warning(const std::string &module, const char *file, const int line, const std::string &message) { do_log("warning: " + message); }
  void error(const std::string &module, const char *file, const int line, const std::string &message) { do_log("error: " + message); }
  void critical(const std::string &module, const char *file, const int line, const std::string &message) { do_log("error: (critical) " + message); }
  void raw(const std::string &message) { do_log(message); }

  void do_log(const std::string data) {
    std::wstring str = utf8::cvt<std::wstring>(data);
    if (str.empty()) return;
    h->setLastLog(str);
    if (boost::algorithm::starts_with(str, L"error:")) {
      h->errorMessage(str);
      if (!error_.empty()) error_ += L"\n";
      error_ += str.substr(6);
    }
    log_.push_back(str);
  }
  void asynch_configure() {}
  void synch_configure() {}
  bool startup() { return true; }
  bool shutdown() { return true; }

  std::wstring get_error() { return error_; }
  bool has_errors() { return !error_.empty(); }
  std::list<std::wstring> get_errors() { return log_; }

  void nsclient::logging::logger::add_subscriber(nsclient::logging::logging_subscriber_instance) {}
  void nsclient::logging::logger::clear_subscribers(void) {}
  void nsclient::logging::logger::destroy(void) {}
  void nsclient::logging::logger::configure(void) {}
  void nsclient::logging::logger::set_backend(std::string) {}
};

void nsclient::logging::log_message_factory::log_fatal(std::string message) {}

std::string nsclient::logging::log_message_factory::create_critical(const std::string &module, const char *file, const int line, const std::string &message) {
  return "critical: " + message;
}
std::string nsclient::logging::log_message_factory::create_error(const std::string &module, const char *file, const int line, const std::string &message) {
  return "error: " + message;
}
std::string nsclient::logging::log_message_factory::create_warning(const std::string &module, const char *file, const int line, const std::string &message) {
  return "warning: " + message;
}
std::string nsclient::logging::log_message_factory::create_info(const std::string &module, const char *file, const int line, const std::string &message) {
  return "info: " + message;
}
std::string nsclient::logging::log_message_factory::create_debug(const std::string &module, const char *file, const int line, const std::string &message) {
  return "debug: " + message;
}
std::string nsclient::logging::log_message_factory::create_trace(const std::string &module, const char *file, const int line, const std::string &message) {
  return "trace: " + message;
}

struct installer_settings_provider : public settings_manager::provider_interface {
  msi_helper *h;
  std::string basepath;
  std::string old_settings_map;
  boost::shared_ptr<msi_logger> logger;

  installer_settings_provider(msi_helper *h, std::wstring basepath, std::wstring old_settings_map)
      : h(h), basepath(utf8::cvt<std::string>(basepath)), old_settings_map(utf8::cvt<std::string>(old_settings_map)), logger(new msi_logger(h)) {}
  installer_settings_provider(msi_helper *h, std::wstring basepath)
      : h(h), basepath(utf8::cvt<std::string>(basepath)), old_settings_map(utf8::cvt<std::string>(old_settings_map)), logger(new msi_logger(h)) {}

  virtual std::string expand_path(std::string file) {
    str::utils::replace(file, "${base-path}", basepath);
    str::utils::replace(file, "${exe-path}", basepath);
    str::utils::replace(file, "${shared-path}", basepath);
    return file;
  }
  std::string get_data(std::string key) {
    if (!old_settings_map.empty() && key == "old_settings_map_data") {
      return old_settings_map;
    }
    return "";
  }

  std::wstring get_error() { return logger->get_error(); }
  bool has_errors() { return logger->has_errors(); }
  std::list<std::wstring> get_errors() { return logger->get_errors(); }

  nsclient::logging::logger_instance get_logger() const { return logger; }
};

static const wchar_t alphanum[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
std::wstring genpwd(const int len) {
  srand((unsigned)time(NULL));
  std::wstring ret;
  for (int i = 0; i < len; i++) ret += alphanum[rand() % ((sizeof(alphanum) / sizeof(wchar_t)) - 1)];
  return ret;
}

bool mod_enabled(std::string key) {
  std::string val = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, key, "0");
  return val == "enabled" || val == "1";
}
bool has_module(std::string key) { return settings_manager::get_settings()->has_key(MAIN_MODULES_SECTION, key); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Many options:
// - configuration not allowed		=> CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=0
// - target not found				=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config read		=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config NOT read => CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=1
//
// Interpretation:
// CONF_HAS_ERRORS=1	=> Dont allow anything (inform of issue)
// CONF_CAN_CHANGE=1	=> Allow change

std::wstring read_map_data(msi_helper &h) {
  std::wstring ret;
  PMSIHANDLE hView = h.open_execute_view(L"SELECT Data FROM Binary WHERE Name='OldSettingsMap'");
  if (h.isNull(hView)) {
    h.logMessage(L"Failed to query service view!");
    return L" ";
  }

  PMSIHANDLE hRec = h.fetch_record(hView);
  if (hRec != NULL) {
    ret = h.get_record_blob(hRec, 1);
    ::MsiCloseHandle(hRec);
  }
  ::MsiCloseHandle(hView);
  return ret;
}

void dump_config(msi_helper &h, std::wstring title) {
  h.dumpReason(title);
  for (const auto key :
       {ALLOWED_HOSTS, NSCLIENT_PWD, CONF_SCHEDULER, CONF_CHECKS, CONF_NRPE, CONF_NSCA, CONF_WEB, CONF_NSCLIENT, NRPEMODE, CONFIGURATION_TYPE, CONF_INCLUDES}) {
    h.dumpProperties(key);
  }
  h.dumpProperty(BACKUP_FILE);
  h.dumpProperty(INT_CONF_CAN_CHANGE);
  h.dumpProperty(INT_CONF_CAN_CHANGE_REASON);
  h.dumpProperty(INT_NSCP_ERROR);
  h.dumpProperty(INT_NSCP_ERROR_CONTEXT);
}

extern "C" UINT __stdcall DetectTool(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"DetectTool");

  try {
    h.logMessage("Detecting monitoring tool config");
    if (!boost::algorithm::trim_copy(h.getMsiPropery(OP5_SERVER)).empty()) {
      h.setPropertyValue(MONITORING_TOOL, MONITORING_TOOL_OP5);
    }
    std::wstring tool = h.getMsiPropery(MONITORING_TOOL);
    h.logMessage(L"Detected monitoring tool is: " + tool);
    dump_config(h, L"After DetectTool");
  } catch (installer_exception &e) {
    h.logMessage(L"Failed to detect monitoring tool: " + e.what());
    return ERROR_SUCCESS;
  } catch (nsclient::nsclient_exception &e) {
    h.logMessage(L"Failed to detect monitoring tool: " + utf8::cvt<std::wstring>(e.reason()));
    return ERROR_SUCCESS;
  } catch (...) {
    h.logMessage(L"Failed to detect monitoring tool: Unknown exception");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall ApplyTool(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ApplyTool");
  try {
    dump_config(h, L"Before ApplyTool");

    h.logMessage("Applying monitoring tool config");
    std::wstring tool = h.getMsiPropery(MONITORING_TOOL);

    if (tool == MONITORING_TOOL_OP5) {
      h.logMessage(L"Setting base config as Op5");
      h.setPropertyKeyAndDefault(NSCLIENT_PWD, L"", L"");
      h.setPropertyKeyAndDefault(CONF_CHECKS, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NRPE, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCA, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_WEB, L"", L"");
      h.setPropertyKeyAndDefault(CONF_NSCLIENT, L"1", L"");
      h.setPropertyKeyAndDefault(NRPEMODE, L"LEGACY", L"");

      h.setPropertyKeyAndDefault(CONF_INCLUDES, L"op5;op5.ini", L"");
      h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, L"registry://HKEY_LOCAL_MACHINE/software/NSClient++", L"");
      h.setFeatureLocal(L"OP5Montoring");
      h.setConfCanChange(true, L"Op5 applied");
    } else if (tool == L"GENERIC") {
      h.logMessage(L"Setting base config as Generic");
      h.setPropertyKeyAndDefault(ALLOWED_HOSTS, L"127.0.0.1", L"");

      h.setPropertyKeyAndDefault(NSCLIENT_PWD, genpwd(16), L"");
      h.setPropertyKeyAndDefault(CONF_CHECKS, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NRPE, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCA, L"", L"");
      h.setPropertyKeyAndDefault(CONF_WEB, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCLIENT, L"", L"");
      h.setPropertyKeyAndDefault(NRPEMODE, L"SECURE", L"");

      h.setPropertyKeyAndDefault(CONF_INCLUDES, L"", L"");
      h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini", L"");
      h.setFeatureAbsent(L"OP5Montoring");
      h.setConfCanChange(true, L"Generic applied");
    }

    h.setConfCanChange(true, L"Default config set from profile");
    h.setPropertyIfEmpty(CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini");

    dump_config(h, L"After ApplyTool");

  } catch (installer_exception &e) {
    h.logMessage(L"Failed to apply monitoring tool: " + e.what());
    return ERROR_SUCCESS;
  } catch (...) {
    h.logMessage(L"Failed to apply monitoring tool: Unknown exception");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall ImportConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ImportConfig");
  try {
    dump_config(h, L"Before ImportConfig");

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");

    std::string tls_version = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_VERSION"));
    std::string tls_verify_mode = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_VERIFY_MODE"));
    std::string tls_ca = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_CA"));
    if (tls_version.empty()) {
      tls_version = "1.3";
    }
    if (tls_verify_mode.empty()) {
      tls_verify_mode = "none";
    }
    if (tls_ca.empty()) {
      tls_ca = "";
    }

    std::wstring map_data = read_map_data(h);
    if (h.getMsiPropery(ALLOW_CONFIGURATION) == L"0") {
      h.setError(L"ImportConfig::1", L"Configuration is not allowed to change");
      h.setConfCanChange(false, L"Changes are not allowed");
      dump_config(h, L"After ImportConfig");
      return ERROR_SUCCESS;
    }

    auto wanted_context = h.getProperyValue(CONFIGURATION_TYPE);
    auto default_context = h.getProperyKey(CONFIGURATION_TYPE);
    auto context = wanted_context.empty() ? default_context : wanted_context;
    h.logMessage(L"Reading existing config using: " + context + L" tls version=" + utf8::cvt<std::wstring>(tls_version) + L", tls_verify=" +
                 utf8::cvt<std::wstring>(tls_verify_mode) + L", tls_ca=" + utf8::cvt<std::wstring>(tls_ca));

    installer_settings_provider provider(&h, target, map_data);
    if (!settings_manager::init_installer_settings(&provider, utf8::cvt<std::string>(context), tls_version, tls_verify_mode, tls_ca)) {
      h.setError(L"ImportConfig::init_installer_settings", L"Settings context had fatal errors");
      h.setConfHasErrors(L"Failed to load existing configuration");
      dump_config(h, L"After ImportConfig");
      return ERROR_SUCCESS;
    }
    if (provider.has_errors()) {
      h.logMessage(L"Settings context reported errors (debug log end)");
      for (std::wstring l : provider.get_errors()) {
        h.logMessage(l);
      }
      h.logMessage(L"Settings context reported errors (debug log end)");
      if (!settings_manager::has_boot_conf()) {
        h.logMessage(L"boot.conf was NOT found (so no new configuration)");
        if (settings_manager::context_exists(DEFAULT_CONF_OLD_LOCATION)) {
          h.setError(L"ImportConfig::has_boot_conf", std::wstring(L"Old configuration (") + utf8::cvt<std::wstring>(DEFAULT_CONF_OLD_LOCATION) +
                                                         L") was found but we got errors accessing it: " + provider.get_error());
          h.setConfHasErrors(L"Errors reading old configuration");
          dump_config(h, L"After ImportConfig");
          return ERROR_SUCCESS;
        } else {
          h.logMessage(L"Failed to read configuration but no configuration was found (so we are assuming there is no configuration).");
          h.setConfCanChange(true, L"Why do we ignore errors here?");
          dump_config(h, L"After ImportConfig");
          return ERROR_SUCCESS;
        }
      } else {
        h.setError(L"ImportConfig::has_errors", L"boot.conf was found but we got errors booting it: " + provider.get_error());
        h.setConfHasErrors(L"Errors during read config");
        dump_config(h, L"After ImportConfig");
        return ERROR_SUCCESS;
      }
    }

    h.logMessage(L"Previous configuration loaded correctly...");

    if (!settings_manager::get_settings()->supports_updates()) {
      h.applyPropertyValue(CONFIGURATION_TYPE);
      h.logMessage(L"Settings does not support updates");
      h.setConfCanChange(false, L"Using a settings system which do no support updates by installer");
      return ERROR_SUCCESS;
    }

    auto actual_context = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_context());
    h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, actual_context, actual_context);
    h.logMessage(L"Using configuration context: " + actual_context + L", " + utf8::cvt<std::wstring>(settings_manager::get_settings()->get_info()));
    if (!settings_manager::get_settings()->supports_updates()) {
      h.errorMessage(L"Updates not supported");
      h.setConfCanChange(false, L"Using a settings system which do no support updates by installer");
      return ERROR_SUCCESS;
    }
    if (!settings_manager::get_core()->supports_updates()) {
      h.errorMessage(L"Using a settings wtore which cannot be updated by installer");
      h.setConfCanChange(false, L"Settings store does not support updates by installer");
      return ERROR_SUCCESS;
    }

    if (settings_manager::get_core()->use_sensitive_keys()) {
      h.errorMessage(L"Using sensitive keys");
      h.setConfCanChange(false, L"Senstive keys cannot be updated by installer");
      return ERROR_SUCCESS;
    }

    h.logMessage(L"Configuration seems updatable...");

    h.setConfCanChange(true, L"Configuration seems good");

    h.logMessage(L"Applying old keys (as default values)...");

    if (settings_manager::get_settings()->has_key("/settings/default", "allowed hosts")) {
      auto old_allowed_hosts = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "allowed hosts", ""));
      h.setPropertyKeyAndDefault(ALLOWED_HOSTS, old_allowed_hosts, old_allowed_hosts);
    }
    if (settings_manager::get_settings()->has_key("/settings/default", "password")) {
      auto old_password = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", ""));
      h.setPropertyKeyAndDefault(NSCLIENT_PWD, old_password, old_password);
    }

    if (has_module("NRPEServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_NRPE, mod_enabled("NRPEServer"));
    }
    if (has_module("Scheduler")) {
      h.setPropertyKeyAndDefaultBool(CONF_SCHEDULER, mod_enabled("Scheduler"));
    }
    if (has_module("NSCAClient")) {
      h.setPropertyKeyAndDefaultBool(CONF_NSCA, mod_enabled("NSCAClient"));
    }
    if (has_module("NSClientServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_NSCLIENT, mod_enabled("NSClientServer"));
    }
    if (has_module("WEBServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_WEB, mod_enabled("WEBServer"));
    }

    if (settings_manager::get_settings()->has_key("/settings/NRPE/server", "insecure") ||
        settings_manager::get_settings()->has_key("/settings/NRPE/server", "verify mode")) {
      std::string insecure = settings_manager::get_settings()->get_string("/settings/NRPE/server", "insecure", "");
      std::string verify = settings_manager::get_settings()->get_string("/settings/NRPE/server", "verify mode", "");
      h.logMessage(L"Old NRPE insecure: " + utf8::cvt<std::wstring>(insecure));
      h.logMessage(L"Old NRPE verify: " + utf8::cvt<std::wstring>(verify));
      if (insecure == "true" || insecure == "1") {
        h.logMessage("Setting old NRPE mode legacy");
        h.setPropertyKeyAndDefault(NRPEMODE, L"LEGACY", L"");
      } else if (verify == "peer-cert") {
        h.logMessage("Setting old NRPE mode secure");
        h.setPropertyKeyAndDefault(NRPEMODE, L"SECURE", L"");
      } else {
        h.logMessage(L"Unknown old NRPE mode: " + h.getProperyKey(NRPEMODE));
      }
    }

    if (has_module("CheckSystem") || has_module("CheckDisk") || has_module("CheckEventLog") || has_module("CheckHelpers") ||
        has_module("CheckExternalScripts") || has_module("CheckNSCP")) {
      h.setPropertyKeyAndDefaultBool(CONF_CHECKS, mod_enabled("CheckSystem") && mod_enabled("CheckDisk") && mod_enabled("CheckEventLog") &&
                                                      mod_enabled("CheckHelpers") && mod_enabled("CheckExternalScripts") && mod_enabled("CheckNSCP"));
    }

    h.logMessage(L"Old keys applied");
    settings_manager::destroy_settings();

    h.logMessage(L"Determaining which keys have changed");
    h.applyPropertyValue(ALLOWED_HOSTS);
    h.applyPropertyValue(NSCLIENT_PWD);
    h.applyPropertyValue(CONF_SCHEDULER);
    h.applyPropertyValue(CONF_CHECKS);
    h.applyPropertyValue(CONF_NRPE);
    h.applyPropertyValue(CONF_NSCA);
    h.applyPropertyValue(CONF_WEB);
    h.applyPropertyValue(CONF_NSCLIENT);
    h.applyPropertyValue(NRPEMODE);

    h.applyPropertyValue(CONFIGURATION_TYPE);
    h.applyPropertyValue(CONF_INCLUDES);

    dump_config(h, L"After ImportConfig");

  } catch (installer_exception &e) {
    h.setError(L"ImportConfig::e1", L"Failed to read old configuration file: " + e.what());
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (nsclient::nsclient_exception &e) {
    h.setError(L"ImportConfig::e2", L"Failed to read old configuration file: " + utf8::cvt<std::wstring>(e.what()));
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (std::exception &e) {
    h.setError(L"ImportConfig::e3", L"Failed to read old configuration file: " + utf8::cvt<std::wstring>(e.what()));
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (...) {
    h.setError(L"ImportConfig::e4", L"Failed to read old configuration file: <Unknown exception>");
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool write_config(msi_helper &h, std::wstring path, std::wstring file);

void write_key(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring path, std::wstring key, std::wstring val) {
  data.write_int(mode);
  data.write_string(path);
  data.write_string(key);
  data.write_string(val);
  h.logMessage(L"write_key: " + path + L"." + key + L"=" + val);
}

void write_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring key, std::wstring val) {
  std::wstring path = utf8::cvt<std::wstring>(MAIN_MODULES_SECTION);
  if (val == L"1" || val == L"enabled") {
    write_key(h, data, mode, path, key, L"enabled");
  } else {
    write_key(h, data, mode, path, key, L"disabled");
  }
}

void write_changed_key(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring path, std::wstring key) {
  std::wstring val = h.getProperyKey(prop);
  if (!h.propertyNotDefault(prop)) {
    h.logMessage(L"IGNORING property not changed: " + prop + L"; " + path + L"." + key + L"=" + val);
    return;
  }
  h.logMessage(L"write_changed_key: " + prop + L"; " + path + L"." + key + L"=" + val);
  write_key(h, data, 1, path, key, val);
}

void write_changed_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring key) {
  std::wstring val = h.getProperyKey(prop);
  if (!h.propertyNotDefault(prop)) {
    h.logMessage(L"write_changed_key_mod: IGNORING property not changed: " + prop + L"; <modules>." + key + L"=" + val);
    return;
  }
  h.logMessage(L"write_changed_key_mod: " + prop + L"; <modules>." + key + L"=" + val);
  write_key_mod(h, data, 1, key, val);
}

bool write_property_if_set(msi_helper &h, msi_helper::custom_action_data_w &data, const std::wstring prop, std::wstring path, std::wstring key) {
  std::wstring val = boost::algorithm::trim_copy(h.getProperyKey(prop));
  if (!val.empty()) {
    h.logMessage(L"write_property_if_set: " + prop + L"; <modules>." + key + L"=" + val);
    write_key(h, data, 1, path, key, val);
    return true;
  } else {
    h.logMessage(L"IGNORING property not set: " + prop + L"; " + path + L"." + key + L"=" + val);
  }
  return false;
}

extern "C" UINT __stdcall BackupConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"BackupConfig");
  try {
    dump_config(h, L"Before BackupConfig");

    if (h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      h.logMessage(L"Configuration changes not allowed: set CONF_CAN_CHANGE=1");
      return ERROR_SUCCESS;
    }

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");
    boost::filesystem::wpath backup = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::wpath target_path = target;
    boost::filesystem::wpath config_file = target_path / L"nsclient.ini";
    if (boost::filesystem::exists(config_file)) {
      h.logMessage(L"Config file found: " + config_file.wstring());
      h.logMessage(L"Backup file: " + backup.wstring());
      copy_file(h, config_file.wstring(), backup.wstring());
      h.setPropertyValue(BACKUP_FILE, backup.wstring());
    }

  } catch (installer_exception &e) {
    h.errorMessage(L"Failed to install service: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to install service: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ScheduleWriteConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ScheduleWriteConfig");
  try {
    dump_config(h, L"Before ScheduleWriteConfig");

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");
    boost::filesystem::wpath backup = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::wpath target_path = target;
    boost::filesystem::wpath config_file = target_path / L"nsclient.ini";

    msi_helper::custom_action_data_w data;
    data.write_string(h.getTargetPath(L"INSTALLLOCATION"));
    data.write_string(h.getProperyKey(CONFIGURATION_TYPE));
    data.write_string(h.getMsiPropery(L"RESTORE_FILE"));
    data.write_string(h.getMsiPropery(BACKUP_FILE));

    data.write_string(h.getMsiPropery(L"TLS_VERSION"));
    data.write_string(h.getMsiPropery(L"TLS_VERIFY_MODE"));
    data.write_string(h.getMsiPropery(L"TLS_CA"));

    if (h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      h.logMessage(L"Configuration changes not allowed (only updating boot.ini): set CONF_CAN_CHANGE=1");
      data.write_int(0);

      if (data.has_data()) {
        h.logMessage(L"Scheduling (ExecWriteConfig): " + data.to_string());
        HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
        if (FAILED(hr)) {
          h.errorMessage(L"failed to schedule config update");
          return hr;
        }
      }

      return ERROR_SUCCESS;
    } else {
      h.logMessage(L"Configuration changes allowed (updating boot.ini)");
      data.write_int(1);
    }

    std::wstring confInclude = h.getProperyKey(CONF_INCLUDES);
    h.logMessage(L"Adding include: " + confInclude);
    if (!confInclude.empty()) {
      std::vector<std::wstring> lst;
      boost::split(lst, confInclude, boost::is_any_of(L";"));
      for (int i = 0; i + 1 < lst.size(); i += 2) {
        h.logMessage(L" + : " + lst[i] + L"=" + lst[i + 1]);
        write_key(h, data, 1, L"/includes", lst[i], lst[i + 1]);
      }
    }

    write_changed_key_mod(h, data, CONF_NRPE, L"NRPEServer");
    write_changed_key_mod(h, data, CONF_SCHEDULER, L"Scheduler");
    write_changed_key_mod(h, data, CONF_NSCA, L"NSCAClient");
    write_changed_key_mod(h, data, CONF_NSCLIENT, L"NSClientServer");
    write_changed_key_mod(h, data, CONF_WMI, L"CheckWMI");
    write_changed_key_mod(h, data, CONF_WEB, L"WEBServer");

    if (h.propertyNotDefault(CONF_CHECKS)) {
      std::wstring modval = h.getProperyKey(CONF_CHECKS);
      if (modval == L"1") {
        modval = L"enabled";
      } else {
        modval = L"disabled";
      }
      write_key_mod(h, data, 1, L"CheckSystem", modval);
      write_key_mod(h, data, 1, L"CheckDisk", modval);
      write_key_mod(h, data, 1, L"CheckEventLog", modval);
      write_key_mod(h, data, 1, L"CheckHelpers", modval);
      write_key_mod(h, data, 1, L"CheckExternalScripts", modval);
      write_key_mod(h, data, 1, L"CheckNSCP", modval);
    }
    if (h.getProperyKey(CONF_NRPE) == L"1") {
      if (h.propertyNotDefault(NRPEMODE)) {
        std::wstring mode = h.getProperyKey(NRPEMODE);
        write_key(h, data, 1, L"/settings/NRPE/server", L"ssl options", L"");
        write_key(h, data, 1, L"/settings/NRPE/server", L"tls version", L"tlsv1.2+");
        if (mode == L"LEGACY") {
          write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"true");
          write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"none");
        } else {
          write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"false");
          write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"peer-cert");
        }
      }
    }

    std::wstring defpath = L"/settings/default";
    write_changed_key(h, data, ALLOWED_HOSTS, defpath, L"allowed hosts");
    write_changed_key(h, data, NSCLIENT_PWD, defpath, L"password");

    std::wstring confSet = h.getMsiPropery(L"CONF_SET");
    h.logMessage(L"Adding conf: " + confSet);
    if (!confSet.empty()) {
      std::vector<std::wstring> lst;
      boost::split(lst, confSet, boost::is_any_of(L";"));
      for (int i = 0; i + 2 < lst.size(); i += 3) {
        h.logMessage(L" + : " + lst[i] + L" " + lst[i + 1] + L"=" + lst[i + 2]);
        write_key(h, data, 1, lst[i], lst[i + 1], lst[i + 2]);
      }
    }

    if (write_property_if_set(h, data, OP5_SERVER, L"/settings/op5", L"server")) {
      write_key(h, data, 1, L"/modules", L"OP5Client", L"enabled");
    }
    write_property_if_set(h, data, OP5_USER, L"/settings/op5", L"user");
    write_property_if_set(h, data, OP5_PASSWORD, L"/settings/op5", L"password");
    write_property_if_set(h, data, OP5_HOSTGROUPS, L"/settings/op5", L"hostgroups");
    write_property_if_set(h, data, OP5_CONTACTGROUP, L"/settings/op5", L"contactgroups");

    if (data.has_data()) {
      h.logMessage(L"Scheduling (ExecWriteConfig): " + data.to_string());
      HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
      if (FAILED(hr)) {
        h.errorMessage(L"failed to schedule config update");
        return hr;
      }
    }
  } catch (installer_exception &e) {
    h.errorMessage(L"Failed to install service: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to install service: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ExecWriteConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecWriteConfig");
  try {
    h.logMessage(L"RAW: " + h.getMsiPropery(L"CustomActionData"));
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    h.logMessage(L"Got CA data: " + data.to_string());
    std::wstring target = data.get_next_string();
    std::wstring context_w = data.get_next_string();
    std::string context = utf8::cvt<std::string>(context_w);
    std::wstring restore = data.get_next_string();
    std::wstring backup = data.get_next_string();

    std::wstring tls_version = data.get_next_string();
    std::wstring tls_verify_mode = data.get_next_string();
    std::wstring tls_ca = data.get_next_string();
    int update_nsclient_ini = data.get_next_int();

    h.logMessage(L"Target: " + target);
    h.logMessage("Context: " + context);
    h.logMessage(L"Restore: " + restore);
    h.logMessage(L"Backup: " + backup);
    h.logMessage(L"Update ns-client.ini: " + update_nsclient_ini ? L"Yes" : L"No");

    boost::filesystem::path target_path = target;
    boost::filesystem::path old_path = target_path / "nsc.ini.old";
    boost::filesystem::path legacy_config_path = target_path / "nsc.ini";

    boost::filesystem::path restore_path = restore;

    boost::filesystem::path backup_path = backup;

    if (boost::filesystem::exists(old_path)) h.logMessage(L"Found old (.old) file: " + strEx::xtos(boost::filesystem::file_size(old_path)));
    if (boost::filesystem::exists(legacy_config_path)) h.logMessage(L"Found legacy file: " + strEx::xtos(boost::filesystem::file_size(legacy_config_path)));

    if (boost::filesystem::exists(backup_path)) {
      h.logMessage(L"Found Backup file: " + strEx::xtos(boost::filesystem::file_size(backup_path)));
      boost::filesystem::path config_path = target_path / "nsclient.ini";
      h.logMessage(L"Restoring from backup: " + backup_path.wstring());
      copy_file(h, backup_path.wstring(), config_path.wstring());
      if (!boost::filesystem::remove(backup_path)) {
        h.errorMessage(L"Failed to remove backup file: " + backup_path.wstring());
      }
    }

    if (boost::filesystem::exists(restore_path)) {
      h.logMessage(L"Found restore file: " + strEx::xtos(boost::filesystem::file_size(restore_path)));
      h.logMessage(L"Restore path exists: " + restore);
      if (!boost::filesystem::exists(legacy_config_path)) {
        h.logMessage(L"Restoring nsc.ini configuration file");
        copy_file(h, restore_path.wstring(), legacy_config_path.wstring());
      }
      if (!boost::filesystem::exists(old_path)) {
        h.logMessage(L"Creating backup nsc.ini.old configuration file");
        copy_file(h, restore_path.wstring(), old_path.wstring());
      }
    }

    installer_settings_provider provider(&h, target);

    auto use_tls_version = tls_version.empty() ? "1.3" : utf8::cvt<std::string>(tls_version);
    auto use_tls_verify_mode = tls_verify_mode.empty() ? "none" : utf8::cvt<std::string>(tls_verify_mode);
    auto use_tls_ca = tls_ca.empty() ? "" : utf8::cvt<std::string>(tls_ca);

    h.logMessage(L"Writing existing config using: " + utf8::cvt<std::wstring>(context) + L" tls version=" + utf8::cvt<std::wstring>(use_tls_version) +
                 L", tls_verify=" + utf8::cvt<std::wstring>(use_tls_verify_mode) + L", tls_ca=" + utf8::cvt<std::wstring>(use_tls_ca));
    if (!settings_manager::init_installer_settings(&provider, context, use_tls_version, use_tls_verify_mode, use_tls_ca)) {
      h.errorMessage(L"Failed to boot settings when writing: " + provider.get_error());
      return ERROR_SUCCESS;
    }

    if (!tls_version.empty()) {
      h.logMessage(L"Setting boot.ini TLS version: " + tls_version);
      settings_manager::write_boot_ini_key("tls", "version", utf8::cvt<std::string>(tls_version));
    }
    if (!tls_verify_mode.empty()) {
      h.logMessage(L"Setting boot.ini TLS verify mode: " + tls_verify_mode);
      settings_manager::write_boot_ini_key("tls", "verify mode", utf8::cvt<std::string>(tls_verify_mode));
    }
    if (!tls_ca.empty()) {
      h.logMessage(L"Setting boot.ini TLS CA: " + tls_ca);
      settings_manager::write_boot_ini_key("tls", "ca", utf8::cvt<std::string>(tls_ca));
    }

    if (!update_nsclient_ini) {
      h.logMessage("Changing context: " + context);
      settings_manager::set_boot_ini_primary(context);

      h.logMessage(L"Not updating nsclient.ini");
      return ERROR_SUCCESS;
    }

    h.logMessage("Switching to: " + context);
    settings_manager::change_context(context);

    while (data.has_more()) {
      unsigned int mode = data.get_next_int();
      std::string path = utf8::cvt<std::string>(data.get_next_string());
      std::string key = utf8::cvt<std::string>(data.get_next_string());
      std::string val = utf8::cvt<std::string>(data.get_next_string());

      if (mode == 1) {
        h.logMessage("Set key: " + path + "/" + key + " = " + val);
        settings_manager::get_settings()->set_string(path, key, val);
      } else if (mode == 2) {
        h.logMessage("***UNSUPPORTED*** Remove key: " + path + "/" + key + " = " + val);
      } else {
        h.errorMessage(L"Unknown mode in CA data: " + strEx::xtos(mode) + L": " + data.to_string());
        return ERROR_INSTALL_FAILURE;
      }
    }
    h.logMessage("Saving settings, not updating existing keys: " + context);
    settings_manager::get_settings()->save(false);
  } catch (const installer_exception &e) {
    h.errorMessage(L"Failed to write configuration: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Failed to write configuration: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to write configuration: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall TranslateSid(MSIHANDLE hInstall) {
  TCHAR szSid[MAX_PATH] = {0};
  TCHAR szSidProperty[MAX_PATH] = {0};
  TCHAR szName[MAX_PATH] = {0};
  DWORD size = MAX_PATH;
  UINT ret = 0;
  ret = MsiGetProperty(hInstall, L"TRANSLATE_SID", szSid, &size);

  if (ret != ERROR_SUCCESS) {
    return 4444;
  }

  size = MAX_PATH;
  ret = MsiGetProperty(hInstall, L"TRANSLATE_SID_PROPERTY", szSidProperty, &size);

  if (ret != ERROR_SUCCESS) {
    return 4445;
  }

  PSID pSID = NULL;

  if (!ConvertStringSidToSid(szSid, &pSID)) {
    return 4446;
  }

  size = MAX_PATH;
  TCHAR szRefDomain[MAX_PATH] = {0};
  SID_NAME_USE nameUse;
  DWORD refSize = MAX_PATH;
  if (!LookupAccountSid(NULL, pSID, szName, &size, szRefDomain, &refSize, &nameUse)) {
    if (pSID != NULL) {
      LocalFree(pSID);
    }
    return 4447;
  }

  ret = MsiSetProperty(hInstall, szSidProperty, szName);
  if (!ConvertStringSidToSid(szSid, &pSID)) {
    if (pSID != NULL) {
      LocalFree(pSID);
    }
    return 4448;
  }

  if (pSID != NULL) {
    LocalFree(pSID);
  }
  return ERROR_SUCCESS;
}
