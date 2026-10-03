// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "Server.h"

#include <threads/guarded_thread.hpp>
#include <utility>

#ifdef NSCP_WEB_BACKEND_BEAST
#include "ServerBeastImpl.h"
#else
#include "ServerMongooseImpl.h"
#endif

// Factory indirection for the backend swap planned in
// `docs/design/beast-web-backend.md`. The default is the mongoose
// backend; defining `NSCP_WEB_BACKEND_BEAST` at compile time (wired
// by Phase 4's CMake selector) picks the Boost.Beast implementation
// instead. Both classes ship in libnscp_mongoose so this header swap
// is the only build-side knob.
Mongoose::Server* Mongoose::Server::make_server(const WebLoggerPtr& logger) {
#ifdef NSCP_WEB_BACKEND_BEAST
  return new ServerBeastImpl(logger);
#else
  return new ServerMongooseImpl(logger);
#endif
}

void Mongoose::stop_and_release(std::shared_ptr<Server> &server, const Server::thread_reporter &reporter) {
  std::shared_ptr<Server> owned;
  owned.swap(server);
  if (!owned) {
    return;
  }
  if (!owned->isServerThread()) {
    owned->stop();
    return;  // freed here, with every thread joined
  }
  // On one of its own threads: see the declaration. The releasing thread holds
  // the last reference, so the server outlives every thread it joins.
  const auto releaser = threads::start_guarded_thread(
      "web server release",
      [owned]() mutable {
        owned->stop();
        owned.reset();
      },
      reporter ? reporter : Server::thread_reporter([](const std::string &) {}));
  releaser->detach();
}
