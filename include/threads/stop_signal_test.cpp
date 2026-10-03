// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <threads/stop_signal.hpp>

#ifndef WIN32
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#endif

namespace {
bool is_signalled(const threads::stop_signal &signal) {
#ifdef WIN32
  return ::WaitForSingleObject(signal.native_handle(), 0) == WAIT_OBJECT_0;
#else
  struct pollfd fd = {signal.wait_fd(), POLLIN, 0};
  return ::poll(&fd, 1, 0) > 0;
#endif
}
}  // namespace

TEST(stop_signal, is_unsignalled_until_signalled) {
  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  EXPECT_TRUE(signal.valid());
  EXPECT_FALSE(is_signalled(signal));
  signal.signal();
  EXPECT_TRUE(is_signalled(signal));
}

TEST(stop_signal, signal_and_close_are_quiet_once_closed) {
  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  signal.close();
  EXPECT_FALSE(signal.valid());
  signal.signal();
  signal.close();
  EXPECT_FALSE(signal.valid());
}

#ifndef WIN32
namespace {
std::set<int> open_descriptors() {
  std::set<int> fds;
  for (int fd = 0; fd < 1024; ++fd) {
    if (::fcntl(fd, F_GETFD) != -1) fds.insert(fd);
  }
  return fds;
}
}  // namespace

TEST(stop_signal, pipe_is_not_inherited_by_child_processes) {
  // A child the agent spawns (exec_command, an external script) must not
  // inherit either end: one that wrote to it would stop the worker for good.
  // Only the read end is exposed, so both are found as the descriptors
  // create() opened.
  const std::set<int> before = open_descriptors();
  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  std::set<int> opened;
  for (const int fd : open_descriptors()) {
    if (before.count(fd) == 0) opened.insert(fd);
  }
  ASSERT_EQ(opened.size(), 2u);
  EXPECT_EQ(opened.count(signal.wait_fd()), 1u);
  for (const int fd : opened) EXPECT_NE(::fcntl(fd, F_GETFD) & FD_CLOEXEC, 0) << "descriptor " << fd;
}
#endif

TEST(stop_signal, wait_for_times_out_when_not_signalled) {
  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  const auto before = std::chrono::steady_clock::now();
  EXPECT_EQ(threads::stop_signal::wait_result::timed_out, signal.wait_for(std::chrono::milliseconds(50)));
  EXPECT_GE(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(50));
}

TEST(stop_signal, wait_for_returns_as_soon_as_signalled) {
  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  std::thread signaller([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    signal.signal();
  });
  const auto before = std::chrono::steady_clock::now();
  EXPECT_EQ(threads::stop_signal::wait_result::signalled, signal.wait_for(std::chrono::seconds(30)));
  EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(10));
  signaller.join();
}

TEST(stop_signal, wait_for_fails_on_a_signal_that_was_never_created) {
  const threads::stop_signal signal;
  EXPECT_EQ(threads::stop_signal::wait_result::failed, signal.wait_for(std::chrono::milliseconds(10)));
}

#ifndef WIN32
namespace {
void ignore_signal(int) {}
}  // namespace

TEST(stop_signal, wait_for_resumes_after_an_interrupting_signal) {
  // A signal handler interrupts poll() with EINTR. That used to count as the
  // wait being over, so the collector took its next sample early.
  struct sigaction action = {};
  action.sa_handler = ignore_signal;  // no SA_RESTART: poll() returns EINTR
  struct sigaction previous = {};
  ASSERT_EQ(0, ::sigaction(SIGUSR1, &action, &previous));

  threads::stop_signal signal;
  std::string error;
  ASSERT_TRUE(signal.create(error)) << error;
  const pthread_t waiter = ::pthread_self();
  std::thread interrupter([waiter] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ::pthread_kill(waiter, SIGUSR1);
  });
  const auto before = std::chrono::steady_clock::now();
  EXPECT_EQ(threads::stop_signal::wait_result::timed_out, signal.wait_for(std::chrono::milliseconds(300)));
  EXPECT_GE(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(300));
  interrupter.join();
  ::sigaction(SIGUSR1, &previous, nullptr);
}
#endif
