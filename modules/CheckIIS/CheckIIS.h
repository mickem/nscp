// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

class CheckIIS : public nscapi::impl::simple_plugin {
 public:
  CheckIIS() {}

  bool loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Check commands
  static void check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
