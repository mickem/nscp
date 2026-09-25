// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_cluster.hpp"

#include <boost/json.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace json = boost::json;
namespace po = boost::program_options;

namespace kube_checks {

namespace {

struct cluster_obj {
  std::string server, source, version, platform, readyz;
  bool api_ready = false;
  long long nodes = 0, nodes_ready = 0, nodes_not_ready = 0;

  std::string show() const { return "Kubernetes " + version + " at " + server; }

  std::string get_server() const { return server; }
  std::string get_source() const { return source; }
  std::string get_version() const { return version; }
  std::string get_platform() const { return platform; }
  std::string get_readyz() const { return readyz; }
  long long get_api_ready() const { return api_ready ? 1 : 0; }
  std::string get_api_ready_str() const { return api_ready ? "ready" : "not ready"; }
  long long get_nodes() const { return nodes; }
  long long get_nodes_ready() const { return nodes_ready; }
  long long get_nodes_not_ready() const { return nodes_not_ready; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<cluster_obj>> cluster_context;
struct cluster_obj_handler : public cluster_context {
  cluster_obj_handler() {
    registry_.add_string_var("server", &cluster_obj::get_server, "The API server address (never the credential)")
        .add_string_var("source", &cluster_obj::get_source, "Where the cluster configuration came from: settings, kubeconfig <path> or in-cluster")
        .add_string_var("version", &cluster_obj::get_version, "Kubernetes version reported by /version (e.g. v1.30.2)")
        .add_string_var("platform", &cluster_obj::get_platform, "Platform the API server runs on (e.g. linux/amd64)")
        .add_string_var("readyz", &cluster_obj::get_readyz, "What /readyz answered: ok, or the failing checks it listed");
    registry_
        .add_int_var("api_ready", &cluster_obj::get_api_ready, &cluster_obj::get_api_ready_str,
                     "1 when /readyz answers ok, 0 when the API server reports itself not ready (renders as ready / not ready)")
        .no_perf();
    registry_.add_int_var("nodes", &cluster_obj::get_nodes, "Number of nodes in the cluster")
        .add_int_perf("", "", " nodes")
        .add_int_var("nodes_ready", &cluster_obj::get_nodes_ready, "Number of nodes whose Ready condition is True")
        .add_int_perf("", "", " ready nodes")
        .add_int_var("nodes_not_ready", &cluster_obj::get_nodes_not_ready, "Number of nodes whose Ready condition is False or Unknown")
        .add_int_perf("", "", " not ready nodes");
  }
};
typedef modern_filter::modern_filters<cluster_obj, cluster_obj_handler> cluster_filter;

// /readyz answers "ok" when healthy and, on failure, a list of "[+]check ok" /
// "[-]check failed" lines. Reduce that to the failing check names.
std::string summarize_readyz(const std::string &body, const long status) {
  std::string failing;
  std::string line;
  for (std::size_t i = 0; i <= body.size(); ++i) {
    if (i < body.size() && body[i] != '\n') {
      line.push_back(body[i]);
      continue;
    }
    if (line.find("[-]") == 0) {
      std::string name = line.substr(3);
      const auto sp = name.find(' ');
      if (sp != std::string::npos) name = name.substr(0, sp);
      str::format::append_list(failing, name, ",");
    }
    line.clear();
  }
  if (!failing.empty()) return "failed: " + failing;
  std::string trimmed = body;
  boost::algorithm::trim(trimmed);
  if (trimmed == "ok") return "ok";
  return "HTTP " + std::to_string(status);
}

}  // namespace

void check_cluster(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                   const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<cluster_filter> filter_helper(request, response, data);
  int timeout = defaults.timeout;

  cluster_filter filter;
  // Reaching the API is the health signal; a server that answers /readyz
  // with a failure is critical, everything else is the operator's threshold.
  filter_helper.add_options("", "api_ready = 0", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "Kubernetes ${version} at ${server}: API ${api_ready}, ${nodes_ready}/${nodes} nodes ready", "${server}",
                           "%(status): No cluster information returned", "");
  // clang-format off
  filter_helper.get_desc().add_options()
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

  auto record = std::make_shared<cluster_obj>();
  record->server = target.address();
  record->source = target.source;

  json::value version;
  if (!fetch_json(fetch, target, "/version", version, response)) return;
  if (!version.is_object()) return fail(response, "Failed to parse the Kubernetes API server response from /version: expected an object");
  record->version = get_str(version.as_object(), "gitVersion");
  record->platform = get_str(version.as_object(), "platform");

  std::string body;
  long status = 0;
  const raw_fetch ready = fetch_raw(fetch, target, "/readyz", body, status, response);
  if (ready == raw_fetch::failed) return;
  record->api_ready = ready == raw_fetch::ok;
  record->readyz = summarize_readyz(body, status);

  std::vector<json::value> nodes;
  if (!list_all(fetch, target, "/api/v1/nodes", list_options(), nodes, response)) return;
  for (const auto &n : nodes) {
    if (!n.is_object()) continue;
    ++record->nodes;
    const json::object *st = get_obj(n.as_object(), "status");
    if (st && condition_status(*st, "Ready") == "True")
      ++record->nodes_ready;
    else
      ++record->nodes_not_ready;
  }

  filter.match(record);
  filter_helper.post_process(filter);
}

}  // namespace kube_checks
