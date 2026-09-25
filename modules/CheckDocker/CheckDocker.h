// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>

#include "check_docker.hpp"

class CheckDocker : public nscapi::impl::simple_plugin {
  docker_checks::settings defaults_;
  // Which parts of the `docker` fact set this module is configured to produce
  // ([/settings/docker/facts]). Read by fetchFacts on the core's scheduler
  // thread, written by loadModuleEx on every load, hence atomic.
  std::atomic<bool> facts_daemon_{false};
  std::atomic<bool> facts_containers_{false};
  std::atomic<bool> facts_images_{false};

 public:
  CheckDocker() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);

  // Check commands
  void check_docker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_docker_info(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_docker_stats(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_docker_restarts(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_docker_df(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
