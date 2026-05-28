/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
