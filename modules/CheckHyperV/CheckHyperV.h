// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <chrono>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "vm_refresher.hpp"

// Hyper-V host and virtual machine checks. The data comes from two places
// that every Hyper-V host has: the hypervisor performance counters (host
// health summary, logical processors) and the root\virtualization\v2 WMI
// namespace (one Msvm_ComputerSystem per virtual machine plus the classes
// hanging off it). Each area keeps its own check_hyperv_<area>.cpp/.hpp pair;
// this class is the dispatch surface the generated module glue binds to and
// holds the one piece of configuration the module has: which fact sets it
// produces. The one thread, vm_refresher's, is started by fetchMetrics and
// stopped by unloadModule, so a settings reload re-entering loadModuleEx only
// re-reads that switch.
class CheckHyperV : public nscapi::impl::simple_plugin {
  // Whether fetchFacts builds the `hyperv.vms` set ([/settings/hyperv/facts]).
  // Read by fetchFacts on the core's scheduler thread, written by
  // loadModuleEx on the loading thread; the atomic is that hand-over.
  std::atomic<bool> facts_vms_{false};

  // The virtual machines fetchMetrics publishes, walked once a minute on a
  // thread of its own (started by the first fetchMetrics that read the host
  // counters, stopped on unload).
  vm_refresher vms_{std::chrono::seconds(60)};

  // When fetchMetrics next tries the host counters after they failed. Only
  // fetchMetrics touches it, and it runs on the core's one metrics thread.
  std::chrono::steady_clock::time_point host_retry_at_{};

 public:
  CheckHyperV() {}

  bool loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  static void check_hyperv_host(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_hyperv_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);
};
