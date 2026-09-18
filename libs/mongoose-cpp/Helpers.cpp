// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "Helpers.h"

#include <bytes/base64.h>
#include <bytes/base64.hpp>

#include "Response.h"

#include <string>

namespace Mongoose {

std::string Helpers::encode_b64(const std::string &str) { return bytes::base64_encode(str); }

std::string Helpers::decode_b64(const std::string &str) {
  if (str.empty()) return {};
  const std::size_t needed = b64::b64_decode(str.data(), str.size(), nullptr, 0);
  // needed == 0 means invalid base64 (e.g. length not a multiple of 4). Bail
  // out before sizing `out` so we never take `&out[0]` on an empty string.
  if (needed == 0) return {};
  std::string out(needed, '\0');
  const std::size_t written = b64::b64_decode(str.data(), str.size(), &out[0], needed);
  out.resize(written);
  return out;
}


// The policy. Chosen for what the bundled web UI actually needs and nothing
// wider: everything it loads comes from its own origin, MUI injects styles at
// runtime so style-src has to allow inline, and icons and fonts arrive as
// data: URIs. frame-ancestors 'none' is the one that matters most here - it
// is what X-Frame-Options says, in the header modern browsers actually read.
const char *const kContentSecurityPolicy =
    "default-src 'self'; frame-ancestors 'none'; base-uri 'none'; object-src 'none'; "
    "style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:; connect-src 'self'";

void Helpers::add_security_headers(Response &response, const bool is_tls) {
  // hasHeader, so a controller that deliberately set its own policy (a future
  // embeddable view, say) keeps it.
  if (!response.hasHeader("Content-Security-Policy")) response.setHeader("Content-Security-Policy", kContentSecurityPolicy);
  if (!response.hasHeader("X-Frame-Options")) response.setHeader("X-Frame-Options", "DENY");
  if (!response.hasHeader("X-Content-Type-Options")) response.setHeader("X-Content-Type-Options", "nosniff");
  if (!response.hasHeader("Referrer-Policy")) response.setHeader("Referrer-Policy", "no-referrer");
  // Only over TLS: sent over cleartext it is ignored by browsers anyway, and
  // the agent supports a deliberate cleartext mode behind a proxy.
  if (is_tls && !response.hasHeader("Strict-Transport-Security")) response.setHeader("Strict-Transport-Security", "max-age=31536000");
}

}  // namespace Mongoose
