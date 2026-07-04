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

#include "cert_filter.hpp"

namespace cert_filter {

using parsers::where::type_bool;
using parsers::where::type_date;
using parsers::where::type_int;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("subject", &filter_obj::get_subject, "Certificate subject (e.g. /CN=host.example.com)")
      .add_string_var("issuer", &filter_obj::get_issuer, "Certificate issuer distinguished name")
      .add_string_var("thumbprint", &filter_obj::get_thumbprint, "SHA-1 fingerprint (lower-case hex)")
      .add_string_var("serial", &filter_obj::get_serial, "Certificate serial number (hex)")
      .add_string_var("source", &filter_obj::get_source, "Where the certificate was read from (file path or store)")
      .add_string_var("store", &filter_obj::get_store, "The store/source type (file or a Windows store name)")
      .add_string_var("valid_from", &filter_obj::get_not_before_su, "Not-before date (UTC)")
      .add_string_var("valid_to", &filter_obj::get_not_after_su, "Not-after / expiry date (UTC)");

  registry_.add_int_var("expires_in", type_int, &filter_obj::get_expires_in_days, "Whole days until the certificate expires (negative if already expired)")
      .add_int_perf("d");
  registry_.add_int_var("expires_in_days", type_int, &filter_obj::get_expires_in_days, "Alias for expires_in");
  registry_.add_int_var("expires_in_sec", type_int, &filter_obj::get_expires_in_sec, "Seconds until the certificate expires (negative if already expired)");
  registry_.add_int_var("not_after", type_date, &filter_obj::get_not_after, "The not-after / expiry date");
  registry_.add_int_var("not_before", type_date, &filter_obj::get_not_before, "The not-before date");
  registry_.add_int_var("expired", type_bool, &filter_obj::get_expired, "True if the certificate has already expired");
  registry_.add_int_var("not_yet_valid", type_bool, &filter_obj::get_not_yet_valid, "True if the certificate is not yet valid (not-before is in the future)");

  registry_.add_human_string("not_after", &filter_obj::get_not_after_su, "")
      .add_human_string("not_before", &filter_obj::get_not_before_su, "");
}

}  // namespace cert_filter
