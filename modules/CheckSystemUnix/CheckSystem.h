// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "filter_config_object.hpp"
#include "realtime_thread.hpp"

class CheckSystem : public nscapi::impl::simple_plugin {
  std::shared_ptr<pdh_thread> collector_;

  // Configured timezone for `check_uptime`, cached in loadModuleEx (issue #365).
  // See `include/nscp_time.hpp` for the supported value syntax.
  std::string timezone_;

 public:
  CheckSystem() : simple_plugin() {}
  virtual ~CheckSystem() = default;

  virtual bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  virtual bool unloadModule();

  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

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
  void check_cpu_frequency(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_temperature(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_battery(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_process_history_new(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // Accessor for the collector thread (used by check_cpu)
  std::shared_ptr<pdh_thread> get_collector() { return collector_; }
};
