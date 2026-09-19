// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// SHA-256 (FIPS 180-4), self-contained.
//
// The fact repository publishes a digest of the host's inventory document so
// the fleet server can see "this changed" without being sent the document
// (service/fact_repository.hpp). That digest has to exist in every build,
// and OpenSSL is an *optional* dependency of this project: the fleet sync and
// the onboarding library - the other users of SHA-256 - are compiled only
// under OPENSSL_FOUND, and an agent built without it still has to report a
// facts hash. Rather than gate the repository on OpenSSL, or teach it two
// code paths, the digest is computed here.
//
// This is a change-detection digest, not a security primitive: nothing signs
// or authenticates with it. It is still the real algorithm, pinned against the
// FIPS 180-4 vectors in sha256_test.cpp, so the bytes match what any other
// SHA-256 implementation (the server's included) computes over the same
// document.
namespace algorithms {
namespace sha256_detail {

inline uint32_t rotr(const uint32_t x, const unsigned int n) { return (x >> n) | (x << (32 - n)); }

// The first 32 bits of the fractional parts of the cube roots of the first 64
// primes (FIPS 180-4 section 4.2.2).
inline const uint32_t *round_constants() {
  static const uint32_t k[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                                 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                                 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                                 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                                 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                                 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                                 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                                 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  return k;
}

inline void compress(uint32_t state[8], const unsigned char block[64]) {
  const uint32_t *k = round_constants();
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) | (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (int i = 0; i < 64; ++i) {
    const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t temp1 = h + S1 + ch + k[i] + w[i];
    const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = S0 + maj;
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

}  // namespace sha256_detail

// The SHA-256 digest of `data` as 32 raw bytes.
inline std::string sha256_raw(const std::string &data) {
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  const std::size_t full_blocks = data.size() / 64;
  for (std::size_t i = 0; i < full_blocks; ++i) {
    sha256_detail::compress(state, reinterpret_cast<const unsigned char *>(data.data()) + i * 64);
  }

  // The tail: the remaining bytes, 0x80, zero padding, and the bit length as a
  // big-endian 64-bit integer. Two blocks when the remainder leaves no room
  // for the length field.
  unsigned char tail[128];
  std::memset(tail, 0, sizeof(tail));
  const std::size_t rest = data.size() - full_blocks * 64;
  if (rest > 0) std::memcpy(tail, data.data() + full_blocks * 64, rest);
  tail[rest] = 0x80;
  const std::size_t tail_blocks = rest < 56 ? 1 : 2;
  const uint64_t bits = static_cast<uint64_t>(data.size()) * 8;
  for (int i = 0; i < 8; ++i) {
    tail[tail_blocks * 64 - 1 - i] = static_cast<unsigned char>((bits >> (8 * i)) & 0xff);
  }
  for (std::size_t i = 0; i < tail_blocks; ++i) {
    sha256_detail::compress(state, tail + i * 64);
  }

  std::string out(32, '\0');
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<char>((state[i] >> 24) & 0xff);
    out[i * 4 + 1] = static_cast<char>((state[i] >> 16) & 0xff);
    out[i * 4 + 2] = static_cast<char>((state[i] >> 8) & 0xff);
    out[i * 4 + 3] = static_cast<char>(state[i] & 0xff);
  }
  return out;
}

// The SHA-256 digest of `data` as 64 lower-case hex characters.
inline std::string sha256_hex(const std::string &data) {
  static const char digits[] = "0123456789abcdef";
  const std::string raw = sha256_raw(data);
  std::string out;
  out.reserve(raw.size() * 2);
  for (const char c : raw) {
    const auto b = static_cast<unsigned char>(c);
    out.push_back(digits[b >> 4]);
    out.push_back(digits[b & 0x0f]);
  }
  return out;
}

}  // namespace algorithms
