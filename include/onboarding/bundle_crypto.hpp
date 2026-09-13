// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Encrypted fleet bundles ("enc-v1"): the operator seals a bundle in the
// browser before it is uploaded, so the fleet server only ever stores and
// serves ciphertext. The key never travels through the server - it reaches the
// agent out of band (`nscp enroll --bundle-key`, the FLEET_BUNDLE_KEY installer
// property) and lives in the enrollment manifest next to the identity.
//
// Envelope, byte for byte what the server's reference implementation
// (crates/core/src/encbundle.rs, web/src/crypto.ts) produces:
//
//   "NSEB1" (5) | key fingerprint (8) | nonce (12) | AES-256-GCM ciphertext | tag (16)
//
// The fingerprint is the first 8 bytes of SHA-256 over the raw 32-byte key and
// is what the server shows the operator. The AEAD additional data is
// `name || 0x00 || version` from the desired-state entry, so a sealed bundle
// served under another name or version fails to open: the server cannot
// re-label what it cannot read. There is no key derivation: the 32 bytes the
// operator was given are the AES key.
//
// The SHA-256 the server publishes, and the Ed25519 signature over it, cover
// the envelope, so download verification happens before and independently of
// decryption.
namespace onboarding {

// Raw key length; the operator-facing form is standard base64 of these bytes.
const std::size_t bundle_key_bytes = 32;

// Split a list of keys as one installer property carries them: separated by
// commas, semicolons or whitespace (none of which occur in base64), empties
// dropped. Order is preserved.
std::vector<std::string> split_bundle_keys(const std::string &list);

// Decode an operator-supplied key. Standard base64 with padding, surrounding
// whitespace ignored, and the result must be exactly 32 bytes. `error` never
// echoes the input.
bool parse_bundle_key(const std::string &key_b64, std::string &raw_key, std::string &error);

// The 16 lowercase hex characters the fleet server displays for a key: the
// first 8 bytes of SHA-256 over the raw key.
std::string bundle_key_fingerprint(const std::string &raw_key);

// Whether `bytes` carries the envelope magic. Agents go by this, not by the
// advisory `format` field of the desired state: the magic is inside what the
// signature covers.
bool is_encrypted_bundle(const std::string &bytes);

// Fingerprint (hex) of the key an envelope was sealed with, for "no key for
// this bundle" diagnostics. Empty when `bytes` is not a complete envelope.
std::string encrypted_bundle_fingerprint(const std::string &bytes);

enum class unseal_status {
  ok,
  not_encrypted,  // no envelope magic
  wrong_key,      // the envelope names a different key: try the next one
  corrupt,        // magic present but the envelope is truncated
  failed          // authentication failed: tampered, or name/version substituted
};

// Open one envelope with one raw 32-byte key. `name`/`version` are the
// desired-state values for the bundle (the AEAD additional data).
unseal_status decrypt_bundle(const std::string &raw_key, const std::string &name, const std::string &version, const std::string &blob, std::string &plaintext,
                             std::string &error);

// Seal `plaintext` with a caller-supplied 12-byte nonce. The agent never needs
// to encrypt; this exists so tests can build envelopes deterministically and
// cross-check them against the server's fixed vector. Throws onboarding_error
// on a bad key or nonce length.
std::string encrypt_bundle(const std::string &raw_key, const std::string &nonce, const std::string &name, const std::string &version,
                           const std::string &plaintext);

// The agent's policy for a downloaded, already verified bundle:
//   - not an envelope: passed through unchanged, unless `require_encrypted`
//     (the "server is untrusted" posture) in which case it is refused;
//   - an envelope: opened with the key whose fingerprint it names. A key that
//     does not match is skipped; a key that matches but fails to authenticate
//     is fatal for the bundle (the content was tampered with or re-labelled),
//     it is not a reason to try another key.
// `keys_b64` are the configured keys in operator form; an unparsable entry is
// reported, never silently skipped. Returns false with `error` set (no key
// material in it) when the bundle must not be applied.
bool open_bundle(const std::vector<std::string> &keys_b64, const std::string &name, const std::string &version, const std::string &bytes,
                 bool require_encrypted, std::string &plaintext, std::string &error);

}  // namespace onboarding
