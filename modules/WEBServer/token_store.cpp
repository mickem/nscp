// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "token_store.hpp"

#include <cstdlib>
#include <random>
#include <sstream>
#include <vector>

#include "sha256.hpp"

#ifdef USE_SSL
#include <openssl/rand.h>
#endif

static constexpr char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

namespace {
// Fallback generator used ONLY when the build has no CSPRNG at all (USE_SSL
// off). std::random_device is NOT guaranteed by the standard to be
// non-deterministic - some toolchains (historically MinGW-w64) implement it as
// a fixed-seed PRNG, which would make session tokens predictable. It is kept
// solely so a build without OpenSSL still produces *some* token rather than an
// empty string; such a build cannot serve TLS either. A build that HAS a
// CSPRNG never falls back to this - see csprng_bytes() below.
std::string generate_token_fallback(const int len, const std::size_t alphanum_size) {
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, static_cast<int>(alphanum_size) - 1);
  std::string ret;
  ret.reserve(len);
  for (int i = 0; i < len; i++) ret += alphanum[dist(rd)];
  return ret;
}

// Test seam, null in production. See token_store::set_rand_bytes_for_test.
token_store::rand_bytes_fn g_rand_bytes_override = nullptr;

// Fill `buf` from the cryptographic RNG.
//   1  -> success
//   0  -> a CSPRNG exists but failed (do NOT silently substitute a weaker one)
//  -1  -> this build has no CSPRNG at all
int csprng_bytes(unsigned char *buf, const int num) {
  if (g_rand_bytes_override != nullptr) return g_rand_bytes_override(buf, num);
#ifdef USE_SSL
  return RAND_bytes(buf, num);
#else
  (void)buf;
  (void)num;
  return -1;
#endif
}
}  // namespace

void token_store::set_rand_bytes_for_test(const rand_bytes_fn fn) { g_rand_bytes_override = fn; }

namespace {
// Test seam, null in production. See token_store::set_digest_for_test.
token_store::digest_fn g_digest_override = nullptr;
}  // namespace

void token_store::set_digest_for_test(const digest_fn fn) { g_digest_override = fn; }

std::string token_store::generate_token(const int len) {
  constexpr std::size_t alphanum_size = sizeof(alphanum) - 1;
  if (len <= 0) return std::string();
  // Session tokens are the primary bearer credential, so draw them from the
  // same cryptographic RNG the module already links for password salts
  // (password_hash.cpp). Reject bytes at or above the largest multiple of the
  // alphabet size so the modulo mapping stays unbiased (256 % 62 != 0).
  const unsigned int reject_limit = 256 - (256 % static_cast<unsigned int>(alphanum_size));
  std::string ret;
  ret.reserve(len);
  std::vector<unsigned char> buf(static_cast<std::size_t>(len));
  while (static_cast<int>(ret.size()) < len) {
    const int rc = csprng_bytes(buf.data(), static_cast<int>(buf.size()));
    if (rc < 0) {
      // No CSPRNG in this build at all (USE_SSL off) - the weak generator is
      // the only option, and such a build cannot serve TLS anyway.
      return generate_token_fallback(len, alphanum_size);
    }
    if (rc != 1) {
      // A CSPRNG exists but failed. Fail closed: the caller must refuse to
      // issue a credential rather than mint one from a source the standard
      // does not require to be non-deterministic. Callers treat "" as an
      // error (see session_manager_interface::store_user_in_response).
      return std::string();
    }
    for (std::size_t i = 0; i < buf.size() && static_cast<int>(ret.size()) < len; ++i) {
      if (buf[i] < reject_limit) ret += alphanum[buf[i] % alphanum_size];
    }
  }
  return ret;
}

bool token_store::has_hashing() {
  // A digest installed by set_digest_for_test() is a hash function this build
  // has, whatever OpenSSL did or did not supply. Without that second term
  // key_for() would skip hash_token() in a no-OpenSSL build and the seam
  // would be a silent no-op there - functional-looking in one build only.
  return web_digest::has_sha256() || g_digest_override != nullptr;
}

std::string token_store::hash_token(const std::string &in) {
  if (g_digest_override != nullptr) return g_digest_override(in);
  // "" in a build with no digest and no seam. has_hashing() is false in
  // exactly that case, so key_for() keys by the raw token - as it always did -
  // and never stores a key this produced.
  return web_digest::sha256_hex(in);
}

// `grants` is guarded by the same mutex as `tokens`: add_user / add_grant run
// from the settings load path while can() is on the per-request authorisation
// path, so they are not naturally serialised against each other.
bool token_store::can(const std::string &uid, const std::string &grant) {
  const std::lock_guard<std::mutex> lock(mutex_);
  return grants.validate(uid, grant);
}

void token_store::add_user(const std::string &user, const std::string &role) {
  const std::lock_guard<std::mutex> lock(mutex_);
  grants.add_user(user, role);
}

std::string token_store::get_role(const std::string &user) const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return grants.get_role(user);
}

void token_store::add_grant(const std::string &role, const std::string &grant) {
  const std::lock_guard<std::mutex> lock(mutex_);
  grants.add_role(role, grant);
}

namespace session_persistence {
namespace {
constexpr char kSeparator = '\t';
constexpr char kNewline = '\n';
constexpr const char *kVersion = "1";

// A hash is what generate_for() and hash_token() produce: 64 lowercase hex
// characters. Rejecting anything else keeps a hand-edited or corrupted row -
// or, in the worst case, a raw token that some future code path wrote by
// mistake - from being loaded back as a live session.
bool is_hash(const std::string &key) {
  if (key.size() != 64) return false;
  for (const char c : key) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool has_delimiter(const std::string &s) { return s.find(kSeparator) != std::string::npos || s.find(kNewline) != std::string::npos; }

// One record line. Returns false for anything but exactly four fields with a
// hash first, a non-empty user and a positive epoch.
bool parse_record(const std::string &line, token_store::persisted_session &out) {
  // Not str::utils::split_lst: it drops a trailing empty field, and the
  // fingerprint field is legitimately empty for a session issued through the
  // fingerprint-less generate_for() overload.
  const std::string::size_type p1 = line.find(kSeparator);
  if (p1 == std::string::npos) return false;
  const std::string::size_type p2 = line.find(kSeparator, p1 + 1);
  if (p2 == std::string::npos) return false;
  const std::string::size_type p3 = line.find(kSeparator, p2 + 1);
  if (p3 == std::string::npos) return false;
  const std::string hash = line.substr(0, p1);
  const std::string user = line.substr(p1 + 1, p2 - p1 - 1);
  const std::string created = line.substr(p2 + 1, p3 - p2 - 1);
  const std::string fingerprint = line.substr(p3 + 1);
  // A fourth separator means a field we do not understand - refuse rather
  // than silently keep the prefix.
  if (fingerprint.find(kSeparator) != std::string::npos) return false;
  if (!is_hash(hash)) return false;
  if (user.empty()) return false;
  if (created.empty() || created.find_first_not_of("0123456789") != std::string::npos) return false;
  const long long epoch = std::strtoll(created.c_str(), nullptr, 10);
  if (epoch <= 0) return false;
  out.hash = hash;
  out.user = user;
  out.created = static_cast<time_t>(epoch);
  out.fingerprint = fingerprint;
  return true;
}
}  // namespace

std::string serialize_sessions(const std::list<token_store::persisted_session> &sessions) {
  std::ostringstream oss;
  bool any = false;
  for (const token_store::persisted_session &session : sessions) {
    // User names come from settings, so a delimiter in one is a configuration
    // mistake rather than an attack, but a record that cannot be read back is
    // worse than one that was never written.
    if (!is_hash(session.hash) || session.user.empty()) continue;
    if (has_delimiter(session.user) || has_delimiter(session.fingerprint)) continue;
    if (!any) oss << kVersion;
    any = true;
    oss << kNewline << session.hash << kSeparator << session.user << kSeparator << static_cast<long long>(session.created) << kSeparator << session.fingerprint;
  }
  return oss.str();
}

std::list<token_store::persisted_session> parse_sessions(const std::string &value) {
  std::list<token_store::persisted_session> ret;
  if (value.empty()) return ret;
  std::string::size_type pos = value.find(kNewline);
  std::string version = value.substr(0, pos);
  if (!version.empty() && version.back() == '\r') version.pop_back();
  if (version != kVersion) return ret;
  while (pos != std::string::npos) {
    const std::string::size_type next = value.find(kNewline, pos + 1);
    std::string line = value.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
    // A row that has been through a text tool on Windows may carry CRLF; the
    // CR is not part of the fingerprint.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    token_store::persisted_session session;
    if (!line.empty() && parse_record(line, session)) ret.push_back(session);
    pos = next;
  }
  return ret;
}
}  // namespace session_persistence
