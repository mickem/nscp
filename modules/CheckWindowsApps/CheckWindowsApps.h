// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

// One module for the Windows application/server-role checks (IIS, RDS, and
// whatever role comes next). Each role keeps its own check_<role>_*.cpp/.hpp
// pair; this class is only the dispatch surface the generated module glue
// binds to. A role earns a module of its own only when it drags in a heavy or
// optional dependency - these are all plain PDH + WMI.
class CheckWindowsApps : public nscapi::impl::simple_plugin {
 public:
  CheckWindowsApps() {}

  bool loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // IIS check commands
  static void check_iis_app_pools(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_sites(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_worker_processes(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_iis_request_queues(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // Remote Desktop Services check commands
  static void check_rds_licenses(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_rds_sessions(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_rds_session_load(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_rds_broker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
