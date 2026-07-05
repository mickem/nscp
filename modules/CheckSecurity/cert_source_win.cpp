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

// Windows certificate-store enumeration. The actual certificate parsing is
// delegated to cert_source::parse_der (OpenSSL) so there is one cert-parsing
// code path shared with the file source.

#include "cert_source.hpp"

#include <Windows.h>
#include <wincrypt.h>

#include <str/utf8.hpp>

namespace cert_source {

void load_store(const std::string &store, const std::string &location, const std::string &ca_file, std::vector<cert_filter::filter_obj_ptr> &out,
                std::string &error) {
  DWORD flags = CERT_SYSTEM_STORE_LOCAL_MACHINE;
  std::string loc = location.empty() ? "LocalMachine" : location;
  if (loc == "CurrentUser" || loc == "currentuser") {
    flags = CERT_SYSTEM_STORE_CURRENT_USER;
  } else if (loc == "LocalMachine" || loc == "localmachine") {
    flags = CERT_SYSTEM_STORE_LOCAL_MACHINE;
  } else {
    error = "Unknown certificate location: " + location + " (expected LocalMachine or CurrentUser)";
    return;
  }

  const std::wstring wstore = utf8::cvt<std::wstring>(store.empty() ? "My" : store);
  HCERTSTORE hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, flags | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, wstore.c_str());
  if (hStore == nullptr) {
    error = "Failed to open certificate store '" + loc + "\\" + store + "'";
    return;
  }

  // Collect raw DER blobs and let the shared OpenSSL path parse + evaluate trust,
  // so this file stays crypt32-only and there is one cert-parsing code path.
  const std::string descriptor = loc + "\\" + (store.empty() ? "My" : store);
  std::vector<der_cert> ders;
  PCCERT_CONTEXT pCtx = nullptr;
  while ((pCtx = CertEnumCertificatesInStore(hStore, pCtx)) != nullptr) {
    der_cert d;
    d.der.assign(pCtx->pbCertEncoded, pCtx->pbCertEncoded + pCtx->cbCertEncoded);
    d.source = descriptor;
    d.store = descriptor;
    ders.push_back(std::move(d));
  }
  CertCloseStore(hStore, 0);

  std::vector<std::string> errors;
  finalize_der_batch(ders, ca_file, out, errors);
}

}  // namespace cert_source
