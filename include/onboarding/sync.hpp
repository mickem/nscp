// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <map>
#include <onboarding/onboarding.hpp>
#include <string>
#include <vector>

// Wire-level and pure-logic pieces of the post-enrollment agent sync loop:
// desired-state parsing, bundle verification (SHA-256 + Ed25519), RFC 7396
// JSON merge patch, the deterministic JSON -> NSClient INI rendering, and
// certificate renewal helpers. Everything here is side-effect free so it can
// be unit tested; the core service's fleet sync loop (service/fleet_sync.cpp)
// owns transport, threading and filesystem staging.
namespace onboarding {

// One bundle entry from a desired-state response.
struct bundle_info {
  std::string id;
  std::string name;
  std::string version;
  std::string sha256;     // hex digest of the bundle bytes
  std::string signature;  // base64 Ed25519 signature over the 32-byte digest
  std::string url;        // server-relative download path
  long long priority = 0;
};

struct desired_state {
  std::string state_hash;
  unsigned long next_poll_in_seconds = 60;
  std::string merged_config_json;  // serialized JSON object; "{}" when empty
  std::vector<bundle_info> bundles;  // sorted by ascending priority (apply order)
};

// Parse a 200 desired-state response body. Throws onboarding_error
// (non-retryable) when required fields are missing or malformed.
//
// Every server-supplied field is validated, not just copied - the response is
// only as trustworthy as the (pinned, mutually authenticated) channel it came
// over, and these values are used in places where a hostile string would do
// real damage:
//   state_hash  a token (<=128 chars, alphanumerics and -._:=+/~) because it
//               is sent back in a query string
//   id          a single harmless path component (no separators, no "..")
//               because it becomes a bundle cache file name
//   sha256      exactly 64 hex characters
//   signature   base64 (<=512 chars)
//   url         a server-relative path ("/..."), no control characters,
//               spaces or '#', because it is appended to the pinned base url
// next_poll_in_seconds is clamped to 1..86400 (nonsense values fall back to
// the 60s default) so a bad response cannot park the agent forever, and a
// bundles field that is present but not an array is an error rather than
// "no bundles" - the latter would apply an empty configuration.
desired_state parse_desired_state(const std::string &body);

// Parse the next_poll_in_seconds hint from a 304 (or 200) body; none when
// absent, not a number, or not a usable interval (<= 0). Clamped to 86400.
boost::optional<unsigned long> parse_next_poll(const std::string &body);

// (parse_retry_after lives in onboarding.hpp: the sync loop and the installer's
// enrollment path share it, and the installer links only the latter.)

// Hex-encoded SHA-256 of a byte buffer.
std::string sha256_hex(const std::string &bytes);

// Verify a bundle: `signature_b64` must be an Ed25519 signature over the
// 32-byte SHA-256 digest of `bytes` (not the raw bytes), made by the key in
// `pub_pem`. Returns false and sets `error` on any mismatch or parse failure.
bool verify_bundle(const std::string &pub_pem, const std::string &bytes, const std::string &sha256_hex_expected, const std::string &signature_b64,
                   std::string &error);

// RFC 7396 JSON Merge Patch: objects deep-merge, scalars/arrays replace
// wholesale, null deletes the key. Returns the patched value.
boost::json::value json_merge_patch(const boost::json::value &target, const boost::json::value &patch);

// Render a merged JSON config to NSClient INI text, deterministically:
// nested objects become sections named by their /-joined path, scalar and
// array members become key=value lines (arrays as comma-separated lists),
// sections and keys are sorted. Nulls are skipped (they only exist pre-merge).
std::string render_ini(const boost::json::value &config);

// --- state report / metrics payloads -----------------------------------------

struct installed_bundle {
  std::string id;
  std::string version;
};

// Build a /agent/v1/state-report body. Pass none as `applied_state_hash`
// after a failed apply (the server then only refreshes last_seen_at).
std::string build_state_report(const boost::optional<std::string> &applied_state_hash, const std::vector<installed_bundle> &bundles_installed,
                               const std::vector<std::string> &errors, const std::map<std::string, std::string> &reported_tags);

struct metric_sample {
  std::string key;
  double value = 0;
  boost::optional<long long> ts;  // unix seconds; only for buffered samples
};

// Build a /agent/v1/metrics body. Samples whose value is not finite (NaN,
// infinity) are dropped: they have no JSON representation, and including one
// would make the server reject the whole batch.
std::string build_metrics(const std::vector<metric_sample> &samples);

// --- transport error classification ------------------------------------------

enum class transport_error_kind {
  network,           // cannot reach the server (resolve/connect/timeout): transient
  tls_identity,      // server rejected (or never received) OUR client certificate
  tls_server_trust,  // the server's certificate does not match the pinned one
  tls_other,         // some other TLS handshake failure
  unknown
};

struct transport_error_info {
  transport_error_kind kind = transport_error_kind::unknown;
  std::string advice;  // operator guidance to append to the log; empty when none
};

// Classify a transport/TLS failure message (as produced by the HTTP client /
// OpenSSL) so callers can log actionable guidance instead of a raw alert, and
// distinguish "server unreachable" from "our identity was rejected".
transport_error_info classify_transport_error(const std::string &message);

// --- certificate lifecycle ----------------------------------------------------

// Days until the certificate expires (negative when already expired).
// Throws onboarding_error (non-retryable) when the PEM cannot be parsed.
long days_until_expiry(const std::string &cert_pem);

// Apply a /agent/v1/renew response: new certificate material + the freshly
// generated key, keeping server_url/mtls_url from the current identity.
// Throws onboarding_error (non-retryable) on malformed/incomplete responses.
enrolled_identity parse_renew_response(const std::string &body, const identity &fresh_identity, const enrolled_identity &current);

}  // namespace onboarding
