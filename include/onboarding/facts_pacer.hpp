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
//  * Only in answer to the server. Until a response has said which document it
//    holds, nothing is uploaded; a server that stops saying (a downgrade to a
//    build without facts) is back to not doing facts, and gets nothing more.
//  * Only on a miss: the document we hold differs from the one it holds.
//  * A rejected upload (400, 401, 404, 429, 5xx: anything the next poll would
//    only repeat) waits 1 min, then 2, doubling up to an hour, before that
//    same document is tried again. The clock belongs to the document - a
//    changed inventory was never tried and goes at once - and any successful
//    upload clears it: a rejection is a transient state, not a record, and a
//    document that comes back after a detour does not inherit an old clock.
//  * A hold (a Retry-After, on any call) pauses every upload, whatever the
//    document: it is the server asking for quiet, not a verdict on one
//    document.
//  * A document the server acknowledged and then reports missing has been
//    lost, which is a separate count from rejections. The first re-send is
//    immediate; each further one waits 1 min, 2, ... up to an hour after the
//    previous one, so a server that never keeps what it is sent costs an
//    upload an hour, not one per poll.
//  * The server answering with the lost document once the wait since its last
//    re-send has passed means it stuck: the loss count resets, so a loss weeks
//    later is repaired at once again. An echo inside that wait only repeats
//    what the re-send just set, and says nothing.
//  * A document the server acknowledged twice without ever answering with its
//    hash in between is not being lost: the server is computing a different
//    hash for it (hashing its own re-encoding, say), and no number of
//    re-sends will make them match. It is refused until it changes, and the
//    caller is told so it can say what is wrong.
//  * A document that can never be sent as it is (413, or one that could not
//    be rendered) is not tried again until it changes.
class facts_upload_pacer {
 public:
  typedef std::chrono::steady_clock clock;

  // What a server response said it holds (an X-Facts-Hash header, already
  // parsed: `none` arrives as the empty document's digest).
  void server_holds(const std::string &hash, const clock::time_point now) {
    server_ = hash;
    if (!acked_.empty() && hash == acked_) confirmed_ = true;
    if (resends_ > 0 && hash == acked_ && now >= resend_at_) {
      resends_ = 0;
      resend_at_ = clock::time_point();
    }
  }

  // A response the server meant carried no X-Facts-Hash: it does not do
  // facts (any more). Nothing is uploaded until one says what it holds again.
  void server_silent() { server_ = boost::none; }

  // Whether to upload the document whose hash is `current`.
  bool should_upload(const std::string &current, const clock::time_point now) const {
    if (!server_ || current.empty()) return false;
    if (current == server_.value() || current == refused_) return false;
    if (now < hold_until_) return false;
    if (current == rejected_hash_ && now < retry_at_) return false;
    if (current == acked_ && now < resend_at_) return false;
    return true;
  }

  // What an acknowledgement means.
  struct ack {
    // How many times in a row this document has now been re-sent because the
    // server lost it: 0 for a document it did not have before.
    unsigned int resends = 0;
    // The server acknowledged it before and never once answered with its
    // hash since: it hashes the document differently, and the document is
    // now refused until it changes.
    bool mismatch = false;
  };

  // The server acknowledged (2xx) the document `hash`.
  ack acknowledged(const std::string &hash, const clock::time_point now) {
    ack result;
    // Whatever was rejected - this document or another one - the server is
    // taking uploads again.
    clear_rejections();
    if (hash == acked_) {
      if (!confirmed_) {
        // Twice acknowledged, never once reported held.
        refused_ = hash;
        result.mismatch = true;
      } else {
        // Acknowledged, reported held, reported missing since: lost. The next
        // re-send of it waits.
        ++resends_;
        resend_at_ = now + step(resends_ - 1);
      }
    } else {
      acked_ = hash;
      resends_ = 0;
      resend_at_ = clock::time_point();
    }
    confirmed_ = false;
    server_ = hash;
    result.resends = resends_;
    return result;
  }

  // The server rejected the upload of `hash` in a way the next poll would
  // only repeat.
  void rejected(const std::string &hash, const clock::time_point now) {
    if (hash != rejected_hash_) {
      rejected_hash_ = hash;
      rejections_ = 0;
    }
    retry_at_ = now + step(rejections_);
    ++rejections_;
  }

  // The server asked for quiet: no upload of any document before `seconds`
  // have passed. A shorter hold never cuts a longer one short.
  void hold(const clock::time_point now, const unsigned long seconds) {
    if (seconds == 0) return;
    hold_until_ = std::max(hold_until_, now + std::chrono::seconds(seconds));
  }

  // `hash` cannot be sent as it is: not tried again until the document
  // changes.
  void refused(const std::string &hash) { refused_ = hash; }

  // For tests and diagnostics: when the document `hash` may next be tried.
  clock::time_point retry_at(const std::string &hash) const {
    clock::time_point at = hold_until_;
    if (hash == rejected_hash_) at = std::max(at, retry_at_);
    if (hash == acked_) at = std::max(at, resend_at_);
    return at;
  }
  unsigned int rejections() const { return rejections_; }
  unsigned int resends() const { return resends_; }

 private:
  // The wait after `taken` earlier steps: 1 min, doubling, capped at an hour.
  static std::chrono::seconds step(const unsigned int taken) {
    const unsigned long first_step_seconds = 60;
    const unsigned long max_step_seconds = 3600;
    const unsigned int shift = std::min(taken, 6u);  // 1 min << 6 is past the hour cap
    return std::chrono::seconds(std::min(first_step_seconds << shift, max_step_seconds));
  }

  void clear_rejections() {
    rejected_hash_.clear();
    rejections_ = 0;
    retry_at_ = clock::time_point();
  }

  boost::optional<std::string> server_;
  std::string refused_;
  clock::time_point hold_until_;
  // Rejections: the document they belong to, how many in a row, and when it
  // may next be tried.
  std::string rejected_hash_;
  unsigned int rejections_ = 0;
  clock::time_point retry_at_;
  // Losses: the document the server last acknowledged, how many times it has
  // been re-sent since because the server reported it missing, and when the
  // next re-send may go.
  std::string acked_;
  unsigned int resends_ = 0;
  clock::time_point resend_at_;
  // Whether the server has answered with acked_ since it acknowledged it.
  bool confirmed_ = false;
};

}  // namespace onboarding
