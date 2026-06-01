/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
