// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Pure Kerberos AS-REQ probe encoding/decoding (RFC 4120), no network or
// platform code: build a minimal unauthenticated AS-REQ and classify what the
// KDC sent back. Any well-formed Kerberos answer — for an account that
// exists, a KRB-ERROR KDC_ERR_PREAUTH_REQUIRED — proves the KDC is up and
// processing requests, which is exactly what a port check cannot.
namespace kdc_probe {

typedef std::vector<unsigned char> bytes;

// DER-encode an AS-REQ for client_name@realm requesting a krbtgt/realm ticket
// (no pre-auth, aes256/aes128/rc4 etypes). An unknown principal still yields
// a KRB-ERROR, i.e. a live KDC, but the probe names one that exists (see
// choose_principal): an AS-REQ for a made-up name is logged on the DC as a
// failed ticket request, the pattern user-enumeration detections alert on.
bytes build_as_req(const std::string &realm, const std::string &client_name, unsigned long nonce);

enum class response_kind { as_rep, krb_error, invalid };

struct classification {
  response_kind kind;
  long long error_code;  // KRB-ERROR error-code; -1 when absent or not applicable

  classification() : kind(response_kind::invalid), error_code(-1) {}

  // True when the response proves a live KDC (AS-REP or any KRB-ERROR).
  bool alive() const { return kind == response_kind::as_rep || kind == response_kind::krb_error; }
  std::string describe() const;
};

// Classify a raw Kerberos response message (without the TCP length prefix).
classification classify_response(const bytes &data);

// Symbolic name for the common KRB-ERROR codes ("error <n>" otherwise).
std::string krb_error_name(long long code);

// The client principal to probe `realm` with: `explicit_principal` when given,
// else this machine's own account (`machine_account`, "HOST$") when `realm` is
// the domain it is joined to (`joined_domain`, compared case-insensitively,
// since AD reports the realm uppercase and the DNS domain as configured).
// Empty when neither applies: there is then no account known to exist in that
// realm, and the caller must ask for one rather than invent a name.
std::string choose_principal(const std::string &explicit_principal, const std::string &realm, const std::string &joined_domain,
                             const std::string &machine_account);

}  // namespace kdc_probe
