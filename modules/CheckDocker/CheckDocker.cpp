// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDocker.h"

#include <ctime>
#include <net/http/client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <sstream>
#include <str/utf8.hpp>

#include "check_docker_df.hpp"
#include "check_docker_restarts.hpp"
#include "check_docker_stats.hpp"
#include "docker_client.hpp"
#include "docker_endpoint.hpp"
#include "docker_facts.hpp"

namespace sh = nscapi::settings_helper;

namespace {
// The real transport: named pipe on Windows, unix domain socket elsewhere
// (both are the "pipe" protocol of the http client). The endpoint has been
// validated by the check before this factory is invoked.
docker_checks::fetcher make_daemon_fetcher(const std::string &endpoint, const int timeout_seconds) {
  return [endpoint, timeout_seconds](const std::string &path) -> std::string {
    // The server argument only feeds the Host header; the daemon does not
    // route on it, but modern daemons reject requests without one ("400 Bad
    // Request"), same as curl's --unix-socket which sends Host: localhost.
    http::request rq("GET", "localhost", path);
    rq.add_default_headers();
    http::http_client_options options("pipe", "", "", "");
    options.timeout_seconds_ = timeout_seconds > 0 ? static_cast<unsigned int>(timeout_seconds) : 10;
    http::simple_client client(options);
    // fetch(), not execute(): execute() throws on any non-2xx, collapsing a
    // container-gone 404 into the same failure as an unreachable daemon. fetch()
    // hands back the status so a 404 can be told apart and skipped (see
    // docker_http_error / fetch_json_item). Docker responses are well under
    // fetch()'s 5 MB buffer cap, and it decodes chunked bodies for us.
    const http::response resp = client.fetch(endpoint, "", rq);
    if (!resp.is_2xx()) {
      throw docker_checks::docker_http_error(resp.status_code_, "HTTP " + std::to_string(resp.status_code_) + " " + resp.status_message_);
    }
    return resp.payload_;
  };
}
}  // namespace

bool CheckDocker::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "docker");

    // clang-format off
    settings.alias().add_key_to_settings()
      .add_string("endpoint", sh::string_key(&defaults_.endpoint, docker_checks::default_docker_endpoint()),
        "DOCKER ENDPOINT", "The local docker daemon socket: a named pipe (\\\\.\\pipe\\docker_engine) on Windows, a unix socket (/var/run/docker.sock) elsewhere.")
      .add_int("timeout", sh::int_key(&defaults_.timeout, 10),
        "TIMEOUT", "Timeout for talking to the daemon, in seconds.", true)
      ;
    // clang-format on

    bool facts_daemon = false;
    bool facts_containers = false;
    bool facts_images = false;
    // clang-format off
    settings.alias().add_key_to_settings("facts")
      .add_bool(docker_facts::id_daemon, sh::bool_key(&facts_daemon, false),
        "DOCKER DAEMON FACTS",
        "Collect the daemon record of the `docker` fact set: the docker version, the OS and architecture it runs on, the kernel, the storage and "
        "cgroup drivers, the CPU and memory it sees and whether it is a swarm node. Not the container or image counts: those are monitoring, "
        "and they live in check_docker_info. One GET /info per facts round. Nothing is collected while this is off.")
      .add_bool(docker_facts::id_containers, sh::bool_key(&facts_containers, false),
        "DOCKER CONTAINERS FACTS",
        "Collect the `docker.containers` fact set: one record per container the daemon knows, stopped ones included - its names (the record id, "
        "the same value check_docker calls `names`), the image it was created from, when it was created, its published and exposed ports and "
        "the compose project and service it belongs to. Not its state: that is monitoring, and it lives in check_docker. Cheap - the same "
        "listing check_docker all=true does, re-read every facts round because containers come and go. Nothing is collected while this is off.")
      .add_bool(docker_facts::id_images, sh::bool_key(&facts_images, false),
        "DOCKER IMAGES FACTS",
        "Collect the `docker.images` fact set: one record per image the daemon holds - its first tag (the record id; the image id when it has "
        "none), every tag, when it was built and its size. One GET /images/json per facts round. Nothing is collected while this is off.")
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    // Which parts of the set fetchFacts builds is configuration, so it is
    // re-read on every load, a reload included: the core drops a set a
    // producer stops returning, and that is what turning it off means.
    facts_daemon_.store(facts_daemon);
    facts_containers_.store(facts_containers);
    facts_images_.store(facts_images);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("loading: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading: ");
    return false;
  }
  return true;
}

bool CheckDocker::unloadModule() { return true; }

void CheckDocker::fetchFacts(const nscapi::facts::request &, nscapi::facts::response &response) {
  docker_facts::selection what;
  what.daemon = facts_daemon_.load();
  what.containers = facts_containers_.load();
  what.images = facts_images_.load();
  if (!what.any()) return;

  // Every round, whatever its reason: containers are started and removed and
  // images pulled under a running agent, and each part costs one request to
  // the daemon. The endpoint is the configured one - a facts round has no
  // request to take a `host=` from - and it is held to the same rule as the
  // checks hold theirs to (see is_local_docker_endpoint).
  const std::string endpoint = defaults_.endpoint.empty() ? docker_checks::default_docker_endpoint() : defaults_.endpoint;
  std::string endpoint_error;
  if (!docker_checks::is_local_docker_endpoint(endpoint, endpoint_error)) {
    response.error(docker_facts::set_docker, endpoint_error);
    return;
  }
  try {
    const docker_facts::snapshot snap = docker_facts::gather(what, make_daemon_fetcher(endpoint, defaults_.timeout), endpoint);
    docker_facts::publish(what, snap, std::time(nullptr), response);
  } catch (const std::exception &e) {
    // Named against the set rather than failing the round: the core keeps the
    // containers it already holds and reports why they are stale. A daemon
    // that is down for a minute must not blank the inventory.
    response.error(docker_facts::set_docker, utf8::utf8_from_native(e.what()));
  }
}

void CheckDocker::check_docker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check_containers(defaults_, request, response, &make_daemon_fetcher);
}

void CheckDocker::check_docker_info(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check_info(defaults_, request, response, &make_daemon_fetcher);
}

void CheckDocker::check_docker_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check_stats(defaults_, request, response, &make_daemon_fetcher);
}

void CheckDocker::check_docker_restarts(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check_restarts(defaults_, request, response, &make_daemon_fetcher);
}

void CheckDocker::check_docker_df(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  docker_checks::check_df(defaults_, request, response, &make_daemon_fetcher);
}
