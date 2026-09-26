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
  std::string signature;  // base64 Ed25519 signature over the descriptor below
  std::string url;        // server-relative download path
  long long priority = 0;
  std::string format;  // "plain" or "enc-v1"; advisory for decryption (the envelope
                       // magic decides that), but signed, so it must be exact
};

struct desired_state {
  // The tenant these bundles belong to. Part of what every bundle signature
  // covers, and the agent cannot derive it - our certificate carries the tenant
  // *slug*, not this id - so the server sends it. There is nothing to trust in
  // the value: the signing key is per tenant, so a wrong one simply fails
  // verification.
  long long tenant_id = 0;
  std::string state_hash;
  unsigned long next_poll_in_seconds = 60;
  std::string merged_config_json;  // serialized JSON object; "{}" when empty
  std::vector<bundle_info> bundles;  // sorted by ascending priority (apply order)
};

// What a bundle's Ed25519 signature actually covers: the identity the server
// advertised for the bundle, together with the digest of its bytes.
//
// Signing the digest alone (the v1 protocol this replaces) bound nothing about
// *which* bundle those bytes are - it said only "this tenant's server saw this
// blob once" - so an old signed blob could be re-advertised under a different
// id, name or version and still verify. Only the encrypted format's AAD closed
// name and version, and only for that format.
//
// Every field is read out of the desired-state response verbatim; nothing here
// is reconstructed locally, because what is being verified is the server's own
// claim about what this bundle is. The server side is fleet_core::bundlesig.
struct bundle_descriptor {
  long long tenant_id = 0;
  std::string bundle_id;
  std::string name;
  std::string version;
  std::string format;      // "plain" or "enc-v1"
  std::string sha256_hex;  // lowercase hex digest of the bundle bytes

  // The exact bytes signed and verified: a version prefix followed by six
  // NUL-separated fields. Ed25519 hashes internally, so these are signed
  // directly rather than digested first.
  //
  //   nsclient-fleet/bundle-sig/v2 \0 tenant_id \0 id \0 name \0 version \0 format \0 sha256
  //
  // The separator is what keeps the encoding unambiguous: every field is a
  // ULID, an integer, or a token from a grammar with no NUL in it (which
  // parse_desired_state enforces on the way in), so no two distinct bundles
  // can produce the same bytes. Priority is deliberately absent - it belongs
  // to a group assignment, not to the bundle, and the same bundle legitimately
  // carries different priorities in different groups.
  std::string signing_bytes() const;
};

// The descriptor for one bundle of a desired state: pairs the state's tenant
// with the bundle's own advertised fields.
bundle_descriptor describe_bundle(long long tenant_id, const bundle_info &bundle);

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
//   name        no NUL, because they are fields of the NUL-separated bundle
//   version     signing descriptor and a NUL in one would let a single
//   format      signature cover two different bundles
// tenant_id is required whenever the response carries bundles (it is needed to
// build that descriptor) and ignored when it does not.
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

// Incremental SHA-256 for payloads assembled from several pieces (an ini plus
// a tree of staged scripts): feed each piece to update() and read the digest
// with hex_final(), instead of concatenating everything into one buffer for
// sha256_hex. Feeding the same bytes in any chunking yields the same digest.
// hex_final() finalises the stream; the object is not reusable afterwards.
// All three members throw onboarding_error if the underlying digest fails.
class sha256_stream {
 public:
  sha256_stream();
  ~sha256_stream();
  sha256_stream(const sha256_stream &) = delete;
  sha256_stream &operator=(const sha256_stream &) = delete;
  void update(const std::string &bytes);
  std::string hex_final();

 private:
  void *ctx_;  // EVP_MD_CTX; opaque so this header does not drag in OpenSSL
};

// Verify a bundle, in two steps that must both pass:
//   integrity    SHA-256 of `bytes` equals `descriptor.sha256_hex`
//   authenticity `signature_b64` is an Ed25519 signature by the key in
//                `pub_pem` over `descriptor.signing_bytes()` - not over the
//                raw bytes, and not over their digest alone
// Returns false and sets `error` on any mismatch or parse failure.
bool verify_bundle(const std::string &pub_pem, const std::string &bytes, const bundle_descriptor &descriptor, const std::string &signature_b64,
                   std::string &error);

// RFC 7396 JSON Merge Patch: objects deep-merge, scalars/arrays replace
// wholesale, null deletes the key. Returns the patched value.
boost::json::value json_merge_patch(const boost::json::value &target, const boost::json::value &patch);

// Render a merged JSON config to NSClient INI text, deterministically:
// nested objects become sections named by their /-joined path, scalar and
// array members become key=value lines (arrays as comma-separated lists),
// sections and keys are sorted. Nulls are skipped (they only exist pre-merge).
std::string render_ini(const boost::json::value &config);

// --- state report payload ----------------------------------------------------

struct installed_bundle {
  std::string id;
  std::string version;
};

// Build a /agent/v1/state-report body. Pass none as `applied_state_hash`
// after a failed apply (the server then only refreshes last_seen_at).
//
// `local_config_present` reports THAT the host carries local configuration
// outranking the fleet-managed values, never what that configuration is.
//
// `facts_hash` is the sha256 hex of the host's facts document (the hash of
// `{}` when nothing is enabled): the report carries the hash, never the
// document, which goes on its own call (build_facts_upload) and only when it
// changed. Empty omits the member, for a build that cannot hash.
std::string build_state_report(const boost::optional<std::string> &applied_state_hash, const std::vector<installed_bundle> &bundles_installed,
                               const std::vector<std::string> &errors, const std::map<std::string, std::string> &reported_tags,
                               bool local_config_present, const std::string &facts_hash = "");

// --- facts upload -------------------------------------------------------------

// Build a /agent/v1/facts body:
//
//   {"collected_at":"<ts>","facts":<document>,"facts_hash":"<hex>"}
//
// `collected_at` is left out when empty (no facts round has completed).
// `facts_json` is spliced in byte for byte rather than parsed and serialised
// again: `facts_hash` is the digest of exactly those bytes, and a round trip
// through a JSON library is free to re-spell a number or reorder a key, after
// which the server could never reproduce the hash from what it received.
// Throws onboarding_error (non-retryable) when `facts_json` is not an object.
std::string build_facts_upload(const std::string &facts_hash, const std::string &collected_at, const std::string &facts_json);

// The desired-state poll path. Carries what the agent holds, so the server
// can answer "you are in sync" without either side sending anything more:
//   current_hash  the applied desired state (omitted before the first apply)
//   facts_hash    the facts document's hash (omitted in a build that cannot
//                 hash)
// Both values are percent-encoded: a state hash is a token that may carry
// base64's + / =, and a bare '+' in a query decodes as a space.
std::string desired_state_path(const std::string &current_hash, const std::string &facts_hash);

// The response header in which a server says which facts document it holds
// for this host, on any desired-state or state-report response - a 304
// included, which is why it is a header: a 304 has no body.
extern const char *const facts_hash_header;  // "x-facts-hash", lowercase as the client stores it

// Read that header's value. None when it is not a sha256 hex digest or
// `none` - and a missing header is none too, which means "this server does
// not do facts" and is never a reason to upload. `none` means the server
// holds no document for this host, which is the same state as holding the
// empty one, so it is returned as the digest of `{}`. A digest is returned
// lowercase, so it compares directly against our own hash.
boost::optional<std::string> parse_facts_hash(const std::string &header_value);

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
