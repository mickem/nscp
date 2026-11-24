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
// Does nothing when both files exist and belong together. A pair that cannot
// be used - a certificate that does not parse, or is for another key - is
// replaced as well, since nothing else would ever get the agent past it; the
// return value says so (true: an existing pair was unusable and replaced), so
// the caller can tell the operator the agent has to be signed again. Throws
// std::runtime_error.
bool ensure_bootstrap_certificate(const pki_paths &paths, const std::string &cn);

// Store what the master sent in pki::UpdateCertificate, after checking that
// it is what the agent asked for: a certificate for the agent's own key, with
// `cn` as its CN, issued by the CA that came with it (or, when none came, by
// the CA already stored). Throws std::runtime_error, with nothing replaced,
// when it is not. Both files are written beside their targets and renamed
// into place, so a failure part-way never leaves a truncated certificate.
void store_signed_certificate(const pki_paths &paths, const std::string &cn, const std::string &cert_pem, const std::string &ca_pem);

// Whether an identity made elsewhere can be this agent's: the certificate is
// for `cn` and for that key, signed, and issued by the CA. Throws
// std::runtime_error saying which of those it is not. Touches no file.
void check_identity(const std::string &cn, const std::string &cert_pem, const std::string &key_pem, const std::string &ca_pem);

// Install an identity made elsewhere - the key, certificate and CA of an
// Icinga 2 agent - as this agent's, after checking that it can be used: the
// certificate is for `cn` and for that key, and the CA issued it. Throws
// std::runtime_error with nothing written when it cannot (check_identity()).
// The three files replace what is there together or not at all. The key is
// written private to the account NSClient++ runs as, whatever the directory
// allows.
void import_identity(const pki_paths &paths, const std::string &cn, const std::string &cert_pem, const std::string &key_pem, const std::string &ca_pem);

// True when `certificate` exists and is not self-signed (issuer != subject).
bool is_signed(const std::string &certificate);

}  // namespace icinga_agent
