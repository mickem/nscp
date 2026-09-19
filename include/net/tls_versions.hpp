// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <openssl/ssl.h>

#include <string>

// The one table of `tls version` spellings.
//
// Header-only and dependency-free on purpose: the socket layer
// (socket_helpers) and the beast web backend both have to understand the
// setting, and the web backend deliberately does not link the socket layer.
// Before this was shared, the web backend carried its own copy and the two had
// already drifted - `sslv3` resolved to SSL 3.0 exactly on one side and to the
// "any" method on the other. Whatever the vocabulary grows next, it grows
// here, once.
namespace tls_versions {

// `spec` is the setting lower-cased and trimmed, with any trailing '+'
// already stripped. "any" is not handled here because it means different
// things to a floor and to a ceiling; each caller spells that out.
inline bool lookup(const std::string &spec, long &version) {
  if (spec == "tlsv1.3" || spec == "tls1.3" || spec == "1.3") {
    version = TLS1_3_VERSION;
  } else if (spec == "tlsv1.2" || spec == "tls1.2" || spec == "1.2") {
    version = TLS1_2_VERSION;
  } else if (spec == "tlsv1.1" || spec == "tls1.1" || spec == "1.1") {
    version = TLS1_1_VERSION;
  } else if (spec == "tlsv1.0" || spec == "tls1.0" || spec == "1.0") {
    version = TLS1_VERSION;
  } else if (spec == "sslv3" || spec == "ssl3") {
    version = SSL3_VERSION;
  } else {
    return false;
  }
  return true;
}

}  // namespace tls_versions
