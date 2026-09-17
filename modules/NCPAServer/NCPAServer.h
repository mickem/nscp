// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <memory>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

#include "ncpa_controller.hpp"
#include "ncpa_sources.hpp"

// The NCPA server: an HTTPS listener on port 5693 that answers the Nagios
// Cross-Platform Agent API, so Nagios Core and Nagios XI can poll this agent
// with the stock check_ncpa.py plugin and the XI NCPA wizard, unmodified.
//
// It is a bridge and measures nothing itself: the built-in nodes are rendered
// from the 1 Hz metrics snapshot every NSClient++ collector already publishes,
// and `plugins/` hands a request straight to the agent's own query dispatch.
class NCPAServer : public nscapi::impl::simple_plugin {
 public:
  NCPAServer();
  virtual ~NCPAServer();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();

  // The 1 Hz snapshot every built-in node reads. Arrives on the metrics thread.
  void submitMetrics(const PB::Metrics::MetricsMessage &message) const;

 private:
  // Settings and auth state, shared with the controller so a reload rewrites
  // them in place rather than restarting the listener.
  ncpa::session_ptr session_;
  std::shared_ptr<ncpa::metrics_snapshot> snapshot_;
  // Previous samples for `delta`. Owned by the module rather than by a request
  // so two consecutive polls can be differenced at all.
  std::shared_ptr<ncpa::delta_store> deltas_;
  std::shared_ptr<ncpa::query_dispatcher> dispatcher_;
  std::shared_ptr<Mongoose::Server> server_;
};
