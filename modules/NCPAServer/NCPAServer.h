// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>

// Serves the Nagios NCPA HTTP API (port 5693) so Nagios Core and XI can poll
// the agent with the stock check_ncpa.py plugin and the XI NCPA wizard. The
// module measures nothing itself: it is a bridge over the agent's own queries,
// the way the check_nt server is.
class NCPAServer : public nscapi::impl::simple_plugin {
 public:
  NCPAServer();
  virtual ~NCPAServer();

  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();

 private:
  void stop_server();

  // Re-resolves the host names in `allowed hosts` in the background when
  // `cache allowed hosts` is off (see NCPAServer.cpp).
  struct host_refresher;
  std::shared_ptr<host_refresher> refresher_;
  std::shared_ptr<Mongoose::Server> server_;
};
