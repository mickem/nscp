// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_nodes.hpp"

#include <boost/json.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

#include "kube_object.hpp"
#include "kube_quantity.hpp"

namespace json = boost::json;
namespace po = boost::program_options;

namespace kube_checks {

namespace {

struct node_obj {
  std::string name, ready, node_status, kubelet_version, os, arch, roles, taints, internal_ip;
  bool schedulable = true;
  bool memory_pressure = false, disk_pressure = false, pid_pressure = false, network_unavailable = false;
  long long cpu_capacity = -1, cpu_allocatable = -1, memory_capacity = -1, memory_allocatable = -1, pods_capacity = -1;
  long long created = 0;
  long long age = -1;

  std::string show() const { return name + ": " + node_status; }

  std::string get_name() const { return name; }
  std::string get_ready() const { return ready; }
  std::string get_node_status() const { return node_status; }
  std::string get_kubelet_version() const { return kubelet_version; }
  std::string get_os() const { return os; }
  std::string get_arch() const { return arch; }
  std::string get_roles() const { return roles; }
  std::string get_taints() const { return taints; }
  std::string get_internal_ip() const { return internal_ip; }
  long long get_schedulable() const { return schedulable ? 1 : 0; }
  long long get_memory_pressure() const { return memory_pressure ? 1 : 0; }
  long long get_disk_pressure() const { return disk_pressure ? 1 : 0; }
  long long get_pid_pressure() const { return pid_pressure ? 1 : 0; }
  long long get_network_unavailable() const { return network_unavailable ? 1 : 0; }
  long long get_cpu_capacity() const { return cpu_capacity; }
  long long get_cpu_allocatable() const { return cpu_allocatable; }
  long long get_memory_capacity() const { return memory_capacity; }
  long long get_memory_allocatable() const { return memory_allocatable; }
  long long get_pods_capacity() const { return pods_capacity; }
  long long get_created() const { return created; }
  long long get_age() const { return age; }
};

// The kubectl STATUS column for a node: Ready / NotReady / Unknown, with
// ",SchedulingDisabled" appended for a cordoned node.
std::string node_status_of(const std::string &ready, const bool schedulable) {
  std::string status = ready == "True" ? "Ready" : ready == "False" ? "NotReady" : "Unknown";
  if (!schedulable) status += ",SchedulingDisabled";
  return status;
}

std::shared_ptr<node_obj> parse_node(const json::object &o) {
  auto record = std::make_shared<node_obj>();
  const api_object node(o);
  const json::object *metadata = &node.metadata;
  const json::object *spec = &node.spec;
  const json::object *status = &node.status;

  record->name = node.name();
  const object_age age = age_of(*metadata);
  record->age = age.age;
  record->created = age.created;
  // Roles are labels: node-role.kubernetes.io/<role>="" (kubectl reads the same).
  if (const json::object *labels = get_obj(*metadata, "labels")) {
    static const std::string prefix = "node-role.kubernetes.io/";
    for (const auto &kv : *labels) {
      const std::string key(kv.key());
      if (key.compare(0, prefix.size(), prefix) == 0 && key.size() > prefix.size()) str::format::append_list(record->roles, key.substr(prefix.size()), ",");
    }
  }

  record->schedulable = !get_bool(*spec, "unschedulable");
  if (const json::array *taints = get_arr(*spec, "taints")) {
    for (const auto &v : *taints) {
      if (!v.is_object()) continue;
      const json::object &t = v.as_object();
      const std::string value = get_str(t, "value");
      str::format::append_list(record->taints, get_str(t, "key") + (value.empty() ? "" : "=" + value) + ":" + get_str(t, "effect"), ",");
    }
  }

  record->ready = condition_status(*status, "Ready");
  record->memory_pressure = condition_is_true(*status, "MemoryPressure");
  record->disk_pressure = condition_is_true(*status, "DiskPressure");
  record->pid_pressure = condition_is_true(*status, "PIDPressure");
  record->network_unavailable = condition_is_true(*status, "NetworkUnavailable");
  record->node_status = node_status_of(record->ready, record->schedulable);

  if (const json::object *info = get_obj(*status, "nodeInfo")) {
    record->kubelet_version = get_str(*info, "kubeletVersion");
    record->os = get_str(*info, "osImage");
    record->arch = get_str(*info, "architecture");
  }
  if (const json::object *capacity = get_obj(*status, "capacity")) {
    record->cpu_capacity = quantity_to_millicores(get_str(*capacity, "cpu"));
    record->memory_capacity = quantity_to_bytes(get_str(*capacity, "memory"));
    record->pods_capacity = quantity_to_count(get_str(*capacity, "pods"));
  }
  if (const json::object *allocatable = get_obj(*status, "allocatable")) {
    record->cpu_allocatable = quantity_to_millicores(get_str(*allocatable, "cpu"));
    record->memory_allocatable = quantity_to_bytes(get_str(*allocatable, "memory"));
  }
  if (const json::array *addresses = get_arr(*status, "addresses")) {
    for (const auto &v : *addresses) {
      if (!v.is_object()) continue;
      if (get_str(v.as_object(), "type") == "InternalIP") {
        record->internal_ip = get_str(v.as_object(), "address");
        break;
      }
    }
  }
  return record;
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<node_obj>> node_context;
struct node_obj_handler : public node_context {
  node_obj_handler() {
    registry_.add_string_var("name", &node_obj::get_name, "Node name")
        .add_string_var("ready", &node_obj::get_ready,
                        "The Ready condition: True, False or Unknown (empty for a requested node the API server does not know about)")
        .add_string_var("node_status", &node_obj::get_node_status,
                        "The kubectl STATUS column: Ready, NotReady or Unknown, with ,SchedulingDisabled appended for a cordoned node; missing for a requested "
                        "node the API server does not know about")
        .add_string_var("kubelet_version", &node_obj::get_kubelet_version, "Kubelet version (e.g. v1.30.2)")
        .add_string_var("os", &node_obj::get_os, "OS image the node runs (e.g. Ubuntu 22.04.4 LTS)")
        .add_string_var("arch", &node_obj::get_arch, "CPU architecture (amd64, arm64, ...)")
        .add_string_var("roles", &node_obj::get_roles, "Node roles from the node-role.kubernetes.io/<role> labels, comma separated")
        .add_string_var("taints", &node_obj::get_taints, "Taints as key=value:effect, comma separated")
        .add_string_var("internal_ip", &node_obj::get_internal_ip, "The node's InternalIP address");
    registry_.add_int_var("schedulable", &node_obj::get_schedulable, "1 unless the node is cordoned (spec.unschedulable), then 0").no_perf();
    registry_.add_int_var("memory_pressure", &node_obj::get_memory_pressure, "1 when the MemoryPressure condition is True, else 0").no_perf();
    registry_.add_int_var("disk_pressure", &node_obj::get_disk_pressure, "1 when the DiskPressure condition is True, else 0").no_perf();
    registry_.add_int_var("pid_pressure", &node_obj::get_pid_pressure, "1 when the PIDPressure condition is True, else 0").no_perf();
    registry_.add_int_var("network_unavailable", &node_obj::get_network_unavailable, "1 when the NetworkUnavailable condition is True, else 0").no_perf();
    registry_.add_int_var("cpu_capacity", &node_obj::get_cpu_capacity, "CPU capacity in millicores (-1 when not reported)")
        .add_int_perf("", "", " cpu capacity")
        .add_int_var("cpu_allocatable", &node_obj::get_cpu_allocatable, "CPU available to pods in millicores (-1 when not reported)")
        .add_int_perf("", "", " cpu allocatable")
        .add_int_var("pods_capacity", &node_obj::get_pods_capacity, "Maximum number of pods the node accepts (-1 when not reported)")
        .add_int_perf("", "", " pods capacity");
    registry_
        .add_int_var("memory_capacity", parsers::where::type_size, &node_obj::get_memory_capacity,
                     "Memory capacity in bytes, -1 when not reported (thresholds take units, e.g. memory_capacity < 8G)")
        .add_int_perf("B", "", " memory capacity")
        .add_int_var("memory_allocatable", parsers::where::type_size, &node_obj::get_memory_allocatable,
                     "Memory available to pods in bytes, -1 when not reported (thresholds take units, e.g. memory_allocatable < 4G)")
        .add_int_perf("B", "", " memory allocatable");
    register_age_keywords<node_obj>(registry_, &node_obj::get_created, &node_obj::get_age, "the node joined the cluster");
  }
};
typedef modern_filter::modern_filters<node_obj, node_obj_handler> node_filter;

}  // namespace

void check_nodes(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                 const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<node_filter> filter_helper(request, response, data);
  int timeout = defaults.timeout;
  std::vector<std::string> required;
  list_options opt;

  node_filter filter;
  // A node that is not Ready is down for scheduling purposes; pressure or a
  // cordon is something to look at before it gets there.
  filter_helper.add_options("memory_pressure = 1 or disk_pressure = 1 or pid_pressure = 1 or network_unavailable = 1 or schedulable = 0", "ready != 'True'", "",
                            filter.get_filter_syntax(), "critical");
  filter_helper.add_syntax("${status}: ${problem_list}", "${name}=${node_status}", "${name}", "%(status): No nodes found",
                           "%(status): All %(count) nodes are ready");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("label-selector", po::value<std::string>(&opt.label_selector), "Label selector passed to the API server, e.g. node-role.kubernetes.io/worker= or topology.kubernetes.io/zone=eu-1a.")
    ("field-selector", po::value<std::string>(&opt.field_selector), "Field selector passed to the API server, e.g. metadata.name=worker-1.")
    ("node", po::value<std::vector<std::string>>(&required), "Name of a node that must exist (repeatable). Only the named nodes take part in the check; a name the API server does not return gets node_status 'missing'.")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for each API server request, in seconds.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  cluster target;
  std::string error;
  if (!resolve_cluster(defaults, target, error)) return fail(response, error);
  target.timeout = timeout;
  fetcher fetch;
  if (!open_fetcher(make_fetcher, target, fetch, response)) return;

  std::vector<json::value> items;
  if (!list_all(fetch, target, "/api/v1/nodes", opt, items, response)) return;

  required_names wanted(required);
  for (const auto &v : items) {
    if (!v.is_object()) continue;
    auto record = parse_node(v.as_object());
    if (!wanted.empty() && !wanted.claim(record->name)) continue;
    filter.match(record);
  }

  // A required node the API server does not know about left the cluster (or
  // never joined), which is exactly what the operator asked to be told about.
  wanted.for_each_missing("", [&filter](const std::string &, const std::string &name) {
    auto record = std::make_shared<node_obj>();
    record->name = name;
    record->node_status = "missing";
    record->schedulable = false;
    filter.match(record);
  });

  filter_helper.post_process(filter);
}

}  // namespace kube_checks
