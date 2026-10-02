// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <boost/thread/mutex.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/metrics.hpp>

// Serves the Nagios NCPA HTTP API (port 5693) so Nagios Core and XI can poll
// the agent with the stock check_ncpa.py plugin and the XI NCPA wizard. The
// module measures nothing itself: it is a bridge over the metrics snapshot and
// the agent's own queries, the way the check_nt server is.
class NCPAServer : public nscapi::impl::simple_plugin {
 public:
  NCPAServer();
  virtual ~NCPAServer();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();
  void submitMetrics(const PB::Metrics::MetricsMessage &response);

 private:
  void stop_server();

  // The latest 1 Hz metrics snapshot. Kept from the start so the built-in node
  // tree (cpu, memory, interface) has data the moment it is wired up; nothing
  // reads it yet.
  boost::mutex metrics_mutex_;
  PB::Metrics::MetricsMessage metrics_;

  std::shared_ptr<Mongoose::Server> server_;
};
