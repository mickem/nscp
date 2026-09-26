// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <chrono>
#include <string>

namespace onboarding {

// When the fleet sync uploads the facts document: the whole decision, kept
// apart from the transport so every rule can be tested with a clock the test
// controls. fleet_sync feeds it what the server says and how each upload went,
// and asks it before every upload.
//
// The rules:
//
//  * Only in answer to the server. Until a response has said which document
//    it holds, nothing is uploaded; a server that never says does not do
//    facts.
//  * Only on a miss: the document we hold differs from the one it holds.
//  * A rejected upload (400, 401, 404, 429, 5xx: anything the next poll would
//    only repeat) waits 1 min, then 2, doubling up to an hour, before that
//    same document is tried again. The clock belongs to the document: a
//    changed inventory was never tried and goes at once.
//  * A 429's Retry-After holds every upload, whatever the document: it is the
//    server asking for quiet, not a verdict on one document.
//  * A document the server acknowledged and then reports missing is sent
//    again at once, the first time. Each further re-send of it waits on the
//    same doubling clock, so a server that never keeps what it is sent costs
//    an upload an hour, not one per poll.
//  * The server answering with the document it acknowledged resets that - but
//    only once the document's clock has run out. An echo seconds after the
//    re-send only repeats what the upload just set; a document still held a
//    whole backoff window later has stuck, and a loss weeks after that is
//    repaired at once again.
//  * A document that can never be sent as it is (413, over our own cap, or
//    one that could not be rendered) is not tried again until it changes.
class facts_upload_pacer {
 public:
  typedef std::chrono::steady_clock clock;

  // What a server response said it holds (an X-Facts-Hash header, already
  // parsed: `none` arrives as the empty document's digest).
  void server_holds(const std::string &hash, const clock::time_point now) {
    server_ = hash;
    if (!acked_.empty() && hash == acked_ && (backoff_hash_ != hash || now >= retry_at_)) reset();
  }

  // Whether to upload the document whose hash is `current`.
  bool should_upload(const std::string &current, const clock::time_point now) const {
    if (!server_ || current.empty()) return false;
    if (current == server_.value() || current == refused_) return false;
    if (now < server_not_before_) return false;
    if (current == backoff_hash_ && now < retry_at_) return false;
    return true;
  }

  // The server acknowledged (2xx) the document `hash`.
  void acknowledged(const std::string &hash, const clock::time_point now) {
    // Acknowledged before, reported missing since: it did not keep it.
    if (hash == acked_) {
      back_off(hash, now);
    } else {
      reset();
    }
    acked_ = hash;
    server_ = hash;
  }

  // The server rejected the upload of `hash` in a way the next poll would
  // only repeat. `retry_after_seconds` is its Retry-After, 0 when it sent
  // none.
  void rejected(const std::string &hash, const clock::time_point now, const unsigned long retry_after_seconds = 0) {
    back_off(hash, now);
    if (retry_after_seconds > 0) server_not_before_ = now + std::chrono::seconds(retry_after_seconds);
  }

  // `hash` cannot be sent as it is: not tried again until the document
  // changes.
  void refused(const std::string &hash) { refused_ = hash; }

  // For tests and diagnostics: when the document `hash` may next be tried.
  clock::time_point retry_at(const std::string &hash) const {
    const clock::time_point document = hash == backoff_hash_ ? retry_at_ : clock::time_point();
    return std::max(document, server_not_before_);
  }

 private:
  void back_off(const std::string &hash, const clock::time_point now) {
    if (hash != backoff_hash_) {
      backoff_hash_ = hash;
      attempts_ = 0;
    }
    const unsigned long first_step_seconds = 60;
    const unsigned long max_step_seconds = 3600;
    const unsigned int shift = std::min(attempts_, 6u);  // 1 min << 6 is past the hour cap
    retry_at_ = now + std::chrono::seconds(std::min(first_step_seconds << shift, max_step_seconds));
    ++attempts_;
  }

  void reset() {
    backoff_hash_.clear();
    attempts_ = 0;
    retry_at_ = clock::time_point();
  }

  boost::optional<std::string> server_;
  std::string acked_;
  std::string refused_;
  // The document the backoff clock belongs to, how many steps it has taken,
  // and when that document may next be tried.
  std::string backoff_hash_;
  unsigned int attempts_ = 0;
  clock::time_point retry_at_;
  clock::time_point server_not_before_;
};

}  // namespace onboarding
