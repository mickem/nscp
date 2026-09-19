// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// A self-contained SHA-256.
//
// The tree already has one, in libs/onboarding (onboarding::sha256_hex), but
// that one is an OpenSSL EVP digest: it only exists in builds where OpenSSL
// was found, and it lives in a library the core does not link. The facts
// document hash is core state - it is what /api/v2/facts returns as its ETag
// and what the state report carries as facts_hash - so it has to be computed
// the same way in every build, including the ones without OpenSSL, and by a
// unit test that links nothing.
//
// That is what this is for: a small, dependency-free implementation of
// FIPS 180-4 SHA-256 for hashing in-memory strings. Anything that needs
// streaming, HMAC or a hash a remote peer verifies should keep using the
// OpenSSL path instead.
namespace hash {
namespace detail {

inline std::uint32_t rotr(const std::uint32_t value, const unsigned int bits) { return (value >> bits) | (value << (32 - bits)); }

inline const std::uint32_t *round_constants() {
  static const std::uint32_t k[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
                                      0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                                      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
                                      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                                      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
                                      0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                                      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  return k;
}

inline void transform(std::uint32_t state[8], const unsigned char block[64]) {
  const std::uint32_t *k = round_constants();
  std::uint32_t w[64];
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) | static_cast<std::uint32_t>(block[i * 4 + 3]);
  }
  for (std::size_t i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (std::size_t i = 0; i < 64; ++i) {
    const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ (~e & g);
    const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
    const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}
}  // namespace detail

// The SHA-256 digest of `data` as 64 lowercase hex characters.
inline std::string sha256_hex(const std::string &data) {
  std::uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  const unsigned char *bytes = reinterpret_cast<const unsigned char *>(data.data());
  const std::size_t size = data.size();
  std::size_t offset = 0;
  for (; offset + 64 <= size; offset += 64) {
    detail::transform(state, bytes + offset);
  }

  // The tail: what is left of the message, the 0x80 terminator, zero padding
  // and the message length in bits as a big-endian 64 bit number. Two blocks
  // are needed when the remainder leaves no room for the length.
  unsigned char tail[128] = {0};
  const std::size_t remaining = size - offset;
  for (std::size_t i = 0; i < remaining; ++i) tail[i] = bytes[offset + i];
  tail[remaining] = 0x80;
  const std::size_t tail_size = remaining < 56 ? 64 : 128;
  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
  for (std::size_t i = 0; i < 8; ++i) {
    tail[tail_size - 1 - i] = static_cast<unsigned char>((bits >> (8 * i)) & 0xff);
  }
  detail::transform(state, tail);
  if (tail_size == 128) detail::transform(state, tail + 64);

  static const char *digits = "0123456789abcdef";
  std::string hex;
  hex.reserve(64);
  for (const std::uint32_t word : state) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      hex.push_back(digits[(word >> shift) & 0xf]);
    }
  }
  return hex;
}

}  // namespace hash
