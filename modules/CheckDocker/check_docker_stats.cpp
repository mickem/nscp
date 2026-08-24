// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_docker_stats.hpp"

#include "docker_client.hpp"
#include "docker_endpoint.hpp"

#include <boost/json.hpp>
#include <memory>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

namespace json = boost::json;
namespace po = boost::program_options;

namespace docker_checks {

namespace {

struct stats_obj {
  std::string names, image;
  long long cpu_pct = 0;
  long long memory_used = 0;
  long long memory_limit = 0;

  std::string show() const { return names + ": cpu " + std::to_string(cpu_pct) + "%, memory " + std::to_string(get_memory_pct()) + "%"; }

  std::string get_names() const { return names; }
  std::string get_image() const { return image; }
  long long get_cpu_pct() const { return cpu_pct; }
  long long get_memory_used() const { return memory_used; }
  long long get_memory_limit() const { return memory_limit; }
  long long get_memory_pct() const { return memory_limit > 0 ? memory_used * 100 / memory_limit : 0; }
  std::string get_memory_human(parsers::where::evaluation_context context) const {
    return str::format::format_byte_units(memory_used, context->get_number_format()) + " of " +
           str::format::format_byte_units(memory_limit, context->get_number_format());
  }
};

// One /containers/{id}/stats?stream=false sample -> cpu% and memory usage.
// Mirrors what `docker stats` shows: cpu = delta of the container's total
// usage over the delta of the host's, scaled by online CPUs; memory excludes
// the page cache (inactive_file on cgroup v2, total_inactive_file on v1).
void parse_stats(const json::object &o, const std::shared_ptr<stats_obj> &record) {
  if (const json::object *mem = get_obj(o, "memory_stats")) {
    long long usage = get_num(*mem, "usage");
    record->memory_limit = get_num(*mem, "limit");
    if (const json::object *details = get_obj(*mem, "stats")) {
      const long long inactive = details->if_contains("total_inactive_file") ? get_num(*details, "total_inactive_file") : get_num(*details, "inactive_file");
      if (inactive < usage) usage -= inactive;
    }
    record->memory_used = usage;
  }

  const json::object *cpu = get_obj(o, "cpu_stats");
  const json::object *precpu = get_obj(o, "precpu_stats");
  if (cpu != nullptr && precpu != nullptr) {
    const json::object *usage = get_obj(*cpu, "cpu_usage");
    const json::object *preusage = get_obj(*precpu, "cpu_usage");
    if (usage != nullptr && preusage != nullptr) {
      const long long cpu_delta = get_num(*usage, "total_usage") - get_num(*preusage, "total_usage");
      const long long system_delta = get_num(*cpu, "system_cpu_usage") - get_num(*precpu, "system_cpu_usage");
      long long online = get_num(*cpu, "online_cpus");
      if (online <= 0) online = 1;
      if (cpu_delta > 0 && system_delta > 0) {
        record->cpu_pct = cpu_delta * 100 * online / system_delta;
      }
    }
  }
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<stats_obj>> stats_context;
struct stats_obj_handler : public stats_context {
  stats_obj_handler() {
    registry_.add_string_var("names", &stats_obj::get_names, "Container name(s), comma separated")
        .add_string_var("image", &stats_obj::get_image, "Image the container was created from")
        .add_string_var_w_context("memory", &stats_obj::get_memory_human,
                                  "Memory usage as human readable text, e.g. 45.2M of 512M (display only; threshold on memory_used or memory_pct)");
    registry_.add_int_var("cpu_pct", &stats_obj::get_cpu_pct, "CPU usage in percent of the host (like docker stats)")
        .add_int_perf("%", "", " cpu")
        .add_int_var("memory_pct", &stats_obj::get_memory_pct, "Memory usage in percent of the container's limit")
        .add_int_perf("%", "", " memory %");
    registry_.add_int_var("memory_used", parsers::where::type_size, &stats_obj::get_memory_used,
                          "Memory used in bytes, page cache excluded (supports size units, e.g. memory_used > 200M)")
        .add_int_perf("B", "", " memory")
        .add_int_var("memory_limit", parsers::where::type_size, &stats_obj::get_memory_limit,
                     "Memory limit in bytes (the host's total memory when the container is unlimited)")
        .no_perf();
  }
};
typedef modern_filter::modern_filters<stats_obj, stats_obj_handler> stats_filter;

}  // namespace

void check_stats(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                 const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<stats_filter> filter_helper(request, response, data);
  std::string endpoint = defaults.endpoint.empty() ? default_docker_endpoint() : defaults.endpoint;
  int timeout = defaults.timeout;
  std::vector<std::string> only;

  stats_filter filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%)", "${names}", "No running containers", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&endpoint)->default_value(endpoint), "The local docker daemon socket (named pipe on Windows, unix socket elsewhere).")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for talking to the daemon, in seconds.")
    ("container", po::value<std::vector<std::string>>(&only), "Only sample the named container (repeatable). Sampling takes about a second per container, so scope this check on busy hosts.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // See check_containers for why the endpoint must be constrained.
  std::string endpoint_error;
  if (!is_local_docker_endpoint(endpoint, endpoint_error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, endpoint_error);
  }

  if (!filter_helper.build_filter(filter)) return;

  const fetcher fetch = make_fetcher(endpoint, timeout);
  json::value root;
  if (!fetch_json(fetch, endpoint, std::string(API) + "/containers/json", root, response)) return;
  if (!root.is_array()) {
    return fail(response, "Failed to parse docker daemon response from /containers/json: expected a list of containers");
  }

  for (const auto &v : root.as_array()) {
    if (!v.is_object()) continue;
    const json::object &c = v.as_object();

    auto record = std::make_shared<stats_obj>();
    record->image = get_str(c, "Image");
    if (const json::value *names = c.if_contains("Names")) {
      if (names->is_array()) {
        for (const auto &name : names->as_array()) {
          if (name.is_string()) str::format::append_list(record->names, strip_slash(name.as_string().c_str()), ",");
        }
      }
    }

    if (!only.empty()) {
      bool matched = false;
      for (const std::string &want : only) {
        if (("," + record->names + ",").find("," + strip_slash(want) + ",") != std::string::npos) matched = true;
      }
      if (!matched) continue;
    }

    // stream=false makes the daemon take two samples ~1s apart so the cpu
    // delta is meaningful (one-shot mode leaves precpu empty). That ~1s per
    // container widens the window for the container to be removed mid-check, so
    // a 404 here is if anything more likely than in check_restarts: skip the
    // vanished container rather than failing the whole check.
    json::value stats;
    switch (fetch_json_item(fetch, endpoint, std::string(API) + "/containers/" + get_str(c, "Id") + "/stats?stream=false", stats, response)) {
      case item_fetch::vanished:
        continue;
      case item_fetch::failed:
        return;
      case item_fetch::ok:
        break;
    }
    if (stats.is_object()) parse_stats(stats.as_object(), record);

    filter.match(record);
  }

  filter_helper.post_process(filter);
}

}  // namespace docker_checks
