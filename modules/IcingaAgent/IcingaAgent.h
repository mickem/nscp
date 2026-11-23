// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace icinga_agent {
class agent;
}

// Native Icinga 2 agent: connects to an Icinga 2 master or satellite over the
// cluster protocol (JSON-RPC over mutual TLS, port 5665), the way the
// PowerShell-only Icinga for Windows does, and runs the checks the master
// schedules on it as NSClient++ queries. No icinga2 binary, no local Icinga
// configuration.
class IcingaAgent : public nscapi::impl::simple_plugin {
 public:
  IcingaAgent();
  virtual ~IcingaAgent();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);

 private:
  void stop();
  // Where the certificates go, as configured: set on every load, so the
  // command line sees it too.
  std::string certificate_directory_;
  std::shared_ptr<icinga_agent::agent> agent_;
};
