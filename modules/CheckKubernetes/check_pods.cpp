// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_pods.hpp"

#include <boost/json.hpp>
#include <check/duration_keyword.hpp>
#include <ctime>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/rfc3339.hpp>
#include <string>
#include <vector>

#include "kube_pod_status.hpp"

namespace json = boost::json;
namespace po = boost::program_options;

namespace kube_checks {

namespace {

struct pod_obj {
  std::string name, ns, node, phase, pod_status, owner_kind, owner, qos, ip, labels;
  long long ready_containers = 0, containers = 0, restarts = 0;
  long long created = 0;  // unix time of creationTimestamp, 0 when unknown
  long long age = -1;     // seconds since creation, -1 when unknown
  bool terminating = false, oom_killed = false, pod_ready = false;

  std::string show() const { return ns + "/" + name + ": " + pod_status; }

  std::string get_name() const { return name; }
  std::string get_namespace() const { return ns; }
  std::string get_node() const { return node; }
  std::string get_phase() const { return phase; }
  std::string get_pod_status() const { return pod_status; }
  std::string get_owner_kind() const { return owner_kind; }
  std::string get_owner() const { return owner; }
  std::string get_qos() const { return qos; }
  std::string get_ip() const { return ip; }
  std::string get_labels() const { return labels; }
  long long get_ready_containers() const { return ready_containers; }
  long long get_containers() const { return containers; }
  long long get_restarts() const { return restarts; }
  long long get_created() const { return created; }
  long long get_age() const { return age; }
  long long get_terminating() const { return terminating ? 1 : 0; }
  long long get_oom_killed() const { return oom_killed ? 1 : 0; }
  long long get_ready() const { return pod_ready ? 1 : 0; }
};

std::shared_ptr<pod_obj> parse_pod(const json::object &o) {
  auto record = std::make_shared<pod_obj>();
  static const json::object empty;
  const json::object *metadata = get_obj(o, "metadata");
  const json::object *spec = get_obj(o, "spec");
  const json::object *status = get_obj(o, "status");
  if (!metadata) metadata = &empty;
  if (!spec) spec = &empty;
  if (!status) status = &empty;

  record->name = get_str(*metadata, "name");
  record->ns = get_str(*metadata, "namespace");
  record->labels = join_map(*metadata, "labels");
  const std::string created = get_str(*metadata, "creationTimestamp");
  record->age = str::seconds_since_rfc3339(created);
  if (record->age >= 0) record->created = static_cast<long long>(std::time(nullptr)) - record->age;
  if (const json::array *owners = get_arr(*metadata, "ownerReferences")) {
    // The controller reference is the one that matters; fall back to the first.
    for (const auto &v : *owners) {
      if (!v.is_object()) continue;
      const json::object &ref = v.as_object();
      if (record->owner.empty() || get_bool(ref, "controller")) {
        record->owner_kind = get_str(ref, "kind");
        record->owner = get_str(ref, "name");
      }
      if (get_bool(ref, "controller")) break;
    }
  }

  record->node = get_str(*spec, "nodeName");
  record->phase = get_str(*status, "phase");
  record->ip = get_str(*status, "podIP");
  record->qos = get_str(*status, "qosClass");

  const pod_state state = derive_pod_state(o);
  record->pod_status = state.status;
  record->restarts = state.restarts;
  record->ready_containers = state.ready;
  record->containers = state.containers;
  record->terminating = state.terminating;
  record->oom_killed = state.oom_killed;
  record->pod_ready = state.pod_ready;
  return record;
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<pod_obj>> pod_context;
struct pod_obj_handler : public pod_context {
  pod_obj_handler() {
    registry_.add_string_var("name", &pod_obj::get_name, "Pod name")
        .add_string_var("namespace", &pod_obj::get_namespace, "Namespace the pod lives in")
        .add_string_var("node", &pod_obj::get_node, "Node the pod is scheduled on (empty while Pending)")
        .add_string_var("phase", &pod_obj::get_phase, "Pod phase: Pending, Running, Succeeded, Failed or Unknown")
        .add_string_var("pod_status", &pod_obj::get_pod_status,
                        "The kubectl STATUS column: Running, Completed, CrashLoopBackOff, ImagePullBackOff, OOMKilled, Terminating, Init:1/2, ... or "
                        "missing for a requested pod the API server does not know about")
        .add_string_var("owner_kind", &pod_obj::get_owner_kind, "Kind of the controller owning the pod: ReplicaSet, StatefulSet, DaemonSet, Job, Node, ...")
        .add_string_var("owner", &pod_obj::get_owner, "Name of the controller owning the pod")
        .add_string_var("qos", &pod_obj::get_qos, "QoS class: Guaranteed, Burstable or BestEffort")
        .add_string_var("ip", &pod_obj::get_ip, "Pod IP address")
        .add_string_var("labels", &pod_obj::get_labels, "Pod labels as key=value, comma separated");
    registry_.add_int_var("ready_containers", &pod_obj::get_ready_containers, "Number of containers reporting ready")
        .add_int_perf("", "", " ready")
        .add_int_var("containers", &pod_obj::get_containers, "Number of containers in the pod spec")
        .add_int_perf("", "", " containers")
        .add_int_var("restarts", &pod_obj::get_restarts, "Total container restarts (the kubectl RESTARTS column)")
        .add_int_perf("", "", " restarts");
    registry_.add_int_var("ready", &pod_obj::get_ready, "1 when the pod's Ready condition is True, else 0").no_perf();
    registry_.add_int_var("terminating", &pod_obj::get_terminating, "1 when the pod is being deleted (deletionTimestamp is set), else 0").no_perf();
    registry_.add_int_var("oom_killed", &pod_obj::get_oom_killed, "1 when a container's current or last termination was an out-of-memory kill, else 0")
        .no_perf();
    registry_.add_int_var("created", parsers::where::type_date, &pod_obj::get_created, "When the pod was created (date)").no_perf();

    static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;
    registry_.add_int_var("age", type_custom_age, &pod_obj::get_age, "Seconds since the pod was created, -1 when unknown (supports units, e.g. age < 10m)")
        .no_perf();
    registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<pod_obj>>);
  }
};
typedef modern_filter::modern_filters<pod_obj, pod_obj_handler> pod_filter;

}  // namespace

void check_pods(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<pod_filter> filter_helper(request, response, data);
  int timeout = defaults.timeout;
  std::vector<std::string> namespaces;
  std::vector<std::string> required;
  list_options opt;

  pod_filter filter;
  // Finished job pods are noise; a pending pod or a bumpy one is worth a look;
  // a failed, crash-looping, OOM-killed or missing pod is the alert.
  filter_helper.add_options("phase = 'Pending' or restarts > 5",
                            "phase = 'Failed' or phase = 'Unknown' or pod_status like 'BackOff' or pod_status = 'OOMKilled' or pod_status = 'missing'",
                            "phase != 'Succeeded'", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${namespace}/${name}=${pod_status}", "${namespace}/${name}", "%(status): No pods found",
                           "%(status): All %(count) pods are fine");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("namespace", po::value<std::vector<std::string>>(&namespaces), "Only list pods in this namespace (repeatable). Default: all namespaces.")
    ("label-selector", po::value<std::string>(&opt.label_selector), "Label selector passed to the API server, e.g. app=web,tier!=cache: filtering happens before the payload is built.")
    ("field-selector", po::value<std::string>(&opt.field_selector), "Field selector passed to the API server, e.g. spec.nodeName=worker-1 or status.phase!=Succeeded.")
    ("pod", po::value<std::vector<std::string>>(&required), "Name of a pod that must exist (repeatable). Only the named pods take part in the check; a name the API server does not return gets pod_status 'missing'.")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for each API server request, in seconds.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  cluster target;
  std::string error;
  if (!resolve_cluster(defaults, target, error)) return fail(response, error);
  target.timeout = timeout;
  const fetcher fetch = make_fetcher(target);

  std::vector<json::value> items;
  if (!list_namespaced(fetch, target, "/api/v1", "pods", namespaces, opt, items, response)) return;

  // Only the requested pods take part in the check; a name may be given as
  // `pod` or `namespace/pod`.
  required_names wanted(required);
  for (const auto &v : items) {
    if (!v.is_object()) continue;
    auto record = parse_pod(v.as_object());
    if (!wanted.empty() && !wanted.claim(record->name, record->ns + "/" + record->name)) continue;
    filter.match(record);
  }

  // A required pod the API server does not know about is synthesised so it
  // shows up (and trips the default critical) instead of silently
  // disappearing from the listing.
  wanted.for_each_missing(default_namespace(namespaces), [&filter](const std::string &ns, const std::string &name) {
    auto record = std::make_shared<pod_obj>();
    record->ns = ns;
    record->name = name;
    record->phase = "missing";
    record->pod_status = "missing";
    filter.match(record);
  });

  filter_helper.post_process(filter);
}

}  // namespace kube_checks
