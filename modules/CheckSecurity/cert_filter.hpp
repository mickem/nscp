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

#include <memory>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

namespace cert_filter {

// One X.509 certificate, sourced either from a file on disk (cross-platform,
// parsed via OpenSSL) or from the Windows certificate store. Timestamps are
// native unix time (seconds since the epoch) so the where-engine, derived
// fields and field registration are identical on both platforms; only the
// enumeration (cert_source_openssl.cpp vs cert_source_win.cpp) differs.
struct filter_obj {
  filter_obj() : not_before(0), not_after(0), key_size(0), self_signed(0), trusted(0), weak_signature(0), weak_key(0) {}

  std::string get_subject() const { return subject; }
  std::string get_issuer() const { return issuer; }
  std::string get_thumbprint() const { return thumbprint; }
  std::string get_serial() const { return serial; }
  std::string get_source() const { return source; }
  std::string get_store() const { return store; }

  std::string get_signature_algorithm() const { return signature_algorithm; }
  std::string get_key_type() const { return key_type; }
  long long get_key_size() const { return key_size; }
  long long get_self_signed() const { return self_signed; }
  long long get_trusted() const { return trusted; }
  long long get_weak_signature() const { return weak_signature; }
  long long get_weak_key() const { return weak_key; }

  long long get_not_before() const { return not_before; }
  long long get_not_after() const { return not_after; }
  std::string get_not_before_su() const { return str::format::format_date(static_cast<std::time_t>(not_before)); }
  std::string get_not_after_su() const { return str::format::format_date(static_cast<std::time_t>(not_after)); }

  // Whole days until (positive) / since (negative) the not-after date, relative
  // to the query's evaluation time. This is the primary threshold field.
  long long get_expires_in_days() const {
    const long long now = parsers::where::constants::get_now();
    return (not_after - now) / 86400;
  }
  long long get_expires_in_sec() const { return not_after - parsers::where::constants::get_now(); }

  long long get_expired() const { return parsers::where::constants::get_now() > not_after ? 1 : 0; }
  long long get_not_yet_valid() const { return parsers::where::constants::get_now() < not_before ? 1 : 0; }

  std::string show() const { return subject; }

  std::string subject;
  std::string issuer;
  std::string thumbprint;  // lower-case hex SHA-1 fingerprint
  std::string serial;      // hex serial number
  std::string source;      // file path or store descriptor the cert came from
  std::string store;       // "file" or e.g. "LocalMachine\\My"
  long long not_before;    // unix epoch seconds
  long long not_after;     // unix epoch seconds

  std::string signature_algorithm;  // e.g. "sha256WithRSAEncryption"
  std::string key_type;             // "RSA" / "EC" / "DSA" / ...
  long long key_size;               // public key size in bits
  long long self_signed;            // 1 if issuer == subject and self-issued
  long long trusted;                // 1 if it chains to a trusted CA (time ignored)
  long long weak_signature;         // 1 if signed with MD5/SHA-1
  long long weak_key;               // 1 if RSA/DSA < 2048 or EC < 256 bits
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace cert_filter
