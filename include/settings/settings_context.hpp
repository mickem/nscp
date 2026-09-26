// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/net.hpp>

#include <string>

namespace settings {

// Whether a settings context names a store on this machine.
//
// A context is handed to create_instance(), which honours every protocol it
// knows - including http and https. Migration copies one store into another:
// with a remote source it pulls this agent's entire configuration ([/modules],
// external script definitions, the lot) from whatever host the context names,
// and with a remote target it pushes the local configuration, credentials
// included, to one. Neither is an operation between the stores on this machine,
// which is what migrating is.
//
// A remote settings source is a deliberate operator decision that belongs in
// boot.ini, where notice 280 already requires it to be https and the [tls]
// section governs how the peer is verified.
//
// Shared by every entry point that takes a context from a caller - the
// settings Control LOAD/SAVE request and the `--migrate-from` / `--migrate-to`
// CLI - so the rule cannot drift between them.
inline bool is_local_context(const std::string &context) {
  const net::url url = net::parse(context);
  return url.protocol != "http" && url.protocol != "https";
}

}  // namespace settings
