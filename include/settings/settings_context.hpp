// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <net/net.hpp>

#include <string>

namespace settings {

// Whether a settings context names a store on this machine.
//
// A context is handed to create_instance(), which honours every protocol it
// knows - including http and https. This asks the one question where that
// matters: may settings be *written* there. nsclient.ini holds the NRPE and
// NSCA keys, the WEB password and every module's credentials, so a migration
// whose target is a remote store publishes, unredacted, exactly what a settings
// read masks - to whatever host the context names.
//
// It deliberately says nothing about a remote *source*. Reading a configuration
// in is what the MSI's ImportConfig action and `nscp settings --migrate-from
// <url>` do, and both are an operator act on a host they already administer.
// Refusing it would not remove a capability either: the settings Control that
// can ask for one is behind settings.put, and settings.put can write
// [/modules] and [/settings/external scripts] and reload. What does matter for a
// source is the transport, and that is settled in settings_http's
// cache_remote_file - anything but https is skipped unless boot.ini opts in with
// `[tls] allow plaintext`, with the same [tls] section deciding how the peer is
// verified (notice 280).
//
// Shared by the two places that check a write target - settings_handler_impl's
// migrate_to and the settings Control SAVE request - so the rule cannot drift
// between them.
inline bool is_local_context(const std::string &context) {
  const net::url url = net::parse(context);
  return url.protocol != "http" && url.protocol != "https";
}

}  // namespace settings
