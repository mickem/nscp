// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "cert_loader.h"

#include <fstream>
#include <nsclient/nsclient_exception.hpp>
#include <sstream>

namespace Mongoose {
namespace cert_loader {

std::string load_file(const std::string& path, const std::string& hint) {
  // A default std::ifstream doesn't throw, so a missing/unreadable file would
  // otherwise yield an empty string silently — making a TLS misconfiguration
  // look like an empty certificate much later. Detect open/read failures and
  // report them so the cause is obvious.
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw nsclient::nsclient_exception("Failed to open " + hint + " file: " + path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    throw nsclient::nsclient_exception("Failed to read " + hint + " file: " + path);
  }
  return buffer.str();
}

std::pair<std::string, std::string> load_certificates(const std::string& cert_path, const std::string& key_path) {
  auto cert = load_file(cert_path, "certificate");
  if (key_path.empty()) {
    return {cert, cert};
  }
  auto key = load_file(key_path, "private key");
  return {cert, key};
}

}  // namespace cert_loader
}  // namespace Mongoose
