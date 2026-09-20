// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

// Hyper-V host and virtual machine checks. The data comes from two places
// that every Hyper-V host has: the hypervisor performance counters (host
// health summary, logical processors) and the root\virtualization\v2 WMI
// namespace (one Msvm_ComputerSystem per virtual machine plus the classes
// hanging off it). Each area keeps its own check_hyperv_<area>.cpp/.hpp pair;
// this class is only the dispatch surface the generated module glue binds to,
// and it holds no state, so a settings reload re-entering loadModuleEx is a
// no-op.
class CheckHyperV : public nscapi::impl::simple_plugin {
 public:
  CheckHyperV() {}

  bool loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  static void check_hyperv_host(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_hyperv_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);
};
