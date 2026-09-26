// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_workloads.hpp"

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/json.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

#include "kube_object.hpp"

namespace json = boost::json;
namespace po = boost::program_options;

namespace kube_checks {

namespace {

struct workload_obj {
  std::string kind, name, ns, labels;
  long long desired = 0, ready = 0, available = 0, updated = 0, unavailable = 0, missing = 0;
  bool paused = false;
  long long created = 0;
  long long age = -1;

  std::string show() const { return kind + " " + ns + "/" + name + ": " + std::to_string(available) + "/" + std::to_string(desired); }

  std::string get_kind() const { return kind; }
  std::string get_name() const { return name; }
  std::string get_namespace() const { return ns; }
  std::string get_labels() const { return labels; }
  long long get_desired() const { return desired; }
  long long get_ready() const { return ready; }
  long long get_available() const { return available; }
  long long get_updated() const { return updated; }
  long long get_unavailable() const { return unavailable; }
  long long get_missing() const { return missing; }
  long long get_paused() const { return paused ? 1 : 0; }
  long long get_created() const { return created; }
  long long get_age() const { return age; }
};

// The three kinds report their counts under different names; normalise them.
struct kind_spec {
  const char *kind;      // API kind, as `kind` renders it
  const char *resource;  // the list resource under /apis/apps/v1
};
const kind_spec KINDS[] = {{"Deployment", "deployments"}, {"StatefulSet", "statefulsets"}, {"DaemonSet", "daemonsets"}};

// The kind a `kind=` argument names (deployment, deployments, Deployment, ...);
// nullptr when it is none of them.
const kind_spec *kind_by_name(const std::string &arg) {
  std::string key = boost::algorithm::to_lower_copy(arg);
  if (!key.empty() && key.back() == 's') key.pop_back();
  for (const kind_spec &k : KINDS) {
    std::string kind = boost::algorithm::to_lower_copy(std::string(k.kind));
    if (key == kind) return &k;
  }
  return nullptr;
}

std::shared_ptr<workload_obj> parse_workload(const kind_spec &kind, const json::object &o) {
  auto record = std::make_shared<workload_obj>();
  const api_object workload(o);

  record->kind = kind.kind;
  record->name = workload.name();
  record->ns = workload.ns();
  record->labels = join_map(workload.metadata, "labels");
  const object_age age = age_of(workload.metadata);
  record->age = age.age;
  record->created = age.created;

  if (std::string(kind.kind) == "DaemonSet") {
    record->desired = get_num(workload.status, "desiredNumberScheduled");
    record->ready = get_num(workload.status, "numberReady");
    record->available = get_num(workload.status, "numberAvailable");
    record->updated = get_num(workload.status, "updatedNumberScheduled");
    record->unavailable = get_num(workload.status, "numberUnavailable");
  } else {
    // spec.replicas defaults to 1 when omitted.
    record->desired = workload.spec.if_contains("replicas") ? get_num(workload.spec, "replicas") : 1;
    record->ready = get_num(workload.status, "readyReplicas");
    // availableReplicas came to statefulsets in 1.22; before that ready is
    // the closest thing.
    record->available = workload.status.if_contains("availableReplicas") ? get_num(workload.status, "availableReplicas") : record->ready;
    record->updated = get_num(workload.status, "updatedReplicas");
    record->unavailable = get_num(workload.status, "unavailableReplicas");
    record->paused = get_bool(workload.spec, "paused");
  }
  // Not std::max: windows.h defines max as a macro and MSVC chokes on it.
  record->missing = record->desired > record->available ? record->desired - record->available : 0;
  return record;
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<workload_obj>> workload_context;
struct workload_obj_handler : public workload_context {
  workload_obj_handler() {
    registry_.add_string_var("kind", &workload_obj::get_kind, "Workload kind: Deployment, StatefulSet or DaemonSet")
        .add_string_var("name", &workload_obj::get_name, "Workload name")
        .add_string_var("namespace", &workload_obj::get_namespace, "Namespace the workload lives in")
        .add_string_var("labels", &workload_obj::get_labels, "Workload labels as key=value, comma separated");
    registry_.add_int_var("desired", &workload_obj::get_desired, "Replicas wanted (spec.replicas, or the nodes a daemonset should run on)")
        .add_int_perf("", "", " desired")
        .add_int_var("ready", &workload_obj::get_ready, "Replicas whose pod is Ready")
        .add_int_perf("", "", " ready")
        .add_int_var("available", &workload_obj::get_available, "Replicas available (Ready for at least minReadySeconds)")
        .add_int_perf("", "", " available")
        .add_int_var("updated", &workload_obj::get_updated, "Replicas running the current template (less than desired during a rollout)")
        .add_int_perf("", "", " updated")
        .add_int_var("unavailable", &workload_obj::get_unavailable, "Replicas the controller reports unavailable")
        .add_int_perf("", "", " unavailable")
        .add_int_var("missing", &workload_obj::get_missing, "desired minus available, never below 0")
        .add_int_perf("", "", " missing");
    registry_.add_int_var("paused", &workload_obj::get_paused, "1 when a deployment's rollout is paused, else 0").no_perf();
    register_age_keywords<workload_obj>(registry_, &workload_obj::get_created, &workload_obj::get_age, "the workload was created");
  }
};
typedef modern_filter::modern_filters<workload_obj, workload_obj_handler> workload_filter;

}  // namespace

void check_workloads(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                     PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<workload_filter> filter_helper(request, response, data);
  int timeout = defaults.timeout;
  std::vector<std::string> namespaces;
  std::vector<std::string> kinds;
  std::vector<std::string> required;
  list_options opt;

  workload_filter filter;
  // Nothing available where something is wanted is an outage; short of the
  // desired count, or a rollout that has not finished, is worth a look.
  filter_helper.add_options("missing > 0 or updated < desired", "available = 0 and desired > 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${problem_list}", "${kind} ${namespace}/${name}=${available}/${desired}", "${namespace}/${name}",
                           "%(status): No workloads found", "%(status): All %(count) workloads are available");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("namespace", po::value<std::vector<std::string>>(&namespaces), "Only list workloads in this namespace (repeatable). Default: all namespaces.")
    ("kind", po::value<std::vector<std::string>>(&kinds), "Only check this workload kind: deployment, statefulset or daemonset (repeatable). Default: all three.")
    ("label-selector", po::value<std::string>(&opt.label_selector), "Label selector passed to the API server, e.g. app.kubernetes.io/part-of=shop.")
    ("field-selector", po::value<std::string>(&opt.field_selector), "Field selector passed to the API server, e.g. metadata.name=web.")
    ("workload", po::value<std::vector<std::string>>(&required), "Name of a workload that must exist, as name or namespace/name (repeatable). Only the named workloads take part in the check; one the API server does not return is reported with 0 available of 1 desired.")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for each API server request, in seconds (a positive number).")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  std::vector<const kind_spec *> selected;
  for (const std::string &k : kinds) {
    const kind_spec *spec = kind_by_name(k);
    if (!spec)
      return nscapi::protobuf::functions::set_response_bad(*response, "Unknown workload kind '" + k + "': expected deployment, statefulset or daemonset");
    if (std::find(selected.begin(), selected.end(), spec) == selected.end()) selected.push_back(spec);
  }
  if (selected.empty()) {
    for (const kind_spec &k : KINDS) selected.push_back(&k);
  }

  if (!filter_helper.build_filter(filter)) return;

  cluster target;
  std::string error;
  if (!resolve_cluster(defaults, target, error)) return fail(response, error);
  if (!set_request_timeout(target, timeout, response)) return;
  fetcher fetch;
  if (!open_fetcher(make_fetcher, target, fetch, response)) return;

  required_names wanted(required);
  for (const kind_spec *kind : selected) {
    std::vector<json::value> items;
    if (!list_namespaced(fetch, target, "/apis/apps/v1", kind->resource, namespaces, opt, items, response)) return;
    for (const auto &v : items) {
      if (!v.is_object()) continue;
      auto record = parse_workload(*kind, v.as_object());
      if (!wanted.empty() && !wanted.claim(record->name, record->ns + "/" + record->name)) continue;
      filter.match(record);
    }
  }

  // A required workload the API server does not know about has nothing
  // available, which is what the default critical says.
  wanted.for_each_missing(default_namespace(namespaces), [&filter](const std::string &ns, const std::string &name) {
    auto record = std::make_shared<workload_obj>();
    record->ns = ns;
    record->name = name;
    record->kind = "missing";
    record->desired = 1;
    record->missing = 1;
    filter.match(record);
  });

  filter_helper.post_process(filter);
}

}  // namespace kube_checks
