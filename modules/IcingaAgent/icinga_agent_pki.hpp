// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace icinga_agent {

// The files an agent keeps, named the way the Icinga 2 agent names them
// (/var/lib/icinga2/certs/<cn>.crt, <cn>.key, ca.crt).
struct pki_paths {
  std::string certificate;
  std::string key;
  std::string ca;
  static pki_paths for_endpoint(const std::string &directory, const std::string &cn);
};

// Generate a 4096 bit RSA key and a self-signed certificate for `cn`, the way
// Icinga for Windows bootstraps: the master signs the public key of whatever
// certificate the agent connects with when it answers pki::RequestCertificate.
// Does nothing when both files exist. Throws std::runtime_error.
void ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn);

// Store what the master sent in pki::UpdateCertificate.
void store_signed_certificate(const pki_paths &paths, const std::string &cert_pem, const std::string &ca_pem);

// True when `certificate` exists and is not self-signed (issuer != subject).
bool is_signed(const std::string &certificate);

// Subject CN of a PEM certificate file, empty when unreadable.
std::string certificate_cn(const std::string &certificate);

}  // namespace icinga_agent
