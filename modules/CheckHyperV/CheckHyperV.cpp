// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckHyperV.h"

#include <map>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_host.hpp"
#include "check_hyperv_internal.hpp"
#include "check_hyperv_vms.hpp"

bool CheckHyperV::loadModuleEx(const std::string &, NSCAPI::moduleLoadMode) { return true; }
bool CheckHyperV::unloadModule() { return true; }

void CheckHyperV::check_hyperv_host(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_hyperv::check_hyperv_host(request, response);
}
void CheckHyperV::check_hyperv_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_hyperv::check_hyperv_cpu(request, response);
}
void CheckHyperV::check_hyperv_vms(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_hyperv::check_hyperv_vms(request, response);
}

// One snapshot per metrics interval: the host counters (a single PDH sample)
// and one record per virtual machine (the same WMI rows check_hyperv_vms
// reads). On a host without the role both sources fail fast and nothing is
// published, so an enabled module on the wrong host costs one failed lookup
// per interval and no log noise.
void CheckHyperV::fetchMetrics(PB::Metrics::MetricsMessage::Response *response) {
  using check_hyperv::check_hyperv_internal::build_records;
  using check_hyperv::check_hyperv_internal::vm_record;
  using nscapi::metrics::describe;
  using nscapi::metrics::for_instance;
  using nscapi::metrics::metric;
  using PDH::value_of;

  std::map<std::string, double> host;
  bool have_host = false;
  try {
    host = check_hyperv::fetch_host_counters();
    have_host = true;
  } catch (const PDH::pdh_exception &) {
    // Not a Hyper-V host (or the hypervisor is not running): nothing to publish.
  }

  std::vector<vm_record> vms;
  bool have_vms = false;
  {
    const com_helper::mta_scope com;
    try {
      vms = build_records(check_hyperv::fetch_vm_rows());
      have_vms = true;
    } catch (const wmi_impl::wmi_exception &) {
      // The namespace is missing without the role; any other failure is
      // reported by the check itself, where it is visible.
    }
  }
  if (!have_host && !have_vms) return;

  PB::Metrics::MetricsBundle *bundle = response->add_bundles();
  bundle->set_key("hyperv");
  describe(bundle, "Hyper-V host and virtual machines");

  if (have_host) {
    metric(bundle, "vms.health_ok").help("Virtual machines whose health is ok").gauge(static_cast<long long>(value_of(host, "Health Ok")));
    metric(bundle, "vms.health_critical").help("Virtual machines whose health is critical").gauge(static_cast<long long>(value_of(host, "Health Critical")));
    metric(bundle, "logical_processors").help("Logical processors the hypervisor manages").gauge(static_cast<long long>(value_of(host, "Logical Processors")));
    metric(bundle, "virtual_processors")
        .help("Virtual processors allocated to running partitions")
        .gauge(static_cast<long long>(value_of(host, "Virtual Processors")));
    metric(bundle, "partitions").help("Running partitions including the root partition").gauge(static_cast<long long>(value_of(host, "Partitions")));
  }

  if (have_vms) {
    long long running = 0;
    for (const vm_record &vm : vms) {
      if (vm.is_running()) ++running;
      const auto scope = for_instance(bundle, vm.name, "vm");
      scope.metric("state").help("Power state of the virtual machine").info(vm.state());
      scope.metric("running").help("1 when the virtual machine is running, else 0").gauge(vm.is_running() ? 1 : 0);
      scope.metric("heartbeat").help("Guest heartbeat status").info(vm.heartbeat);
      scope.metric("uptime").help("Seconds since the virtual machine was started").unit("seconds").gauge(vm.uptime_seconds);
      scope.metric("cpu_load").help("Average load of the virtual processors").unit("percent").gauge(vm.cpu_load);
      scope.metric("memory_assigned").help("Memory assigned to the virtual machine").unit("bytes").gauge(vm.memory_assigned);
      scope.metric("vcpus").help("Configured virtual processors").gauge(vm.vcpus);
      scope.metric("snapshots").help("Checkpoints the virtual machine has").gauge(vm.snapshots);
    }
    metric(bundle, "vms.total").help("Virtual machines on the host").gauge(static_cast<long long>(vms.size()));
    metric(bundle, "vms.running").help("Virtual machines that are running").gauge(running);
  }
}
