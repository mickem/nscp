// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <string>

namespace net {

// The stock `tls version` of the HTTP listeners (WEBServer, NCPAServer). Named
// because two places have to agree on it: the setting's registered default,
// and the test in apply_tls_options() for whether the operator chose a value.
constexpr const char *kDefaultWebTlsVersion = "1.2+";

// Hand `tls version` and `allowed ciphers` to the server. An untouched
// `tls version` goes in as empty rather than as its default: the mongoose
// backend cannot honour the setting and logs that it is ignoring it, which on
// a stock agent would mean an error on every start and reload about a setting
// nobody wrote. The beast backend applies the same default itself when handed
// an empty string.
inline void apply_tls_options(Mongoose::Server &server, const std::string &tls_version, const std::string &allowed_ciphers) {
  server.setTlsOptions(tls_version == kDefaultWebTlsVersion ? std::string() : tls_version, allowed_ciphers);
}

}  // namespace net
