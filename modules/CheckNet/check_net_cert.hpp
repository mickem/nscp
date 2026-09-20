// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>
#include <list>
#include <memory>
#include <parsers/where/node.hpp>
#include <string>
#include <vector>

// Forward declared rather than included: <net/socket/socket_helpers.hpp> drags
// boost::asio::ssl - and with it the whole OpenSSL header set - into every
// translation unit that includes check_tcp.h or check_http.h, when the only
// thing here that needs it is populate(). That one lives in check_net_cert.cpp,
// which does include it.
namespace socket_helpers {
struct peer_certificate;
}

namespace check_net {
// The peer certificate as a check reports it: the fields every TLS-capable
// check exposes as keywords, plus the name matching behind the `sans` option.
// Shared by check_tcp and check_http so the two spell a certificate the same
// way - a filter written against one works verbatim against the other.
namespace cert {

// Everything the check learned about the certificate. Every field is owned:
// `warning`/`critical` are evaluated in match_post(), long after the socket
// that produced these values has been closed.
struct cert_fields {
  // The guard for all of it. expiry_days is legitimately negative for an
  // expired certificate, so "no certificate" cannot be encoded as a value.
  bool has_certificate = false;
  // Whole days until the certificate expires. Empty when there is no
  // certificate at all, and also when one was served whose notAfter could not
  // be read - neither is a number, and reporting either as one would let an
  // expiry threshold fire on something that is not an expiry. Tell the two
  // apart with has_certificate.
  boost::optional<long long> expiry_days;
  std::string subject;
  std::string issuer;
  std::string subject_cn;
  std::string issuer_cn;
  // subjectAltName entries as `DNS:host` / `IP:addr`, comma separated - the
  // form `openssl x509 -text` prints, so an operator can paste what they see.
  std::string sans;
  bool self_signed = false;
  // OpenSSL's verdict on the chain: "ok", or the reason it did not verify.
  // Empty when the connection carried no TLS at all. Recorded even when
  // `verify` is off, which is what lets a check report an untrusted chain
  // without refusing to connect - see socket_helpers::peer_verify_result for
  // why that is NOT an authentication result.
  std::string verify_result;
  // Names asked for via `sans=` that the certificate does not carry, comma
  // separated. Empty when none were asked for, or all were present.
  std::string missing_sans;
};

// Fold a name to its comparable form: drop the `DNS:` / `IP:` prefix a SAN
// entry carries, lowercase it (DNS names are case-insensitive) and drop a
// trailing root dot.
inline std::string normalize_name(const std::string &name) {
  std::string result = name;
  if (result.compare(0, 4, "DNS:") == 0)
    result.erase(0, 4);
  else if (result.compare(0, 3, "IP:") == 0)
    result.erase(0, 3);
  boost::algorithm::to_lower(result);
  boost::algorithm::trim(result);
  if (result.size() > 1 && result[result.size() - 1] == '.') result.erase(result.size() - 1);
  return result;
}

// Whether a certificate's SAN entry covers the name the operator asked for.
//
// Wildcards follow RFC 6125: `*.example.com` covers `www.example.com` but
// neither `example.com` itself nor `a.b.example.com`, and the `*` is only ever
// the whole leftmost label. Asking for a literal `*.example.com` matches the
// wildcard entry itself, which is how you assert that a wildcard certificate
// is in fact a wildcard.
inline bool name_matches(const std::string &san_entry, const std::string &required) {
  const std::string san = normalize_name(san_entry);
  const std::string want = normalize_name(required);
  if (san.empty() || want.empty()) return false;
  if (san == want) return true;
  if (san.compare(0, 2, "*.") != 0) return false;
  // Asking for a wildcard means asking for that exact entry, already covered
  // by the equality above; it must not also match through the '*'.
  if (want.compare(0, 2, "*.") == 0) return false;
  const std::string suffix = san.substr(1);  // ".example.com"
  if (want.size() <= suffix.size()) return false;
  if (want.compare(want.size() - suffix.size(), suffix.size(), suffix) != 0) return false;
  const std::string label = want.substr(0, want.size() - suffix.size());
  // The '*' stands for exactly one label, so what it matched cannot itself
  // contain a dot.
  return !label.empty() && label.find('.') == std::string::npos;
}

// The required names the certificate does not carry, in the order asked for.
// Matching is against subjectAltName only, never the subject CN: a CN that is
// not repeated as a SAN has not been a valid identity since RFC 2818 was
// superseded, and every current browser and library ignores it.
inline std::vector<std::string> find_missing_sans(const std::list<std::string> &sans, const std::vector<std::string> &required) {
  std::vector<std::string> missing;
  for (const std::string &want : required) {
    if (want.empty()) continue;
    bool found = false;
    for (const std::string &entry : sans) {
      if (name_matches(entry, want)) {
        found = true;
        break;
      }
    }
    if (!found) missing.push_back(want);
  }
  return missing;
}

// Record that every required name is missing because there was no certificate
// to carry them. A check asked to require names and handed no certificate has
// not met the requirement - staying silent there would report ok for the one
// case the option exists to catch. Returns false whenever names were required,
// mirroring populate()'s "did the requirement hold" result.
inline bool require_without_certificate(cert_fields &out, const std::vector<std::string> &required_sans) {
  if (required_sans.empty()) return true;
  out.missing_sans = boost::algorithm::join(required_sans, ",");
  return false;
}

// Split a comma separated `sans=` value into the names to require.
inline std::vector<std::string> parse_required_sans(const std::string &value) {
  std::vector<std::string> names;
  if (value.empty()) return names;
  boost::algorithm::split(names, value, boost::algorithm::is_any_of(","));
  for (std::string &name : names) boost::algorithm::trim(name);
  names.erase(std::remove(names.begin(), names.end(), std::string()), names.end());
  return names;
}

// Fill `out` from a peer certificate read off a live session, and apply the
// `sans=` requirement. Returns false when a required name is missing, which is
// the caller's cue to fail the check's `result`.
bool populate(cert_fields &out, const socket_helpers::peer_certificate &info, const std::vector<std::string> &required_sans);

// Register the certificate keywords on a check's filter registry. Templated on
// the filter object so check_tcp and check_http share one vocabulary; `field`
// points at the cert_fields member of that object.
//
// None of these names collide with the filter engine's generic summary
// keywords, and all are prefixed `cert_` so they stay recognisable next to a
// check's own keywords.
template <typename Obj, typename Registry>
void register_keywords(Registry &registry, cert_fields Obj::*field) {
  registry.add_string_var(
      "cert_subject", [field](std::shared_ptr<Obj> o) { return ((*o).*field).subject; },
      "Subject of the peer's TLS certificate as an RFC 2253 string, e.g. CN=www.example.com,O=Acme. Empty when there is no certificate.");
  registry.add_string_var(
      "cert_issuer", [field](std::shared_ptr<Obj> o) { return ((*o).*field).issuer; },
      "Issuer of the peer's TLS certificate as an RFC 2253 string. Empty when there is no certificate.");
  registry.add_string_var(
      "cert_cn", [field](std::shared_ptr<Obj> o) { return ((*o).*field).subject_cn; },
      "commonName of the certificate subject, e.g. www.example.com. Empty for a certificate that identifies its hosts only through "
      "subjectAltName, which is normal - assert on cert_sans instead.");
  registry.add_string_var(
      "cert_issuer_cn", [field](std::shared_ptr<Obj> o) { return ((*o).*field).issuer_cn; },
      "commonName of the certificate issuer, e.g. R11. Use it to alert when a certificate was renewed by an unexpected CA.");
  registry.add_string_var(
      "cert_sans", [field](std::shared_ptr<Obj> o) { return ((*o).*field).sans; },
      "subjectAltName entries of the certificate, comma separated and in the openssl form (DNS:host, IP:addr). Use with like/regexp for "
      "ad-hoc matching, or the sans= option for a checked requirement.");
  registry.add_string_var(
      "cert_verify", [field](std::shared_ptr<Obj> o) { return ((*o).*field).verify_result; },
      "OpenSSL's verdict on the certificate chain: 'ok', or why it did not verify ('unable to get local issuer certificate', 'self signed "
      "certificate', ...). Recorded even with verify=none, so a check can report an untrusted chain without refusing to connect. Empty on a "
      "plain connection. NOT an authentication result on its own: only a successful handshake under a verifying mode is that.");
  registry.add_string_var(
      "missing_sans", [field](std::shared_ptr<Obj> o) { return ((*o).*field).missing_sans; },
      "Names given to the sans= option that the certificate does not cover, comma separated. Empty when nothing was required or everything "
      "was found.");
  registry
      .add_int_var(
          "cert_self_signed", parsers::where::type_bool,
          [field](std::shared_ptr<Obj> o) { return static_cast<long long>(((*o).*field).self_signed ? 1 : 0); },
          "True when the certificate's subject equals its issuer. An internal CA root is legitimately self-signed, so this is reported "
          "rather than judged.")
      .no_perf();
}

}  // namespace cert
}  // namespace check_net
