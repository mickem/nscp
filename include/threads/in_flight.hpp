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
// It can also be closed: try_enter() then refuses, so a remover can shut
// the door on one element, wait for the walks already inside it, and know no
// new one will follow - which is what lets a walk enter the tracker of each
// element only around its own call, instead of every element's up front.
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
    // enter() unless the tracker is closed. Returns whether this guard is
    // now inside.
    bool try_enter() {
      if (entered_) return true;
      entered_ = owner_->try_enter();
      return entered_;
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

  // Refuse every try_enter() from now on, and return the cutoff covering
  // every entry made before - the ones a wait still has to see out. The
  // close and the cutoff are one step under the tracker's own lock, so no
  // entry can slip in between them. enter() is not refused; it is for
  // callers that close a door of their own (dll_plugin).
  std::uint64_t close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    return next_seq_;
  }
  // Let try_enter() in again, after a removal that was refused.
  void reopen() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = false;
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

  bool try_enter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return false;
    entries_.push_back(entry{boost::this_thread::get_id(), next_seq_++});
    return true;
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
  bool closed_ = false;
};

// One element of a list that walks call into - a log subscriber, a metrics
// or facts module - behind its own in_flight gate. The point is ownership:
// the element is reachable only through the gate, so a walk holds no strong
// reference of its own, and the remover, which takes() it after the drain,
// is its last holder. A walk that copied the element instead could be the
// one to drop the last reference, and run the module's destructor - and
// dlclose - on whatever thread it happened to be walking on.
//
// The protocol, for the walk:
//
//     in_flight::guard inside(g.tracker());
//     if (!inside.try_enter()) continue;   // closed: being removed
//     T value = g.value();                 // a copy for the call
//     call(value);
//     value = T();                         // released before leaving
//     inside.leave();
//
// and for the remover: cutoff = g.tracker().close(), then
// wait_for_others_before(cutoff, ...), then take() once that came back
// clean. value() is stable while the caller is inside, because take() only
// runs after every other thread inside has left; the copy is for the one
// case that is not true - a walk that removes its own element from inside
// the call, which the wait does not wait for.
template <class T>
class gated {
 public:
  explicit gated(T value) : value_(std::move(value)) {}
  gated(const gated &) = delete;
  gated &operator=(const gated &) = delete;

  in_flight &tracker() { return tracker_; }
  const T &value() const { return value_; }
  // Hand the element over, after a clean drain.
  T take() {
    T out = std::move(value_);
    value_ = T();
    return out;
  }

 private:
  in_flight tracker_;
  T value_;
};

}  // namespace threads
