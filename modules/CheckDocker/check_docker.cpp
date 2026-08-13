// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_docker.hpp"

#include "docker_client.hpp"
#include "docker_endpoint.hpp"

#include <algorithm>
#include <boost/json.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

namespace json = boost::json;
namespace po = boost::program_options;

namespace docker_checks {

// --- check_docker (containers) ----------------------------------------------

namespace {

struct container_obj {
  std::string id, names, image, image_id, command, state, status, health, ip, ports, labels;
  long long created = 0;

  std::string show() const { return names + " (" + image + "): " + state + " " + status; }

  std::string get_id() const { return id; }
  std::string get_names() const { return names; }
  std::string get_image() const { return image; }
  std::string get_image_id() const { return image_id; }
  std::string get_command() const { return command; }
  std::string get_state() const { return state; }
  std::string get_status() const { return status; }
  std::string get_health() const { return health; }
  std::string get_ip() const { return ip; }
  std::string get_ports() const { return ports; }
  std::string get_labels() const { return labels; }
  long long get_created() const { return created; }
  long long get_has_health_check() const { return health.empty() ? 0 : 1; }
};

// The health portion of the human status text ("Up 3 hours (healthy)").
std::string parse_health(const std::string &status) {
  if (status.find("(healthy)") != std::string::npos) return "healthy";
  if (status.find("(unhealthy)") != std::string::npos) return "unhealthy";
  if (status.find("(health: starting)") != std::string::npos) return "starting";
  return "";
}

std::shared_ptr<container_obj> parse_container(const json::object &o) {
  auto record = std::make_shared<container_obj>();
  record->id = get_str(o, "Id");
  record->image = get_str(o, "Image");
  record->image_id = get_str(o, "ImageID");
  record->command = get_str(o, "Command");
  record->state = get_str(o, "State");
  record->status = get_str(o, "Status");
  record->health = parse_health(record->status);
  record->created = get_num(o, "Created");

  if (const json::value *names = o.if_contains("Names")) {
    if (names->is_array()) {
      for (const auto &name : names->as_array()) {
        if (name.is_string()) str::format::append_list(record->names, strip_slash(name.as_string().c_str()), ",");
      }
    }
  }

  // First IP found on any attached network (containers are commonly on
  // custom networks, not just "bridge").
  if (const json::value *ns = o.if_contains("NetworkSettings")) {
    if (ns->is_object()) {
      if (const json::value *nets = ns->as_object().if_contains("Networks")) {
        if (nets->is_object()) {
          for (const auto &net : nets->as_object()) {
            if (!net.value().is_object()) continue;
            const std::string ip = get_str(net.value().as_object(), "IPAddress");
            if (!ip.empty()) {
              record->ip = ip;
              break;
            }
          }
        }
      }
    }
  }

  // "0.0.0.0:8080->80/tcp" for published ports, "80/tcp" for unpublished.
  if (const json::value *ports = o.if_contains("Ports")) {
    if (ports->is_array()) {
      for (const auto &p : ports->as_array()) {
        if (!p.is_object()) continue;
        const json::object &po_ = p.as_object();
        const long long private_port = get_num(po_, "PrivatePort");
        const long long public_port = get_num(po_, "PublicPort");
        const std::string type = get_str(po_, "Type");
        std::string entry;
        if (public_port > 0) {
          const std::string ip = get_str(po_, "IP");
          entry = (ip.empty() ? "" : ip + ":") + std::to_string(public_port) + "->";
        }
        entry += std::to_string(private_port) + (type.empty() ? "" : "/" + type);
        str::format::append_list(record->ports, entry, ",");
      }
    }
  }

  if (const json::value *labels = o.if_contains("Labels")) {
    if (labels->is_object()) {
      for (const auto &l : labels->as_object()) {
        if (l.value().is_string()) str::format::append_list(record->labels, std::string(l.key()) + "=" + l.value().as_string().c_str(), ",");
      }
    }
  }
  return record;
}

typedef parsers::where::filter_handler_impl<std::shared_ptr<container_obj>> container_context;
struct container_obj_handler : public container_context {
  container_obj_handler() {
    registry_.add_string_var("id", &container_obj::get_id, "Container id")
        .add_string_var("names", &container_obj::get_names, "Container name(s), comma separated")
        .add_string_var("image", &container_obj::get_image, "Image the container was created from")
        .add_string_var("image_id", &container_obj::get_image_id, "Id of the image the container was created from")
        .add_string_var("command", &container_obj::get_command, "Command the container runs")
        .add_string_var("container_state", &container_obj::get_state,
                        "Container state: created, restarting, running, removing, paused, exited, dead or missing (a requested container the daemon does not "
                        "know about)")
        .add_string_var("status", &container_obj::get_status, "Human readable status, e.g. 'Up 3 hours (healthy)'")
        .add_string_var("health", &container_obj::get_health, "Health-check state: healthy, unhealthy, starting or empty when the container has no health check")
        .add_string_var("ip", &container_obj::get_ip, "First IP address on any network the container is attached to")
        .add_string_var("ports", &container_obj::get_ports, "Published/exposed ports, e.g. 0.0.0.0:8080->80/tcp")
        .add_string_var("labels", &container_obj::get_labels, "Container labels as key=value, comma separated");
    registry_.add_int_var("created", parsers::where::type_date, &container_obj::get_created, "When the container was created").no_perf();
    // The where-parser has no empty-string literal, so "has a health check at
    // all" needs its own keyword (filter=has_health_check = 1).
    registry_.add_int_var("has_health_check", &container_obj::get_has_health_check, "1 when the container defines a health check, else 0").no_perf();
  }
};
typedef modern_filter::modern_filters<container_obj, container_obj_handler> container_filter;

}  // namespace

void check_containers(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                      PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<container_filter> filter_helper(request, response, data);
  std::string endpoint = defaults.endpoint.empty() ? default_docker_endpoint() : defaults.endpoint;
  int timeout = defaults.timeout;
  bool all = false;
  std::vector<std::string> required;

  container_filter filter;
  filter_helper.add_options("", "container_state != 'running'", "", filter.get_filter_syntax(), "warning");
  filter_helper.add_syntax("${status}: ${list}", "${names}=${container_state}", "${names}", "No containers found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&endpoint)->default_value(endpoint), "The local docker daemon socket (named pipe on Windows, unix socket elsewhere).")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for talking to the daemon, in seconds.")
    ("all", po::value<bool>(&all)->implicit_value(true)->default_value(false), "Include stopped containers (docker ps -a); by default only running containers are listed.")
    ("container", po::value<std::vector<std::string>>(&required), "Name of a container that must exist (repeatable). Implies all; a name the daemon does not know gets container_state 'missing'.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // `host` is a check argument, so it arrives from whoever can run this check
  // - over REST that is anyone holding `queries.execute`. On Windows it is
  // handed to CreateFileA: a UNC target such as \\attacker\pipe\x would make
  // this host authenticate to a remote server over SMB. Constrain it to a
  // local endpoint before it gets anywhere near the transport.
  std::string endpoint_error;
  if (!is_local_docker_endpoint(endpoint, endpoint_error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, endpoint_error);
  }

  if (!filter_helper.build_filter(filter)) return;

  json::value root;
  const std::string path = std::string(API) + "/containers/json" + (all || !required.empty() ? "?all=true" : "");
  if (!fetch_json(make_fetcher(endpoint, timeout), endpoint, path, root, response)) return;
  if (!root.is_array()) {
    return fail(response, "Failed to parse docker daemon response from " + path + ": expected a list of containers");
  }

  std::vector<bool> seen(required.size(), false);
  for (const auto &v : root.as_array()) {
    if (!v.is_object()) continue;
    auto record = parse_container(v.as_object());
    if (!required.empty()) {
      // Only the requested containers take part in the check.
      bool matched = false;
      for (std::size_t i = 0; i < required.size(); i++) {
        std::string want = strip_slash(required[i]);
        if (("," + record->names + ",").find("," + want + ",") != std::string::npos) {
          seen[i] = true;
          matched = true;
        }
      }
      if (!matched) continue;
    }
    filter.match(record);
  }

  // A required container the daemon does not know about at all: synthesize a
  // record so it shows up (and trips the default critical) instead of
  // silently disappearing from the listing.
  for (std::size_t i = 0; i < required.size(); i++) {
    if (seen[i]) continue;
    auto record = std::make_shared<container_obj>();
    record->names = strip_slash(required[i]);
    record->state = "missing";
    record->status = "no such container";
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

// --- check_docker_info (daemon health) ---------------------------------------

namespace {

struct info_obj {
  std::string version, name, os;
  long long containers = 0, running = 0, paused = 0, stopped = 0, images = 0;

  std::string show() const { return "docker " + version + " on " + name; }

  std::string get_version() const { return version; }
  std::string get_name() const { return name; }
  std::string get_os() const { return os; }
  long long get_containers() const { return containers; }
  long long get_running() const { return running; }
  long long get_paused() const { return paused; }
  long long get_stopped() const { return stopped; }
  long long get_images() const { return images; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<info_obj>> info_context;
struct info_obj_handler : public info_context {
  info_obj_handler() {
    registry_.add_string_var("version", &info_obj::get_version, "Docker server version")
        .add_string_var("name", &info_obj::get_name, "Daemon host name")
        .add_string_var("os", &info_obj::get_os, "Operating system the daemon runs on");
    registry_.add_int_var("containers", &info_obj::get_containers, "Total number of containers")
        .add_int_perf("", "", " containers")
        .add_int_var("running", &info_obj::get_running, "Number of running containers")
        .add_int_perf("", "", " running")
        .add_int_var("paused", &info_obj::get_paused, "Number of paused containers")
        .add_int_perf("", "", " paused")
        .add_int_var("stopped", &info_obj::get_stopped, "Number of stopped containers")
        .add_int_perf("", "", " stopped")
        .add_int_var("images", &info_obj::get_images, "Number of images")
        .add_int_perf("", "", " images");
  }
};
typedef modern_filter::modern_filters<info_obj, info_obj_handler> info_filter;

}  // namespace

void check_info(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const fetcher_factory &make_fetcher) {
  modern_filter::data_container data;
  modern_filter::cli_helper<info_filter> filter_helper(request, response, data);
  std::string endpoint = defaults.endpoint.empty() ? default_docker_endpoint() : defaults.endpoint;
  int timeout = defaults.timeout;

  info_filter filter;
  // Reaching the daemon is the health signal: a responding daemon is OK
  // unless the user thresholds the counts themselves.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}",
                           "docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images", "${name}",
                           "%(status): No daemon information returned", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("host", po::value<std::string>(&endpoint)->default_value(endpoint), "The local docker daemon socket (named pipe on Windows, unix socket elsewhere).")
    ("timeout", po::value<int>(&timeout)->default_value(timeout), "Timeout for talking to the daemon, in seconds.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  // See check_containers for why the endpoint must be constrained.
  std::string endpoint_error;
  if (!is_local_docker_endpoint(endpoint, endpoint_error)) {
    return nscapi::protobuf::functions::set_response_bad(*response, endpoint_error);
  }

  if (!filter_helper.build_filter(filter)) return;

  json::value root;
  if (!fetch_json(make_fetcher(endpoint, timeout), endpoint, std::string(API) + "/info", root, response)) return;
  if (!root.is_object()) {
    return fail(response, "Failed to parse docker daemon response from /info: expected an object");
  }
  const json::object &o = root.as_object();

  auto record = std::make_shared<info_obj>();
  record->version = get_str(o, "ServerVersion");
  record->name = get_str(o, "Name");
  record->os = get_str(o, "OperatingSystem");
  record->containers = get_num(o, "Containers");
  record->running = get_num(o, "ContainersRunning");
  record->paused = get_num(o, "ContainersPaused");
  record->stopped = get_num(o, "ContainersStopped");
  record->images = get_num(o, "Images");

  filter.match(record);
  filter_helper.post_process(filter);
}

}  // namespace docker_checks
