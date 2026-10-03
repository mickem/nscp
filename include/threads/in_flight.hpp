// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/thread.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <vector>

namespace threads {

// Tracks the threads inside a region - a walk over a list of plugins, a log
// line's fan-out to its subscribers - so that a thread which has just taken
// something out of that list can wait for the walks that may still hold a
// copy of it before tearing it down.
//
// The shape is the one dll_plugin's dispatch_lock / unload_plugin established
// for its dispatchers, with two refinements that the logger and the plugin
// lists need and that keep it from being pasted a third time:
//
//   * Entries are numbered. A waiter takes a cutoff() under the same lock it
//     mutated its list under, so the entries below it are exactly the walks
//     that can still hold a copy of the removed element, and it waits for
//     those only. Waiting for "nobody else inside" instead never returns
//     under steady traffic, where a new walk starts before the last one ends.
//   * The calling thread's own entries are never waited for. A walk that
//     removes an element from inside its callback (a module unloading another
//     from fetchMetrics, a log handler unsubscribing itself) is one of the
//     walks the waiter would otherwise be waiting on, and it only ends when
//     the waiter returns.
//
// The wait is bounded on the steady clock, so a stepped wall clock cannot
// stretch it. Notification happens under the tracker's lock: a waiter that
// woke on its deadline and returned may already have destroyed the owner of
// this tracker, so the leaving thread must not touch it after unlocking.
class in_flight {
 public:
  in_flight() = default;
  in_flight(const in_flight &) = delete;
  in_flight &operator=(const in_flight &) = delete;

  // Marks the calling thread as inside from enter() until the guard goes.
  // enter() is separate from construction so the caller can take it under
  // the lock that orders it against a waiter's cutoff(); see on_log_message.
  class guard {
    in_flight *owner_;
    bool entered_;

   public:
    explicit guard(in_flight &owner) : owner_(&owner), entered_(false) {}
    ~guard() { leave(); }
    guard(const guard &) = delete;
    guard &operator=(const guard &) = delete;
    // Movable so a delivery can hold one per subscriber in a vector.
    guard(guard &&other) noexcept : owner_(other.owner_), entered_(other.entered_) { other.entered_ = false; }
    guard &operator=(guard &&other) noexcept {
      if (this != &other) {
        leave();
        owner_ = other.owner_;
        entered_ = other.entered_;
        other.entered_ = false;
      }
      return *this;
    }

    void enter() {
      if (entered_) return;
      owner_->enter();
      entered_ = true;
    }
    // Leave before the guard goes, for a caller that is done with one
    // region but still holds the guard. Idempotent.
    void leave() {
      if (!entered_) return;
      owner_->leave();
      entered_ = false;
    }
    bool entered() const { return entered_; }
  };

  // Whether the calling thread is inside already. Only the calling thread
  // adds or removes its own entries, so the answer is stable for it.
  bool on_this_thread() const {
    const boost::thread::id self = boost::this_thread::get_id();
    std::lock_guard<std::mutex> lock(mutex_);
    for (const entry &e : entries_) {
      if (e.thread == self) return true;
    }
    return false;
  }

  // The number the next enter() will get. Every entry made before this call
  // is below it; take it under the lock that the entries were made under.
  std::uint64_t cutoff() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return next_seq_;
  }

  // Wait until no other thread's entry below `cutoff` remains, or `timeout`
  // has passed. Returns false on the timeout - the caller decides whether
  // that is worth a log line; this class has no logger to report to.
  bool wait_for_others_before(std::uint64_t cutoff, std::chrono::milliseconds timeout) const {
    const boost::thread::id self = boost::this_thread::get_id();
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lock(mutex_);
    ++waiters_;
    bool clean = true;
    while (others_before(self, cutoff)) {
      if (idle_.wait_until(lock, deadline) == std::cv_status::timeout && others_before(self, cutoff)) {
        clean = false;
        break;
      }
    }
    --waiters_;
    return clean;
  }

 private:
  struct entry {
    boost::thread::id thread;
    std::uint64_t seq;
  };

  bool others_before(const boost::thread::id &self, std::uint64_t cutoff) const {
    for (const entry &e : entries_) {
      if (e.thread != self && e.seq < cutoff) return true;
    }
    return false;
  }

  void enter() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(entry{boost::this_thread::get_id(), next_seq_++});
  }

  void leave() {
    std::lock_guard<std::mutex> lock(mutex_);
    // The innermost entry for this thread: a nested walk leaves first, and
    // the outer one stays until it returns.
    for (std::vector<entry>::reverse_iterator it = entries_.rbegin(); it != entries_.rend(); ++it) {
      if (it->thread == boost::this_thread::get_id()) {
        entries_.erase(std::next(it).base());
        break;
      }
    }
    if (waiters_ > 0) idle_.notify_all();
  }

  mutable std::mutex mutex_;
  mutable std::condition_variable idle_;
  // A vector rather than a set: it holds a handful of entries at most, and
  // after the first few walks it never allocates again. boost::thread::id
  // rather than std's because that is what the rest of the tree compares
  // threads by (dll_plugin's dispatchers), and it names any thread, however
  // it was started.
  std::vector<entry> entries_;
  std::uint64_t next_seq_ = 0;
  mutable unsigned waiters_ = 0;
};

}  // namespace threads
