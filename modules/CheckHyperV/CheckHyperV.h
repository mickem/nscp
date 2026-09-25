// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <vector>

#include "check_hyperv_internal.hpp"

// Hyper-V host and virtual machine checks. The data comes from two places
// that every Hyper-V host has: the hypervisor performance counters (host
// health summary, logical processors) and the root\virtualization\v2 WMI
// namespace (one Msvm_ComputerSystem per virtual machine plus the classes
// hanging off it). Each area keeps its own check_hyperv_<area>.cpp/.hpp pair;
// this class is the dispatch surface the generated module glue binds to and
// holds the one piece of configuration the module has: which fact sets it
// produces. Nothing here starts a thread, so a settings reload re-entering
// loadModuleEx only re-reads that switch.
class CheckHyperV : public nscapi::impl::simple_plugin {
  // Whether fetchFacts builds the `hyperv.vms` set ([/settings/hyperv/facts]).
  // Read by fetchFacts on the core's scheduler thread, written by
  // loadModuleEx on the loading thread; the atomic is that hand-over.
  std::atomic<bool> facts_vms_{false};

  // The virtual machines fetchMetrics publishes, re-read at most once per
  // kVmRefresh by refresh_vms. Only fetchMetrics touches them, and the core
  // calls it from its one metrics thread.
  static const std::chrono::seconds kVmRefresh;
  std::vector<check_hyperv::check_hyperv_internal::vm_record> vms_;
  bool have_vms_ = false;
  bool vms_read_ = false;
  std::chrono::steady_clock::time_point vms_read_at_;

  // Re-read vms_ once it is older than kVmRefresh. `host` is this interval's
  // counters, or null when they could not be read; it is what tells "no VMs"
  // apart from "VMs this account cannot see".
  void refresh_vms(const std::map<std::string, double> *host);

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
