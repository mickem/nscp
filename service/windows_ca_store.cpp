// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "windows_ca_store.hpp"

#ifdef WIN32

// clang-format off
#include <windows.h>
// wincrypt.h must follow windows.h
#include <wincrypt.h>
// clang-format on

#include <bytes/base64.hpp>
#include <fstream>

namespace nsclient {
namespace windows_ca {

namespace {

// Wrap a base64 string at 64 characters per line, the convention for PEM.
std::string wrap_pem(const std::string& b64) {
  std::string out;
  out.reserve(b64.size() + b64.size() / 64 + 1);
  for (std::size_t i = 0; i < b64.size(); i += 64) {
    out.append(b64, i, 64);
    out.append("\r\n");
  }
  return out;
}

std::string der_to_pem(const unsigned char* der, std::size_t len) {
  const std::string b64 = bytes::base64_encode(der, len);
  return "-----BEGIN CERTIFICATE-----\r\n" + wrap_pem(b64) + "-----END CERTIFICATE-----\r\n";
}

}  // namespace

unsigned int export_root_store(const std::string& path, std::list<std::string>& errors) {
  const HCERTSTORE store = CertOpenSystemStoreA(NULL, "ROOT");
  if (store == nullptr) {
    errors.emplace_back("Failed to open Windows ROOT certificate store: GetLastError=" + std::to_string(::GetLastError()));
    return 0;
  }

  std::ofstream os(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!os) {
    CertCloseStore(store, 0);
    errors.emplace_back("Failed to open " + path + " for writing");
    return 0;
  }

  unsigned int count = 0;
  PCCERT_CONTEXT cert = nullptr;
  while ((cert = CertEnumCertificatesInStore(store, cert)) != nullptr) {
    if (cert->pbCertEncoded == nullptr || cert->cbCertEncoded == 0) continue;
    const std::string pem = der_to_pem(cert->pbCertEncoded, cert->cbCertEncoded);
    os.write(pem.data(), static_cast<std::streamsize>(pem.size()));
    if (!os) {
      errors.emplace_back("Failed writing to " + path + " after " + std::to_string(count) + " certificates");
      CertFreeCertificateContext(cert);
      break;
    }
    ++count;
  }

  // CertEnumCertificatesInStore frees the previous context; we only need to
  // close the store after the loop.
  CertCloseStore(store, 0);
  return count;
}

}  // namespace windows_ca
}  // namespace nsclient

#endif  // WIN32
