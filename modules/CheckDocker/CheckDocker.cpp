// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckDocker.h"

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

    settings.register_all();
    settings.notify();
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
