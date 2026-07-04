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
#include <vector>

#include "cert_filter.hpp"

namespace cert_source {

// Parse a single DER-encoded certificate into a filter object. Implemented once
// (OpenSSL, cross-platform) in cert_source_openssl.cpp and reused by both the
// file source and the Windows store source. Returns nullptr and sets `error` on
// a parse failure.
cert_filter::filter_obj_ptr parse_der(const unsigned char *der, size_t len, const std::string &source, const std::string &store, std::string &error);

// Load every certificate found in the given files (PEM may hold several) or,
// when a path is a directory, in the files it contains (recursively when
// `recurse`). Cross-platform (OpenSSL). Appends to `out`; per-file problems are
// appended to `errors` but do not abort the scan.
void load_files(const std::vector<std::string> &paths, bool recurse, std::vector<cert_filter::filter_obj_ptr> &out, std::vector<std::string> &errors);

#ifdef WIN32
// Enumerate the certificates in a Windows system store (e.g. "My", "Root") at
// the given location ("LocalMachine" or "CurrentUser"). Windows only.
void load_store(const std::string &store, const std::string &location, std::vector<cert_filter::filter_obj_ptr> &out, std::string &error);
#endif

}  // namespace cert_source
