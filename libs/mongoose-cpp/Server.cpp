// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "Server.h"

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
