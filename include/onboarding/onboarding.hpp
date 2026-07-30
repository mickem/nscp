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

  // TLS settings for the public HTTPS enrollment call.
  std::string tls_version = "tlsv1.2+";
  std::string verify_mode;  // empty: "certificate" when a ca is given, else "none"
  std::string ca;           // optional CA bundle used to verify the server

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
// cannot be read or parsed.
boost::optional<enrolled_identity> load_state(const std::string &path);

}  // namespace onboarding
