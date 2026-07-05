// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <cstddef>

#include <boost/thread.hpp>

/**
 * RAII-owning group of background worker threads.
 *
 *   * Threads are joined on destruction.
 *   * interrupt_all() delivers boost::thread_interrupted to threads parked
 *     on an interruption point; it does not preempt running code.
 *   * Thread bodies that throw (boost::thread_interrupted, std::exception,
 *     or anything else) are caught and swallowed so an escaping exception
 *     cannot terminate the process. Diagnostics belong in the body itself.
 *   * count() returns the number of currently-live worker threads.
 *
 * Non-copyable.
 *
 * Thread-safety:
 *   wait_all() and interrupt_all() are idempotent — boost::thread_group's
 *   join_all() / interrupt_all() check joinable() per thread before acting,
 *   so the destructor calling wait_all() after a prior explicit wait_all()
 *   (e.g. from a shutdown sequence) is a safe no-op.
 *
 *   Precondition: create_thread() must not race with wait_all() from another
 *   thread. boost::thread_group's internal locking keeps the container
 *   consistent, but a thread created while wait_all() is in flight may
 *   become an orphan that the in-flight join misses. All current callers
 *   funnel start/stop through a single owning thread, so this is a documented
 *   precondition rather than an enforced one.
 */
class scoped_thread_group {
 public:
  scoped_thread_group() = default;
  ~scoped_thread_group() { wait_all(); }

  scoped_thread_group(const scoped_thread_group&) = delete;
  scoped_thread_group& operator=(const scoped_thread_group&) = delete;

  template <typename Callable>
  void create_thread(Callable f) {
    live_count_.fetch_add(1, std::memory_order_relaxed);
    try {
      group_.create_thread([this, f]() mutable {
        try {
          f();
        } catch (const boost::thread_interrupted&) {
          // Cooperative interruption — normal exit path on shutdown.
        } catch (const std::exception&) {
          // Swallow: an uncaught exception in a worker calls std::terminate().
        } catch (...) {
        }
        live_count_.fetch_sub(1, std::memory_order_relaxed);
      });
    } catch (...) {
      // Spawn failed (e.g. boost::thread_resource_error, std::bad_alloc);
      // roll back the optimistic counter increment.
      live_count_.fetch_sub(1, std::memory_order_relaxed);
      throw;
    }
  }

  void interrupt_all() { group_.interrupt_all(); }

  void wait_all() { group_.join_all(); }

  std::size_t count() const {
    return live_count_.load(std::memory_order_relaxed);
  }

 private:
  boost::thread_group group_;
  std::atomic<std::size_t> live_count_{0};
};
