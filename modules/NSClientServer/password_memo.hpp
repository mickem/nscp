// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscp/password_hash.hpp>
#include <str/constant_time.hpp>

#include <mutex>
#include <string>

// Password verification behind check_nt's `<password>&<code>` request, and the
// one derivation it is allowed to cost.
//
// The shared /settings/default/password is stored hashed (pbkdf2-sha256$...),
// which is a deliberately expensive thing to verify - 100k iterations, tens of
// milliseconds. The WEB server pays that once per login and rides a session
// afterwards; check_nt has no session, so verifying it the naive way pays that
// on every single request. On a listener with no rate limiter, that lets any
// host inside `allowed hosts` spend a core of agent CPU at a few dozen requests
// a second.
//
// So derive once. The first request that proves itself against the hash leaves
// the clear text behind, and every request after it - right password or wrong -
// is answered by a constant-time compare, because a value that is not the one
// clear text hashing to the stored hash cannot be the password. Only the window
// before the first successful login runs the KDF, and the monitoring system
// closes that on its first poll.
//
// The memo does put the clear text in the agent's memory once a login has
// succeeded. That is not what the on-disk hashing is for: the hash protects
// nsclient.ini from anyone who can read the file, while anyone who can read
// this process's memory already has the check_nt traffic and every other secret
// the agent holds.
namespace check_nt_password {

class memo {
 public:
  memo() : known_(false), derivations_(0) {}

  // True when `offered` is the password `stored` stands for. `stored` is either
  // the clear text or the hashed form; both compare in constant time.
  //
  // The lock spans the whole verification rather than just the memo. The fast
  // path is a string compare, so serialising it costs nothing at any rate a
  // monitoring system polls at, and during the cold window it keeps the KDF on
  // one core instead of letting a flood of requests occupy all of them.
  bool verify(const std::string &offered, const std::string &stored) {
    // Not a stored hash: verify_password compares constant-time against a
    // clear-text value, and rejects a value that carries the hash prefix but
    // does not parse. Neither runs the KDF, and routing through it rather than
    // comparing here keeps that fail-closed rule in one place.
    if (!password_hash::is_hashed(stored)) {
      return password_hash::verify_password(offered, stored);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (known_ && hash_ == stored) {
      return str::constant_time_eq(offered, clear_);
    }
    ++derivations_;
    if (!password_hash::verify_password(offered, stored)) {
      return false;
    }
    clear_ = offered;
    hash_ = stored;
    known_ = true;
    return true;
  }

  // Drop the memo. A settings reload may hand the module a different password,
  // and the memo belongs to the old one. verify() notices that on its own (it
  // keeps the stored value it learned the clear text from), so this is for the
  // reload path to be explicit rather than to rely on that.
  void forget() {
    std::lock_guard<std::mutex> lock(mutex_);
    known_ = false;
    clear_.clear();
    hash_.clear();
  }

  // How many PBKDF2 verifications this memo has run. The point of the memo is
  // that this stops growing, so the tests assert on it.
  unsigned derivations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return derivations_;
  }

 private:
  bool known_;
  std::string clear_;
  // The stored value `clear_` was learned from, so a password changed under us
  // is never answered from a memo of the previous one.
  std::string hash_;
  unsigned derivations_;
  mutable std::mutex mutex_;
};

}  // namespace check_nt_password
