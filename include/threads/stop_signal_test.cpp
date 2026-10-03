// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <threads/stop_signal.hpp>

#ifndef WIN32
#include <fcntl.h>
#include <poll.h>
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
