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

#include <config.h>

#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <settings/settings_core.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "NSClient++.h"
#include "cli_parser.hpp"
#include "core_api.h"
#include "logger/nsclient_logger.hpp"

#ifdef WIN32
#include <com_helpers.hpp>
#include <win/service_control.hpp>
com_helper::initialize_com com_helper_;
#endif

#ifdef WIN32
#include <breakpad/exception_handler_win32.hpp>
#endif

std::shared_ptr<NSClient> mainClient;  // Global core instance.

/**
 * Application startup point
 *
 * @param argc Argument count
 * @param argv[] Argument array
 * @param envp[] Environment array
 * @return exit status
 */
int nscp_main(int argc, char *argv[]);

#ifdef WIN32
int wmain(int argc, wchar_t *argv[], wchar_t *envp[]) {
  char **wargv = new char *[argc];
  for (int i = 0; i < argc; i++) {
    std::string s = utf8::cvt<std::string>(argv[i]);
    wargv[i] = new char[s.length() + 10];
    strncpy(wargv[i], s.c_str(), s.size() + 1);
  }
  int ret = nscp_main(argc, wargv);
  for (int i = 0; i < argc; i++) {
    delete[] wargv[i];
  }
  delete[] wargv;
  return ret;
}
#else
int main(int argc, char *argv[]) { return nscp_main(argc, argv); }
#endif

int nscp_main(int argc, char *argv[]) {
  try {
    mainClient.reset(new NSClient());
    srand(static_cast<unsigned>(time(nullptr)));
    cli_parser parser(mainClient);
    const int exit = parser.parse(argc, argv);
    return exit;
  } catch (const std::exception &e) {
    std::cerr << "Exception raised: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception raised" << std::endl;
    return 1;
  }
}

//////////////////////////////////////////////////////////////////////////
// Service functions

struct nscp_settings_provider : public settings_manager::provider_interface {
  nsclient::logging::logger_instance log_instance_;
  nsclient::core::path_instance path_;
  nscp_settings_provider(nsclient::core::path_instance path_, nsclient::logging::logger_instance log_instance) : log_instance_(log_instance), path_(path_) {}
  virtual ~nscp_settings_provider() {}

  virtual std::string expand_path(std::string file) { return path_->expand_path(file); }
  nsclient::logging::logger_instance get_logger() const { return log_instance_; }
};

nscp_settings_provider *provider_ = NULL;

NSClientT::NSClientT()
    : service_name_(DEFAULT_SERVICE_NAME),
      log_instance_(new nsclient::logging::impl::nsclient_logger()),
      path_(new nsclient::core::path_manager(log_instance_)),
      plugins_(new nsclient::core::plugin_manager(path_, log_instance_)),
      storage_manager_(new nsclient::core::storage_manager(path_, log_instance_)) {
  provider_ = new nscp_settings_provider(path_, log_instance_);
  log_instance_->startup();
}

NSClientT::~NSClientT() {
  try {
    delete provider_;
    log_instance_->destroy();
  } catch (...) {
    std::cerr << "UNknown exception raised: When destroying logger" << std::endl;
  }
}

namespace sh = nscapi::settings_helper;

/**
 * Initialize the program
 * @param boot true if we shall boot all plugins
 * @param attachIfPossible is true we will attach to a running instance.
 * @return success
 * @author mickem
 */
bool NSClientT::load_configuration_1() {
  // TODO: These are split temporarily to allow overriding log-path
#ifdef WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

  LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " Loading settings and logger...");

  if (!settings_manager::init_settings(provider_, context_)) {
    return false;
  }
  return true;
}

bool NSClientT::load_configuration_2(const bool override_log) {
  log_instance_->configure();

  LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " booting...");
  LOG_DEBUG_CORE("Booted settings subsystem...");

  bool crash_archive = false;
#ifdef WIN32
  bool crash_restart = false;
#endif
  bool use_credentials = false;
  std::string crash_folder, log_level;
  try {
    sh::settings_registry settings(settings_manager::get_proxy());

    // clang-format off
    settings.add_path()
      (MAIN_MODULES_SECTION, "MODULES", "A list of modules.")
      ("settings", "Settings", "Core configuration.")
    ;

    settings.add_path_to_settings()
      ("log", "LOG SETTINGS", "Section for configuring the log handling.")
      ("crash", "CRASH HANDLER", "Section for configuring the crash handler.")
      ("default", "Default values", "Default values used in other config sections.")
    ;
    // clang-format on

    settings.add_key_to_path("/settings")
        .add_bool("use credential manager", sh::bool_key(&use_credentials, false), "use credential manager",
                  "Store sensitive keys in use credential manager instead of ini file");

    settings.add_key_to_settings("log").add_string("level", sh::string_key(&log_level, "info"), "LOG LEVEL",
                                                   "Log level to use. Available levels are error,warning,info,debug,trace");

    settings.add_key_to_settings("crash")
        .add_bool("archive", sh::bool_key(&crash_archive, true), "ARCHIVE CRASHREPORTS", "Archive crash reports in the archive folder")
        .add_string("archive folder", sh::path_key(&crash_folder, CRASH_ARCHIVE_FOLDER), "CRASH ARCHIVE LOCATION", "The folder to archive crash dumps in");

    settings.register_all();
    settings.notify();
    if (use_credentials) {
      settings_manager::get_settings()->enable_credentials();
    }
  } catch (settings::settings_exception &e) {
    LOG_ERROR_CORE_STD("Could not find settings: " + utf8::utf8_from_native(e.what()));
  }
  if (!override_log) {
    log_instance_->set_log_level(log_level);
  }

#ifdef WIN32
  ExceptionManager::instance()->setup_app(APPLICATION_NAME, STRPRODUCTVER, STRPRODUCTDATE);

  if (crash_restart) {
    LOG_DEBUG_CORE("On crash: restart service");
    ExceptionManager::instance()->setup_restart_flag();
  }

  bool crashHandling = false;
  if (crash_archive) {
    ExceptionManager::instance()->setup_path(crash_folder);
    LOG_DEBUG_CORE("Archiving crash dumps in: " + crash_folder);
    crashHandling = true;
  }
  if (!crashHandling) {
    LOG_ERROR_CORE("No crash handling configured");
  } else {
    ExceptionManager::StartMonitoring();
  }
#endif

#ifdef WIN32
  try {
    com_helper_.initialize();
  } catch (com_helper::com_exception &e) {
    LOG_ERROR_CORE_STD("COM exception: " + e.reason());
    return false;
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception iniating COM...");
    return false;
  }
#endif

  boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
  if (!boost::filesystem::is_directory(pluginPath)) {
    const auto tmpPluginPath = path_->expand_path("${exe-path}/modules");
    if (boost::filesystem::is_directory(tmpPluginPath)) {
      LOG_WARN_CORE("Modules folder " + pluginPath.string() + " not found using " + tmpPluginPath + " instead.");
      pluginPath = tmpPluginPath;
    } else {
      LOG_ERROR_CORE("Failed to find modules folder: " + pluginPath.string());
      return false;
    }
  }
  plugins_->set_path(pluginPath);

  return true;
}
bool NSClientT::boot_load_active_plugins() {
  try {
    plugins_->load_active_plugins();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
    return false;
  }
  return true;
}

void NSClientT::boot_load_all_plugin_files() {
  try {
    plugins_->load_all_plugins();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
  }
}

bool NSClientT::boot_load_single_plugin(const std::string &plugin) {
  try {
    return plugins_->load_single_plugin(std::move(plugin));
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
  }
  return false;
}

bool NSClientT::boot_start_plugins(bool boot) {
  storage_manager_->load();
  try {
    plugins_->start_plugins(boot ? NSCAPI::normalStart : NSCAPI::dontStart);
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception loading plugins");
    return false;
  }
  if (boot) {
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "string", "settings maintenance interval", "Maintenance interval",
                                               "How often settings shall reload config if it has changed", "5m", true, false);
    std::string smi = settings_manager::get_settings()->get_string("/settings/core", "settings maintenance interval", "5m");
    scheduler_.add_task(task_scheduler::schedule_metadata::SETTINGS, smi);
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "string", "metrics interval", "Maintenance interval",
                                               "How often to fetch metrics from modules", "10s", true, false);
    smi = settings_manager::get_settings()->get_string("/settings/core", "metrics interval", "10s");
    scheduler_.add_task(task_scheduler::schedule_metadata::METRICS, smi);
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "int", "settings maintenance threads", "Maintenance thread count",
                                               "How many threads will run in the background to maintain the various core helper tasks.", "1", true, false);
    int count = str::stox<int>(settings_manager::get_settings()->get_string("/settings/core", "settings maintenance threads", "1"));
    scheduler_.set_threads(count);
    scheduler_.start();
  }
  try {
    if (boot) {
      plugins_->post_start_plugins();
    }
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception starting plugins");
    return false;
  }
  LOG_DEBUG_CORE(utf8::cvt<std::string>(APPLICATION_NAME " - " CURRENT_SERVICE_VERSION " Started!"));
  return true;
}

bool NSClientT::stop_nsclient() {
  scheduler_.stop();
  LOG_DEBUG_CORE("Attempting to stop all plugins");
  try {
    LOG_DEBUG_CORE("Stopping all plugins");
    unloadPlugins();
  } catch (nsclient::core::plugin_exception &e) {
    LOG_ERROR_CORE_STD("Exception raised when unloading non msg plguins: " + e.reason() + " in module: " + e.file());
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception raised when unloading non msg plugins");
  }
  storage_manager_->save();
#ifdef WIN32
  LOG_DEBUG_CORE("Stopping: COM helper");
  try {
    com_helper_.unInitialize();
  } catch (com_helper::com_exception &e) {
    LOG_ERROR_CORE_STD("COM exception: " + e.reason());
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception uniniating COM...");
  }
#endif
  LOG_DEBUG_CORE("Stopping: Settings instance");
  settings_manager::destroy_settings();
  try {
    log_instance_->shutdown();
    google::protobuf::ShutdownProtobufLibrary();
  } catch (...) {
    LOG_ERROR_CORE("UNknown exception raised: When stopping");
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
// Member functions

void NSClientT::unloadPlugins() {
  log_instance_->clear_subscribers();
  plugins_->stop_plugins();
}
void NSClientT::reloadPlugins() {
  plugins_->start_plugins(NSCAPI::reloadStart);
  boot_load_active_plugins();
  plugins_->start_plugins(NSCAPI::normalStart);
  // TODO: Figure out changed set and remove/add delete/added modules.
  settings_manager::get_core()->set_reload(false);
}

bool NSClientT::do_reload(const std::string module) {
  if (module == "settings") {
    try {
      settings_manager::get_settings()->clear_cache();
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
    }
  } else if (module == "service") {
    try {
      LOG_DEBUG_CORE_STD("Reloading all modules.");
      reloadPlugins();
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
    }
  } else {
    return plugins_->reload_plugin(module);
  }
  return false;
}

NSCAPI::errorReturn NSClientT::reload(const std::string module) {
  try {
    std::string task = module;
    bool delayed = false;
    if (module.size() > 8 && module.substr(0, 8) == "delayed,") {
      task = module.substr(8);
      delayed = true;
    } else if (module.size() > 6 && module.substr(0, 6) == "delay,") {
      task = module.substr(6);
      delayed = true;
    } else if (module.size() > 6 && module.substr(0, 8) == "instant,") {
      task = module.substr(8);
      delayed = false;
    } else if (module == "service") {
      delayed = false;
    }
    if (delayed) {
      LOG_TRACE_CORE("Delayed reload");
      scheduler_.add_task(task_scheduler::schedule_metadata::RELOAD, "", task);
      return NSCAPI::api_return_codes::isSuccess;
    } else {
      LOG_TRACE_CORE("Instant reload");
      return do_reload(task) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
    }

  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Reload failed: " + utf8::utf8_from_native(e.what()));
    return NSCAPI::api_return_codes::hasFailed;
  } catch (...) {
    LOG_ERROR_CORE("Reload failed");
    return NSCAPI::api_return_codes::hasFailed;
  }
}

// Service API
NSClient *NSClientT::get_global_instance() { return mainClient.get(); }
void NSClientT::handle_startup(std::string service_name) {
  LOG_DEBUG_CORE("Starting: " + service_name);
  service_name_ = service_name;
#ifdef WIN32
  ExceptionManager::instance()->setup_service_name(service_name);
#endif
  load_configuration_1();
  load_configuration_2();
  boot_load_active_plugins();
  boot_start_plugins(true);
  LOG_DEBUG_CORE("Starting: DONE");
}
void NSClientT::handle_shutdown(std::string service_name) { stop_nsclient(); }

NSClientT::service_controller NSClientT::get_service_control() { return service_controller(service_name_); }

void NSClientT::service_controller::stop() {
#ifdef WIN32
  win_service_control::StopNoWait(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
void NSClientT::service_controller::start() {
#ifdef WIN32
  win_service_control::Start(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
bool NSClientT::service_controller::is_started() {
#ifdef WIN32
  try {
    if (win_service_control::isStarted(utf8::cvt<std::wstring>(get_service_name()))) {
      return true;
    }
  } catch (...) {
    return false;
  }
#endif
  return false;
}

PB::Metrics::MetricsBundle NSClientT::ownMetricsFetcher() {
  PB::Metrics::MetricsBundle bundle;
  bundle.set_key("workers");
  if (scheduler_.get_scheduler().has_metrics()) {
    boost::uint64_t taskes_ = scheduler_.get_scheduler().get_metric_executed();
    boost::uint64_t submitted_ = scheduler_.get_scheduler().get_metric_compleated();
    boost::uint64_t errors_ = scheduler_.get_scheduler().get_metric_errors();
    boost::uint64_t threads = scheduler_.get_scheduler().get_metric_threads();

    PB::Metrics::Metric *m = bundle.add_value();
    m->set_key("jobs");
    m->mutable_gauge_value()->set_value(static_cast<double>(taskes_));
    m = bundle.add_value();
    m->set_key("submitted");
    m->mutable_gauge_value()->set_value(static_cast<double>(submitted_));
    m = bundle.add_value();
    m->set_key("errors");
    m->mutable_gauge_value()->set_value(static_cast<double>(errors_));
    m = bundle.add_value();
    m->set_key("threads");
    m->mutable_gauge_value()->set_value(static_cast<double>(threads));
  } else {
    PB::Metrics::Metric *m = bundle.add_value();
    m->set_key("metrics.available");
    m->mutable_gauge_value()->set_value(0);
  }
  return bundle;
}
void NSClientT::process_metrics() { plugins_->process_metrics(ownMetricsFetcher()); }

#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {}
#endif
