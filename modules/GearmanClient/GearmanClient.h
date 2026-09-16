// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>
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
 *
 * The same module is also a passive channel. The core's result thread files
 * whatever arrives on `check_results`, so the Scheduler, the REST API and
 * `nscp client` can push results into the same gearmand the checks come from
 * and an installation that had NSCA only for that can stop running it. That
 * half is in `gearman_client.hpp`; here it is one `client::configuration` and
 * the three entry points the core routes through it.
 */
class GearmanClient : public nscapi::impl::simple_plugin {
 public:
  GearmanClient();
  virtual ~GearmanClient();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  /** A submission on the configured channel, from the Scheduler or elsewhere. */
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);
  /** `nscp client --module GearmanClient ... submit_gearman`. */
  bool commandLineExec(int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response);
  /** `submit_gearman` reached as a query, which is how REST and NRPE get to it. */
  void query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message);
  /** What the worker pool has done so far; see `gearman::worker_counters`. */
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

 private:
  /** What the worker section of the settings turned out to say. */
  enum class worker_setup {
    /** Nothing configured. A passive-only deployment, not a mistake. */
    disabled,
    /** Configured but unusable; the reason has been logged as an error. */
    invalid,
    /** `config` is filled in and the pool can be started from it. */
    ready
  };

  /**
   * Read the settings into a worker configuration.
   *
   * `invalid`, having logged why, when a worker is asked for but must not be
   * started: no server, no queues, an unknown mode, or a key that would leave
   * the payloads readable by anyone who can reach gearmand.
   */
  worker_setup build_config(const std::string &alias, gearman::worker_config &config);
  /** Read the passive channel's own settings and register its channel. */
  void build_client(const std::string &alias);
  void add_target(const std::string &key, const std::string &args);
  void add_command(const std::string &key, const std::string &args);

  gearman::worker_pool pool_;

  /** The channel the Scheduler and CheckHelpers submit to; `GEARMAN` by default. */
  std::string channel_;
  /** The name results are filed under on the core, `auto` for this host's own. */
  std::string hostname_;
  client::configuration client_;
};
