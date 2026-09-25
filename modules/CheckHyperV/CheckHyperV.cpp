// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckHyperV.h"

#include <ctime>
#include <map>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_hyperv_host.hpp"
#include "check_hyperv_internal.hpp"
#include "check_hyperv_vms.hpp"
#include "hyperv_facts.hpp"

namespace sh = nscapi::settings_helper;

bool CheckHyperV::loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("hyperv", alias);

  bool facts_vms = false;
  // clang-format off
  settings.alias().add_key_to_settings("facts")
    .add_bool(hyperv_facts::id_vms, sh::bool_key(&facts_vms, false),
        "HYPER-V VMS FACTS",
        "Collect the `hyperv.vms` fact set: one record per virtual machine on this host - its name (the record id, the same value "
        "check_hyperv_vms calls `vm`), its GUID, generation and configuration version, the configured processors and memory, whether dynamic "
        "memory is on, how many checkpoints it has and its Hyper-V Replica role. Not its state, heartbeat, load or assigned memory: that is "
        "monitoring, and it lives in check_hyperv_vms. Re-read every facts round with the same WMI queries the check runs, because VMs are "
        "created, reconfigured and removed while the agent runs. Nothing is collected while this is off.")
    ;
  // clang-format on
  settings.register_all();
  settings.notify();

  // Which fact sets fetchFacts builds is configuration, so it is re-read on
  // every load, a reload included: the core drops a set a producer stops
  // returning, and that is what turning it off means.
  facts_vms_.store(facts_vms);
  return true;
}
bool CheckHyperV::unloadModule() { return true; }

void CheckHyperV::fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response) {
  if (!facts_vms_.load()) return;
  // The startup round runs on the boot thread, and every producer after this
  // one in the round waits for it. The first query into the virtualization
  // namespace starts the Hyper-V management provider when it is not already
  // up, which on a host whose VMMS is stopped takes long enough to hold the
  // rest of the inventory (and the service start) hostage - the same reason
  // CheckSystem reads nothing over WMI in a round. So the set is claimed at
  // startup and collected on the first scheduled, reload or manual round,
  // which run on their own threads.
  if (request.reason() == "startup") {
    response.error(hyperv_facts::set_hyperv,
                   "Not collected during startup: the virtual machines are read on the first scheduled round, or now with a manual refresh");
    return;
  }
  // Every other round, whatever its reason: VMs come and go while the
  // process runs, and reading them costs what one check_hyperv_vms costs.
  const com_helper::mta_scope com;
  try {
    const std::vector<check_hyperv::check_hyperv_internal::vm_record> vms = check_hyperv::check_hyperv_internal::build_records(check_hyperv::fetch_vm_rows());
    // An empty list here would tell the server the host has no VMs; when it
    // has some this account cannot see, say that instead and keep the last
    // list the core holds.
    const std::string hidden = check_hyperv::vms_hidden_from_caller(vms.size());
    if (!hidden.empty()) return response.error(hyperv_facts::set_hyperv, hidden);
    hyperv_facts::publish(vms, std::time(nullptr), response);
  } catch (const wmi_impl::wmi_exception &e) {
    // Named against the set rather than failing the round: the core keeps
    // the VMs it already holds and reports why they are stale. On a host
    // without the role that is the standing answer, every round, which is
    // what an enabled set that cannot be collected is supposed to say.
    if (check_hyperv::is_hyperv_missing(e)) {
      response.error(hyperv_facts::set_hyperv, "The Hyper-V role is not installed on this host (root\\virtualization\\v2 missing)");
    } else {
      response.error(hyperv_facts::set_hyperv, "Failed to query Hyper-V virtual machines: " + e.reason());
    }
  } catch (const std::exception &e) {
    response.error(hyperv_facts::set_hyperv, "Failed to query Hyper-V virtual machines: " + utf8::utf8_from_native(e.what()));
  }
}

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
      // VMs this account is not allowed to see would publish as vms.total 0.
      have_vms = check_hyperv::vms_hidden_from_caller(vms.size()).empty();
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
