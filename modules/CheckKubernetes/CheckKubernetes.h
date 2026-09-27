// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <mutex>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <set>
#include <string>

#include "kube_client.hpp"
#include "kube_settings.hpp"

class CheckKubernetes : public nscapi::impl::simple_plugin {
  kube_checks::settings defaults_;
  // Servers already warned about (plain http with a token), so the warning
  // is logged once per server rather than once per check. Checks run
  // concurrently, hence the lock.
  std::mutex warned_mutex_;
  std::set<std::string> warned_targets_;

  kube_checks::fetcher make_api_fetcher(const kube_checks::cluster &target);
  kube_checks::fetcher_factory api_fetcher_factory() {
    return [this](const kube_checks::cluster &target) { return make_api_fetcher(target); };
  }
  bool first_warning_for(const std::string &key);

 public:
  CheckKubernetes() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Check commands
  void check_kubernetes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pods(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_nodes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_workloads(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
