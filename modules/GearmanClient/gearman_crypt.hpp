// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

/**
 * The Mod-Gearman payload envelope.
 *
 * Every job and every result travels as `base64(AES-256-ECB(key32, text))`,
 * where the shared password is NUL-padded to a 32 byte key and the plaintext
 * is zero-padded by hand with OpenSSL's own padding switched off. This is what
 * `common/gm_crypt.c` does, byte for byte identically in ConSol's mod_gearman
 * and in the nagios-mod-gearman fork; the captured payloads under
 * `modules/GearmanClient/fixtures/` prove it for both.
 *
 * ECB with the password used directly as the key hides the content and
 * nothing else: it does not authenticate the sender and it does not prevent
 * replay. That is the protocol's design and the module cannot fix it, so
 * nothing here pretends otherwise - see the security section of the scenario
 * documentation for what containing it looks like.
 */
namespace gearman {

/** Thrown when a payload cannot be decoded, which usually means the wrong key. */
class crypt_error : public std::runtime_error {
 public:
  explicit crypt_error(const std::string &what) : std::runtime_error(what) {}
};

const std::size_t key_bytes = 32;
const std::size_t block_size = 16;

/** The password truncated or NUL-padded to exactly 32 bytes (`mod_gm_aes_init`). */
std::string make_key(const std::string &password);

/**
 * Encrypt as `mod_gm_encrypt` does: the plaintext carries its terminating NUL
 * (`strlen + 1`) and is zero-padded to the block size by the reference's own
 * rule, which is `BLOCKSIZE % len != 0` rather than `len % BLOCKSIZE != 0`.
 * For a length that is a multiple of 16 other than 16 itself that test is true
 * and appends a whole extra block of zeros; the quirk is reproduced here so
 * the bytes match what the cores accept and send.
 *
 * (The same rule leaves a partial block for lengths 1, 2, 4 and 8, which the
 * reference then fails to encrypt at all. Those are padded to a full block
 * instead of failing: no reachable payload is that short, so this changes no
 * byte either flavour can produce.)
 */
std::string encrypt_payload(const std::string &text, const std::string &password);

/**
 * Decrypt a base64 envelope (`mod_gm_decrypt`): whitespace is stripped before
 * decoding, a trailing partial block is dropped, and the plaintext is cut at
 * the first NUL - the reference reads the result as a C string, which is what
 * discards the zero padding.
 */
std::string decrypt_payload(const std::string &encoded, const std::string &password);

/** `encryption=no`: base64 of the text, nothing else. */
std::string encode_plain_payload(const std::string &text);
std::string decode_plain_payload(const std::string &encoded);

struct envelope {
  /** Off means base64 only, which needs `insecure` in the settings. */
  bool encryption = true;
  /** The shared password, not the derived key; ignored when encryption is off. */
  std::string key;

  envelope() = default;
  envelope(bool encryption_, std::string key_) : encryption(encryption_), key(std::move(key_)) {}
};

std::string encode_payload(const std::string &text, const envelope &settings);

/**
 * Decode one payload. With encryption on this decrypts and nothing else: a
 * cleartext payload is refused rather than sniffed, because accepting one
 * would let anybody who can reach gearmand inject jobs without the key.
 * (Upstream accepts cleartext only on the core's result side, and only behind
 * its explicit `accept_clear_results` option.)
 */
std::string decode_payload(const std::string &encoded, const envelope &settings);

}  // namespace gearman
