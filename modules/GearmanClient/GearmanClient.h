// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <string>

/**
 * Mod-Gearman worker for NSClient++.
 *
 * The module connects out to a gearmand job server, registers for the queues
 * a Naemon or Nagios Core routes its checks to, runs each check as a native
 * NSClient++ query and submits the result back. Nothing is ever accepted
 * inbound, so a monitored host needs no open port.
 *
 * This is the skeleton: the payload codec, the crypto and the job and result
 * text formats are implemented and tested (`gearman_protocol`,
 * `gearman_crypt`, `gearman_job`), but the connection and the worker loop that
 * use them are not here yet, so loading the module currently does nothing.
 */
class GearmanClient : public nscapi::impl::simple_plugin {
 public:
  GearmanClient();
  virtual ~GearmanClient();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
};
