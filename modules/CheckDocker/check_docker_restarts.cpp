// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_docker_restarts.hpp"

#include "docker_client.hpp"
#include "docker_endpoint.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
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

// Seconds since an RFC3339 timestamp ("2026-08-12T07:44:00.123456789Z");
// -1 when absent, unparsable or the zero-value "0001-01-01T00:00:00Z" the
// daemon reports for containers that never started.
long long seconds_since(const std::string &rfc3339) {
  if (rfc3339.empty() || rfc3339[0] == '0') return -1;
  std::string s = rfc3339;
  const auto dot = s.find('.');
  if (dot != std::string::npos) {
    s = s.substr(0, dot);
  } else if (!s.empty() && s.back() == 'Z') {
    s.pop_back();
  }
  try {
    const boost::posix_time::ptime t = boost::posix_time::from_iso_extended_string(s);
    const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
    return (now - t).total_seconds();
  } catch (const std::exception &) {
    return -1;
  }
}

struct restart_obj {
  std::string names, image, state;
  long long restart_count = 0;
  long long exit_code = 0;
  long long started = -1;  // seconds since last start, -1 = never/unknown
  bool oom_killed = false;

  std::string show() const { return names + ": " + state + ", " + std::to_string(restart_count) + " restarts"; }

  std::string get_names() const { return names; }
  std::string get_image() const { return image; }
  std::string get_state() const { return state; }
  long long get_restart_count() const { return restart_count; }
  long long get_exit_code() const { return exit_code; }
  long long get_started() const { return started; }
  long long get_oom_killed() const { return oom_killed ? 1 : 0; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<restart_obj>> restart_context;
struct restart_obj_handler : public restart_context {
  restart_obj_handler() {
    registry_.add_string_var("names", &restart_obj::get_names, "Container name(s), comma separated")
        .add_string_var("image", &restart_obj::get_image, "Image the container was created from")
        .add_string_var("container_state", &restart_obj::get_state, "Container state: created, restarting, running, removing, paused, exited or dead");
    registry_.add_int_var("restart_count", &restart_obj::get_restart_count, "How many times the container has been restarted (since creation)")
        .add_int_perf("", "", " restarts");
    registry_.add_int_var("exit_code", &restart_obj::get_exit_code, "Exit code of the last exit (0 while running fine)").no_perf();
    registry_.add_int_var("oom_killed", &restart_obj::get_oom_killed, "1 when the last exit was an out-of-memory kill, else 0").no_perf();

    static const parsers::where::value_type type_custom_started = parsers::where::type_custom_int_1;
    registry_
        .add_int_var("started", type_custom_started, &restart_obj::get_started,
                     "Seconds since the container last started, -1 when it never started (supports units, e.g. started < 10m)")
        .no_perf();
    registry_.add_converter(type_custom_started, &parse_time<std::shared_ptr<restart_obj>>);
  }
};
typedef modern_filter::modern_filters<restart_obj, restart_obj_handler> restart_filter;

}  // namespace

void check_restarts(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                    PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<restart_filter> filter_helper(request, response, data);
  std::string endpoint = defaults.endpoint.empty() ? default_docker_endpoint() : defaults.endpoint;
  int timeout = defaults.timeout;
  std::vector<std::string> only;

  restart_filter filter;
  // A high cumulative restart count only signals a loop when the last start
  // is recent too - a container that has been up for a month is fine no
  // matter how bumpy its past. OOM kills are always worth a critical.
  filter_helper.add_options("restart_count > 3 and started < 15m and started >= 0", "oom_killed = 1", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${names}: ${restart_count} restarts, ${container_state}", "${names}", "No containers found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&endpoint)->default_value(endpoint), "The local docker daemon socket (named pipe on Windows, unix socket elsewhere).")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for talking to the daemon, in seconds.")
    ("container", po::value<std::vector<std::string>>(&only), "Only inspect the named container (repeatable).")
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
  // all=true: a crash-looping container spends most of its time not-running,
  // and a final crash leaves it exited - both must stay visible.
  if (!fetch_json(fetch, endpoint, std::string(API) + "/containers/json?all=true", root, response)) return;
  if (!root.is_array()) {
    return fail(response, "Failed to parse docker daemon response from /containers/json: expected a list of containers");
  }

  for (const auto &v : root.as_array()) {
    if (!v.is_object()) continue;
    const json::object &c = v.as_object();

    auto record = std::make_shared<restart_obj>();
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

    // RestartCount and the State details only exist in the inspect payload.
    // The container can be removed between the list call and this one (a
    // `--rm` job finishing, `docker compose run`, CI churn); a 404 there means
    // "gone", so drop it and keep reporting on the rest rather than failing the
    // whole check and blaming the socket.
    json::value inspected;
    switch (fetch_json_item(fetch, endpoint, std::string(API) + "/containers/" + get_str(c, "Id") + "/json", inspected, response)) {
      case item_fetch::vanished:
        continue;
      case item_fetch::failed:
        return;
      case item_fetch::ok:
        break;
    }
    if (inspected.is_object()) {
      const json::object &o = inspected.as_object();
      record->restart_count = get_num(o, "RestartCount");
      if (const json::object *state = get_obj(o, "State")) {
        record->state = get_str(*state, "Status");
        record->exit_code = get_num(*state, "ExitCode");
        record->oom_killed = get_bool(*state, "OOMKilled");
        record->started = seconds_since(get_str(*state, "StartedAt"));
      }
    }

    filter.match(record);
  }

  filter_helper.post_process(filter);
}

}  // namespace docker_checks
