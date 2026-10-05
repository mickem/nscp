// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <boost/thread/thread.hpp>
#include <list>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>

struct ncpa_auth_state;

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

  std::shared_ptr<Mongoose::Server> server_;
  // The token rate limiter. Created once and handed to every controller, so a
  // settings reload does not reset it.
  std::shared_ptr<ncpa_auth_state> auth_;
  // Threads freeing a server released from one of its own threads: they run
  // the controller's destructor, so the destructor waits for them.
  std::list<std::shared_ptr<boost::thread>> retired_;
};
