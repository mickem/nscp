// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <functional>
#include <net/http/http_response.hpp>
#include <stdexcept>
#include <string>

// One-time onboarding (enrollment) of this host against an NSClient fleet
// server. The flow is: generate an Ed25519 keypair, build a PKCS#10 CSR from
// it, POST the CSR together with the one-time bootstrap token to
// {server_url}/enroll/v1 over public HTTPS, and persist the returned
// certificate material plus the local private key to durable storage. All
// later agent traffic uses that material over mTLS.
//
// The code is shared between the installer (enroll during install) and the
// core client (enroll on first boot / via command line), so it depends only
// on OpenSSL, Boost and the bundled HTTP client - no plugin API.
namespace onboarding {

// ALPN protocol offered on every mTLS call to the fleet server (its
// `fleet_proto::AGENT_ALPN`), and not optional.
//
// `mtls_url` normally points at port 443, shared with the operator web UI, and
// the server reads ALPN out of the ClientHello to decide which TLS
// configuration answers: this name gets the pinned certificate and a
// client-certificate request, anything else gets the public web certificate and
// no client-cert request. Omitting it does not fail at connect - the TCP
// connection comes up, and the handshake then fails pin validation with a
// misleading "untrusted certificate".
//
// Offer it first with "http/1.1" behind it, which is what a deployment running
// a dedicated mTLS port advertises. It applies to the agent API only: enrolment
// goes to the public endpoint, which must be reached through the ordinary web
// branch and so offers no ALPN of its own.
constexpr const char *kAgentAlpn = "nsclient-fleet/1";

// Thrown for all onboarding failures. `retryable()` distinguishes transient
// conditions (rate limit, server error, network) from fatal ones - most
// importantly a rejected bootstrap token, which is burned server-side on
// first use and can never succeed on retry.
class onboarding_error : public std::runtime_error {
 public:
  onboarding_error(const std::string &message, const bool retryable) : std::runtime_error(message), retryable_(retryable) {}
  bool retryable() const { return retryable_; }

 private:
  bool retryable_;
};

// A freshly generated agent identity: an Ed25519 private key (PKCS#8 PEM) and
// a PKCS#10 CSR signed with it. The server sets the real identity in the
// issued certificate from the bootstrap token's claims, so the CN is
// cosmetic.
struct identity {
  std::string private_key_pem;
  std::string csr_pem;
};

// Generate a new Ed25519 keypair and a CSR for it. Throws onboarding_error
// (non-retryable) on OpenSSL failures.
identity generate_identity(const std::string &cn = "client");

struct enrollment_request {
  std::string server_url;       // public API base, e.g. https://fleet.example.com
  std::string bootstrap_token;  // one-time token from the install command
  std::string hostname;         // optional; defaults to the local host name
  std::string os;               // optional; defaults to windows/macos/linux

  // TLS settings for the public HTTPS enrollment call. This is the exchange
  // that decides who the fleet server is, so it verifies by default: an
  // unverified enrollment lets an on-path attacker substitute both trust
  // anchors (the mTLS pin and the bundle signing key) for the life of the
  // agent. Pass verify_mode = "none" explicitly to opt out.
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode;  // empty: "certificate"
  std::string ca;           // CA bundle used to verify the server

  unsigned int max_attempts = 3;  // attempts for retryable failures (429/5xx/network)
};

// Everything a successful enrollment returns plus the locally generated
// private key: the durable identity the agent needs for all subsequent mTLS
// calls. Treat as secret material.
struct enrolled_identity {
  std::string private_key_pem;         // local Ed25519 key matching cert_pem
  std::string cert_pem;                // client cert signed by the tenant CA
  std::string ca_pem;                  // tenant CA cert
  std::string bundle_signing_pub_pem;  // Ed25519 public key for bundle signatures
  std::string server_url;              // public API base
  std::string mtls_url;                // base URL for all /agent/v1/* calls
  std::string mtls_server_cert_pem;    // cert to pin when connecting to mtls_url
};

// Parse a Retry-After header value (429/503) as RFC 9110 delay-seconds; none
// for the HTTP-date form and for anything else we cannot act on, in which case
// the caller falls back to its own backoff. Deliberately strict: strtoul-style
// parsing accepts "-5" (which wraps to a near-eternal wait) and reads "1e9" as
// 1, both of which a server can send by accident. Lives here rather than in
// sync.hpp because the installer links only the enrollment sources.
boost::optional<unsigned long> parse_retry_after(const std::string &header_value);

// Seams for testing: how to POST a JSON payload and how to sleep between
// retries. The default implementations use net/http/client.hpp and a real
// sleep.
typedef std::function<http::response(const std::string &url, const std::string &payload)> post_function;
typedef std::function<void(unsigned long milliseconds)> sleep_function;

// One-shot onboarding: generate an identity, enroll it and return the
// combined material. Throws onboarding_error; check retryable() to decide
// whether asking the user for a new install command is required.
enrolled_identity enroll(const enrollment_request &request);

// As above but with an already generated identity and injectable transport /
// sleep (used by tests).
enrolled_identity enroll(const enrollment_request &request, const identity &id, const post_function &post, const sleep_function &sleep_ms);

// Parse a successful /enroll/v1 response body. `fallback_server_url` is used
// when the response omits server_url. Throws onboarding_error (non-retryable)
// on malformed or incomplete responses.
enrolled_identity parse_enroll_response(const std::string &body, const identity &id, const std::string &fallback_server_url);

// Durably persist the enrolled identity as JSON: written to a temp file
// (0600 on POSIX), fsync'd and atomically renamed over `path`, so a crash
// mid-write never destroys an existing state file.
void save_state(const enrolled_identity &state, const std::string &path);

// Load a previously saved state file. Returns none when the file does not
// exist (not yet enrolled); throws onboarding_error when the file exists but
// cannot be read or parsed. Every field carrying identity (keys, certificates,
// mtls_url) must be present AND non-empty - a truncated state file is a fatal
// error, not a half-usable identity.
boost::optional<enrolled_identity> load_state(const std::string &path);

}  // namespace onboarding
