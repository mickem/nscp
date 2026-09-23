// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <win/CheckMemory.h>

#include <atomic>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "check_pdh.hpp"
#include "check_registry.hpp"
#include "filter_config_object.hpp"
#include "pdh_thread.hpp"

class CheckSystem : public nscapi::impl::simple_plugin {
 private:
  CheckMemory memoryChecker;
  // A reload replaces the collector while checks are running: the core calls
  // loadModuleEx(reloadStart) on the live module without waiting for the
  // threads that are inside handleCommand. Publishing the pointer atomically
  // and taking a copy in every check keeps the old instance alive until the
  // last check that observed it returns, instead of freeing its mutexes and
  // buffers under a check that is reading them.
  std::shared_ptr<pdh_thread> collector;

  typedef std::map<std::string, std::string> counter_map_type;
  counter_map_type counters;

  // Which fact sets this module is configured to produce
  // ([/settings/system/windows/facts]). Read by fetchFacts on the core's
  // schedule and written by loadModuleEx, which a reload runs on the live
  // module while that schedule is ticking - hence atomic rather than plain
  // bools.
  std::atomic<bool> facts_os_{false};
  std::atomic<bool> facts_hardware_{false};

  //	std::map<DWORD,std::string> lookups_;

  check_pdh::check pdh_checker;

  // Which registry keys a caller may name in check_registry_key /
  // check_registry_value. Open by default; see
  // docs/docs/concepts/check-access.md.
  check::access::prefix_policy registry_access_{"registry key", "registry keys", "/settings/system/windows", '\\', &normalize_registry_hive};

  // Configured timezone for `check_uptime`, cached in loadModuleEx (issue #365).
  // See `include/nscp_time.hpp` for the supported value syntax.
  std::string timezone_;

  // The collector as the check threads must read it. Never dereference the
  // member directly from a check: a reload can replace it between the test and
  // the call.
  std::shared_ptr<pdh_thread> get_collector() const { return std::atomic_load(&collector); }

 public:
  CheckSystem() {}
  virtual ~CheckSystem() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  NSCAPI::nagiosReturn commandLineExec(const int target_mode, const std::string &command, const std::list<std::string> &arguments, std::string &result);

  // Checks
  void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pdh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pagefile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void add_counter(std::string key, std::string query);
  void add_rrd_counter(std::string key, std::string query);
  void check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history_new(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_registry_key(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_registry_value(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pending_reboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_patch_age(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_installed_software(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_hardware(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_hostname(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_printqueue(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_printjobs(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_w32time(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // Metrics
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);

  // Legacy checks
  void checkCpu(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkMem(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkUptime(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkServiceState(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkProcState(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkCounter(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
