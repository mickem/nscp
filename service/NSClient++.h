// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nsclient/logger/logger.hpp>
#include <service/system_service.hpp>

#include "nsclient_core_interface.hpp"
#include "plugins/plugin_cache.hpp"
#include "plugins/plugin_manager.hpp"
#include "scheduler_handler.hpp"
#include "storage_manager.hpp"

class NSClientT;
typedef service_helper::impl<NSClientT>::system_service NSClient;

/**
 * Main NSClient++ core class. This is the service core and as such is responsible for pretty much everything.
 * It also acts as a broker for all plugins and other sub threads and such.
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @todo Plugininfy the socket somehow ?
 * It is technically possible to make the socket a plug-in but would it be a good idea ?
 */

class NSClientT : public nsclient::core::core_interface {
 private:
  boost::timed_mutex internalVariables;

  std::string context_;

  std::string service_name_;
  nsclient::logging::logger_instance log_instance_;
  nsclient::core::path_instance path_;
  nsclient::core::plugin_mgr_instance plugins_;
  nsclient::core::storage_manager_instance storage_manager_;
  // Path overrides supplied via --path-override on the command line. Applied to
  // path_ inside load_configuration_1, after init_settings has loaded
  // boot.ini's [paths] section, so CLI wins over boot.ini.
  std::map<std::string, std::string> cli_path_overrides_;

  task_scheduler::scheduler scheduler_;

 public:
  typedef std::multimap<std::string, std::string> plugin_alias_list_type;
  // c-tor, d-tor
  NSClientT();
  virtual ~NSClientT();

  // Startup/Shutdown
  bool load_configuration_1();
  bool load_configuration_2(const bool override_log = false);
  bool boot_load_active_plugins();
  void boot_load_all_plugin_files();
  bool boot_load_single_plugin(const std::string& plugin);
  bool boot_start_plugins(bool boot);

  bool stop_nsclient();
  void set_settings_context(std::string context) { context_ = context; }
  void set_cli_path_overrides(std::map<std::string, std::string> overrides) { cli_path_overrides_ = std::move(overrides); }

  NSCAPI::errorReturn reload(const std::string module);
  bool do_reload(const std::string module);

  // Service API
  static NSClient* get_global_instance();
  void handle_startup(std::string service_name);
  void handle_shutdown(std::string service_name);
#ifdef _WIN32
  void handle_session_change(unsigned long dwSessionId, bool logon);
#endif

  // Core API interface (get modules)
  nsclient::logging::logger_instance get_logger() { return log_instance_; }
  nsclient::core::plugin_mgr_instance get_plugin_manager() { return plugins_; }
  nsclient::core::path_instance get_path() { return path_; }
  nsclient::core::plugin_cache* get_plugin_cache() { return plugins_->get_plugin_cache(); }
  nsclient::core::storage_manager_instance get_storage_manager() override { return storage_manager_; }

  struct service_controller {
    std::string service;
    service_controller(std::string service) : service(service) {}
    service_controller(const service_controller& other) : service(other.service) {}
    service_controller& operator=(const service_controller& other) {
      service = other.service;
      return *this;
    }
    void stop();
    void start();
    std::string get_service_name() { return service; }
    bool is_started();
  };

  service_controller get_service_control();

  void process_metrics();

 private:
  void reloadPlugins();
  void unloadPlugins();

  PB::Metrics::MetricsBundle ownMetricsFetcher();
};
