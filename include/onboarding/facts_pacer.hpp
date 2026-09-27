// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <chrono>
#include <onboarding/facts_hash.hpp>
#include <string>

namespace onboarding {

// When the fleet sync uploads the facts document: the whole decision, kept
// apart from the transport so every rule can be tested with a clock the test
// controls. fleet_sync tells it when a loop turn starts, what the server says
// and how each upload went, and asks it before every upload.
//
// The rules:
//
//  * Only in answer to the server. Until a response has said which document it
//    holds, nothing is uploaded; a server that stops saying (a downgrade to a
//    build without facts) is back to not doing facts, and gets nothing more.
//  * Only on a miss: the document we hold differs from the one it holds.
//  * A rejected upload (any error status but 413: anything the next poll would
//    only repeat) waits 1 min, then 2, doubling up to an hour, before that
//    same document is tried again. The clock belongs to the document - a
//    changed inventory was never tried and goes at once - and any successful
//    upload clears it: a rejection is a transient state, not a record, and a
//    document that comes back after a detour does not inherit an old clock.
//  * Trouble in a loop turn - an error answer to its poll or report - means no
//    upload in that turn. A rate-limited upload means none in the next turn
//    either. Turns, not a clock, so the skip lines up with the jittered poll
//    it is about; begin_turn() moves the carried-over skip into the turn that
//    starts, whether or not that turn gets as far as asking.
//  * A hold (a Retry-After on a report or an upload) pauses every upload,
//    whatever the document: it is the server asking for quiet, not a verdict
//    on one document. A poll's own Retry-After (429 or 503) is not a hold: the
//    loop sleeps it, and a second clock over the same wait would only outlast
//    that sleep.
//  * A document the server acknowledged, reported holding, and then reports
//    missing has been lost. The first re-send is immediate; each further one
//    waits 1 min, 2, ... up to an hour after the previous one, so a server that
//    never keeps what it is sent costs an upload an hour, not one per poll.
//    The server answering with it once the wait since the last re-send has
//    passed means it stuck: the loss count resets, so a loss weeks later is
//    repaired at once again. An echo inside that wait says nothing.
//  * An acknowledgement that carries the server's X-Facts-Hash settles at once
//    what the server made of the document: our hash is confirmation, `none` is
//    a server that did not keep it, and any other new digest is a server
//    hashing the document differently - no re-send will ever match, so the
//    document is refused (see the last rule), and the caller is told so it can
//    say what is wrong. The digest the server held before the upload is not
//    new: a server whose storage is asynchronous echoes it until the write
//    lands, so it is an unconfirmed acknowledgement, left to the rule below.
//  * An acknowledgement without the header proves nothing either way, so the
//    same verdict takes three acknowledgements of one document with no
//    confirmation between them. A document not yet confirmed is re-sent a step
//    (1 min) after its upload at the soonest, then on the doubling steps, so a
//    single stale answer (a queued write, a lagging replica, a cache) neither
//    reaches the verdict nor costs a second copy of the document.
//  * A document that cannot be sent as it is (413, one that could not be
//    rendered, or a hash mismatch) is not tried again until it changes, the
//    hash the server reports changes (a hashing bug fixed on its side), or a
//    day has passed (what its answers cannot show, like a raised size cap).
class facts_upload_pacer {
 public:
  typedef std::chrono::steady_clock clock;

  // The sync starts (again, after a restart): no turn is carried over. The
  // restart delay has already given a server that asked for quiet more than
  // the one turn it asked for.
  void start() {
    skip_this_turn_ = false;
    skip_next_turn_ = false;
  }

  // A loop turn starts: whatever the last one carried over applies now.
  void begin_turn() {
    skip_this_turn_ = skip_next_turn_;
    skip_next_turn_ = false;
  }
  // A poll or report in this turn got an error answer: no upload this turn.
  void trouble_this_turn() { skip_this_turn_ = true; }
  // The upload was rate limited: no upload in the next turn either.
  void quiet_next_turn() { skip_next_turn_ = true; }

  // What a server response said it holds (an X-Facts-Hash header, already
  // parsed: `none` arrives as the empty document's digest).
  void server_holds(const std::string &hash, const clock::time_point now) {
    server_ = hash;
    if (!acked_.empty() && hash == acked_) confirm(now);
    if (!refused_.empty()) {
      // The first answer after a refusal is what the server held when it
      // refused; any later, different one means something changed on its
      // side (a hashing bug fixed, the store wiped) and the refused document
      // is worth offering again.
      if (!refused_server_) {
        refused_server_ = hash;
      } else if (hash != refused_server_.value()) {
        clear_refusal();
      }
    }
  }

  // A response the server meant carried no readable X-Facts-Hash: it does not
  // do facts (any more). Nothing is uploaded until one says what it holds.
  void server_silent() { server_ = boost::none; }

  // Whether to upload the document whose hash is `current`.
  bool should_upload(const std::string &current, const clock::time_point now) const {
    if (skip_this_turn_) return false;
    if (!server_ || current.empty()) return false;
    if (current == server_.value()) return false;
    if (current == refused_ && now < refused_until_) return false;
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
    // The server does not end up holding the document as we hash it (see
    // the rules); it is refused (see refused()).
    bool mismatch = false;
    // The mismatch was stated by the server itself - it answered the upload
    // with a different digest - rather than inferred from acknowledgements
    // that were never confirmed (which a server that simply does not keep the
    // document produces too).
    bool hashed_differently = false;
    // The acknowledgement itself said the server holds the document.
    bool confirmed = false;
  };

  // The server acknowledged (2xx) the document `hash`. `server_says` is the
  // X-Facts-Hash that came with the acknowledgement, parsed, when there was a
  // readable one.
  ack acknowledged(const std::string &hash, const clock::time_point now, const boost::optional<std::string> &server_says = boost::none) {
    ack result;
    // What the server said it held before this upload: an acknowledgement
    // that merely repeats it is stale, not a verdict.
    const boost::optional<std::string> held_before = server_;
    // Whatever was rejected - this document or another one - the server is
    // taking uploads again.
    clear_rejections();
    if (hash == acked_) {
      if (confirmed_) {
        // Acknowledged, reported held, reported missing since: lost. The next
        // re-send of it waits.
        ++resends_;
        resend_at_ = now + step(resends_ - 1);
        unconfirmed_ = 0;
      } else {
        // Acknowledged again without a confirmation in between: paced like a
        // loss, and counted towards the verdict below.
        resend_at_ = now + step(unconfirmed_ > 0 ? unconfirmed_ - 1 : 0);
      }
    } else {
      acked_ = hash;
      resends_ = 0;
      // A document the server has not confirmed yet is not re-sent before a
      // step has passed, even if the next poll still answers with what it
      // held before: an asynchronous store gets the time to land the write
      // instead of a second copy of it. A confirmation lifts this, so a
      // confirmed document that is lost later is re-sent at once.
      resend_at_ = now + step(0);
      unconfirmed_ = 0;
    }
    confirmed_ = false;
    ++unconfirmed_;
    server_ = hash;

    if (server_says) {
      const std::string &says = server_says.value();
      if (says == hash) {
        confirm(now);
        result.confirmed = true;
      } else if (says == empty_facts_hash || (held_before && says == held_before.value())) {
        // It says it holds nothing, or still what it held before - the write
        // has not landed, or did not keep. Not a verdict on its own: the
        // unconfirmed count below decides, over paced re-sends.
        server_ = server_says;
      } else {
        // It kept something new, and says it is something other than what
        // it was sent.
        result.mismatch = true;
        result.hashed_differently = true;
      }
    }
    if (!result.mismatch && unconfirmed_ >= unconfirmed_verdict) result.mismatch = true;
    if (result.mismatch) {
      refused(hash, now);
      // What the server said instead is the baseline a later change is
      // measured against.
      if (server_says) refused_server_ = server_says;
    }
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
  // changes, the hash the server reports changes, or a day has passed - the
  // last for what the server's answers cannot show, such as its size cap
  // being raised after a 413.
  void refused(const std::string &hash, const clock::time_point now) {
    refused_ = hash;
    refused_until_ = now + std::chrono::hours(24);
    refused_server_ = boost::none;
  }

  // For tests and diagnostics: when the document `hash` may next be tried.
  clock::time_point retry_at(const std::string &hash) const {
    clock::time_point at = hold_until_;
    if (hash == rejected_hash_) at = std::max(at, retry_at_);
    if (hash == acked_) at = std::max(at, resend_at_);
    return at;
  }
  unsigned int rejections() const { return rejections_; }
  unsigned int resends() const { return resends_; }

  // Acknowledgements without a confirmation that make a hash mismatch, when
  // the server says nothing on the acknowledgement itself.
  static const unsigned int unconfirmed_verdict = 3;

 private:
  // The wait after `taken` earlier steps: 1 min, doubling, capped at an hour.
  static std::chrono::seconds step(const unsigned int taken) {
    const unsigned long first_step_seconds = 60;
    const unsigned long max_step_seconds = 3600;
    const unsigned int shift = std::min(taken, 6u);  // 1 min << 6 is past the hour cap
    return std::chrono::seconds(std::min(first_step_seconds << shift, max_step_seconds));
  }

  // The server holds the acknowledged document.
  void confirm(const clock::time_point now) {
    confirmed_ = true;
    unconfirmed_ = 0;
    // Never lost, or kept through the whole wait since the last re-send: it
    // stuck, and a later loss is re-sent at once.
    if (resends_ == 0 || now >= resend_at_) {
      resends_ = 0;
      resend_at_ = clock::time_point();
    }
  }

  void clear_refusal() {
    refused_.clear();
    refused_until_ = clock::time_point();
    refused_server_ = boost::none;
  }

  void clear_rejections() {
    rejected_hash_.clear();
    rejections_ = 0;
    retry_at_ = clock::time_point();
  }

  boost::optional<std::string> server_;
  // A document that is not offered again, until when at the latest, and what
  // the server reported when it was refused.
  std::string refused_;
  clock::time_point refused_until_;
  boost::optional<std::string> refused_server_;
  clock::time_point hold_until_;
  bool skip_this_turn_ = false;
  bool skip_next_turn_ = false;
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
  // Whether the server has answered with acked_ since it last acknowledged
  // it, and how many acknowledgements of it have gone unconfirmed in a row.
  bool confirmed_ = false;
  unsigned int unconfirmed_ = 0;
};

}  // namespace onboarding
