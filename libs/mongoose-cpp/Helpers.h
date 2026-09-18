// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

#include "dll_defines.hpp"

/**
 * A stream response to a request
 */
namespace Mongoose {
class Response;

struct NSCP_MONGOOSE_EXPORT Helpers {
  static std::string encode_b64(const std::string &str);
  static std::string decode_b64(const std::string &str);

  // Add the browser-facing hardening headers to a response, unless the
  // controller already set one of them.
  //
  // The agent serves an admin UI that holds a session token, so a page able to
  // frame it gets an authenticated UI to click-jack, and any future injection
  // has nothing standing in its way. These four headers cost nothing and are
  // what a browser needs to refuse both. Strict-Transport-Security is only
  // emitted over TLS, where it is meaningful and where it cannot strand an
  // operator who is deliberately running cleartext behind a proxy.
  //
  // Applied by both backends just before the response is written, so it covers
  // static files, API answers and error pages alike.
  static void add_security_headers(Response &response, bool is_tls);
};
}  // namespace Mongoose
