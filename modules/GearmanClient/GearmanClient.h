// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <string>

#include "gearman_worker.hpp"

/**
 * Mod-Gearman worker for NSClient++.
 *
 * The module connects out to a gearmand job server, registers for the queues
 * a Naemon or Nagios Core routes its checks to, runs each check as a native
 * NSClient++ query and submits the result back. Nothing is ever accepted
 * inbound, so a monitored host needs no open port.
 *
 * Two deployments, one loop, told apart by the `mode` setting. An *agent*
 * answers for the host it runs on: a job is refused unless its `host_name` is
 * one this agent answers for. A *proxy* answers for others: the binding is
 * off, and the check_command the core defines names its own target, so one
 * box runs a whole hostgroup's checks through NRPEClient, NSCPClient or
 * CheckWMI without an agent, or an open port, on any of them.
 */
class GearmanClient : public nscapi::impl::simple_plugin {
 public:
  GearmanClient();
  virtual ~GearmanClient();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

 private:
  /**
   * Read the settings into a worker configuration. Returns false, having
   * logged why, when the module must not start: no server, no queues, or a
   * key that would leave the payloads readable by anyone who can reach
   * gearmand.
   */
  bool build_config(const std::string &alias, gearman::worker_config &config);

  gearman::worker_pool pool_;
};
