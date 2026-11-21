// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>

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

 private:
  void stop();
  std::shared_ptr<icinga_agent::agent> agent_;
};
