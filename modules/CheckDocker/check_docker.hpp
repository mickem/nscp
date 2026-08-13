// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace docker_checks {

// Module-level defaults (from /settings/docker), overridable per check.
struct settings {
  std::string endpoint;  // docker daemon socket / named pipe
  int timeout = 10;      // per-request timeout in seconds
};

// GET `path` from the docker daemon and return the response body; throws on
// connection or HTTP failure.
typedef std::function<std::string(const std::string &path)> fetcher;

// Creates a fetcher bound to a validated endpoint. Injectable so the checks
// (and their unit tests) never touch the HTTP client directly; the module
// wires in the real pipe/unix-socket transport from CheckDocker.cpp.
typedef std::function<fetcher(const std::string &endpoint, int timeout_seconds)> fetcher_factory;

// Check container state via GET /containers/json.
void check_containers(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                      PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher);

// Check daemon health via GET /info.
void check_info(const settings &defaults, const PB::Commands::QueryRequestMessage::Request &request,
                PB::Commands::QueryResponseMessage::Response *response, const fetcher_factory &make_fetcher);

}  // namespace docker_checks
