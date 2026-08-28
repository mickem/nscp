// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

// Internal helpers for web_installer.cpp pulled out of the anonymous namespace
// so they can be unit-tested. Not part of the module's public API — only
// web_installer.cpp and the test TU should include this.

namespace nsclient {
namespace web {
namespace detail {

struct parsed_url {
  std::string protocol;
  std::string host;
  std::string port;
  std::string path;
};

// Bare-bones "https://host[:port]/path" parser. Returns false on anything
// that isn't an absolute http/https URL with a non-empty host.
bool parse_url(const std::string& url, parsed_url& out);

// Resolve a redirect Location against the URL it was served from.
//
// Handles the three shapes a Location can take: absolute ("https://h/p"),
// protocol-relative ("//h/p", which inherits the current scheme per RFC 3986
// 4.2) and path-relative ("/p" or "p"). Refuses an HTTPS->HTTP downgrade: the
// bundle's integrity rests on the TLS channel, so a chain that began on https
// must not silently drop to cleartext.
//
// Returns true and fills `next` on success; returns false and fills `error`
// when the redirect is refused.
bool resolve_redirect(const parsed_url& current, std::string location, std::string& next, std::string& error);

// Parse a "<hex> <filename>" sha256 manifest (sha256sum/shasum output).
// Returns the lower-cased 64-char hex digest, or empty on malformed input.
std::string parse_sha256_manifest(const std::string& contents);

// Return true if a zip entry name could escape the extraction root. Rejects
// any path segment equal to "..", any leading '/' or '\' (absolute paths),
// and Windows drive-letter prefixes ("C:..."). Splits on both '/' and '\'
// because zips occasionally carry backslashes and boost::filesystem on
// Windows treats both as separators.
bool is_unsafe_zip_path(const std::string& filename);

}  // namespace detail
}  // namespace web
}  // namespace nsclient
