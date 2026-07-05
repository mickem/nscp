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

// A raw DER-encoded certificate tagged with where it came from. This is the
// common currency between the platform sources: the Windows store source
// (crypt32) collects DER blobs and hands them here, keeping all OpenSSL logic in
// cert_source_openssl.cpp.
struct der_cert {
  std::vector<unsigned char> der;
  std::string source;
  std::string store;
};

// Parse a batch of DER certificates, fill every field (including the security
// fields and trust), and append the resulting filter objects to `out`. Trust is
// evaluated against `ca_file` (a CA bundle) or the system default store when it
// is empty, using the whole batch as candidate intermediates. Cross-platform
// (OpenSSL). Per-cert parse failures are appended to `errors`.
void finalize_der_batch(const std::vector<der_cert> &ders, const std::string &ca_file, std::vector<cert_filter::filter_obj_ptr> &out,
                        std::vector<std::string> &errors);

// Load every certificate found in the given files (PEM may hold several, PKCS#12
// .pfx is supported with `password`) or, when a path is a directory, in the
// files it contains (recursively when `recurse`). Cross-platform (OpenSSL).
// Appends to `out`; per-file problems are appended to `errors` but do not abort
// the scan.
void load_files(const std::vector<std::string> &paths, bool recurse, const std::string &ca_file, const std::string &password,
                std::vector<cert_filter::filter_obj_ptr> &out, std::vector<std::string> &errors);

#ifdef WIN32
// Enumerate the certificates in a Windows system store (e.g. "My", "Root") at
// the given location ("LocalMachine" or "CurrentUser"). Windows only.
void load_store(const std::string &store, const std::string &location, const std::string &ca_file, std::vector<cert_filter::filter_obj_ptr> &out,
                std::string &error);
#endif

}  // namespace cert_source
