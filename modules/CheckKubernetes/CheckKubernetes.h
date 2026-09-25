// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>

#include "kube_settings.hpp"

class CheckKubernetes : public nscapi::impl::simple_plugin {
  kube_checks::settings defaults_;

 public:
  CheckKubernetes() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Check commands
  void check_kubernetes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_pods(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
