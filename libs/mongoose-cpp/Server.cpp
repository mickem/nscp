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

std::shared_ptr<boost::thread> Mongoose::stop_and_release(std::shared_ptr<Server> &server, const Server::thread_reporter &reporter) {
  std::shared_ptr<Server> owned;
  owned.swap(server);
  if (!owned) {
    return nullptr;
  }
  if (!owned->isServerThread()) {
    owned->stop();
    return nullptr;  // freed here, with every thread joined
  }
  // On one of its own threads: see the declaration. The releasing thread holds
  // the last reference, so the server outlives every thread it joins. Through
  // one shared holder: the thread keeps copies of its body for as long as the
  // thread object lives, and a reference in each copy would keep the server
  // (and the caller's controllers) alive until the caller joined.
  const auto holder = std::make_shared<std::shared_ptr<Server>>(std::move(owned));
  return threads::start_guarded_thread(
      "web server release",
      [holder]() {
        (*holder)->stop();
        holder->reset();
      },
      reporter ? reporter : Server::thread_reporter([](const std::string &) {}));
}
