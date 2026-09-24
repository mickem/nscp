// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <facts/host_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "filter_config_object.hpp"
#include "realtime_thread.hpp"

class CheckSystem : public nscapi::impl::simple_plugin {
  // A reload replaces the collector while checks are running: the core calls
  // loadModuleEx(reloadStart) on the live module without waiting for the
  // threads that are inside handleCommand. Publishing the pointer atomically
  // and taking a copy in every check keeps the old instance alive until the
  // last check that observed it returns, instead of freeing its buffers under
  // a check that is reading them.
  std::shared_ptr<pdh_thread> collector_;

  // Configured timezone for `check_uptime`, cached in loadModuleEx (issue #365).
  // See `include/nscp_time.hpp` for the supported value syntax.
  std::string timezone_;

  // Which fact sets this module is configured to produce
  // ([/settings/system/unix/facts]). Read by fetchFacts on the core's
  // schedule and written by loadModuleEx, which a reload runs on the live
  // module while that schedule is ticking - hence atomic rather than plain
  // bools.
  std::atomic<bool> facts_os_{false};
  std::atomic<bool> facts_hardware_{false};
  std::atomic<bool> facts_network_interfaces_{false};
  // The last gathered snapshot. These facts cannot change without the host
  // rebooting, so a scheduled round reports what this holds and says how old
  // it is; see host_facts::should_regather.
  host_facts::snapshot_cache facts_cache_;

 public:
  CheckSystem() : simple_plugin() {}
  virtual ~CheckSystem() = default;

  virtual bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  virtual bool unloadModule();

  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);

  void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_cpu_utilization(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_kernel_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_swap_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pagefile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_installed_software(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_hostname(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_kernel_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history_new(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // The collector as the check threads must read it. Never copy the member
  // directly from a check: a reload can replace it while the copy is taken.
  std::shared_ptr<pdh_thread> get_collector() const { return std::atomic_load(&collector_); }
};
