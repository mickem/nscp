/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/filesystem/operations.hpp>
#include <fstream>
#include <pid_file.hpp>

#ifdef WIN32
#include <process.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif
pidfile::pidfile(boost::filesystem::path const& rundir, std::string const& process_name)
    : filepath_((rundir.empty() ? get_default_rundir() : rundir) / (process_name + ".pid")) {}

pidfile::pidfile(boost::filesystem::path const& filepath) : filepath_(filepath) {}

pidfile::~pidfile() {
  try {
    remove();
  } catch (...) {
  }
}

bool pidfile::create(pid_t const pid) const {
#ifdef WIN32
  // pidfile is only used on POSIX in production (cli_parser.cpp guards the
  // call site with #ifndef WIN32). The Windows branch is kept so the class
  // still compiles for any out-of-tree consumer.
  remove();
  std::ofstream out(filepath_.string().c_str());
  out << pid;
  return out.good();
#else
  // Open with O_EXCL|O_NOFOLLOW so we never follow a planted symlink and
  // never overwrite an existing pid file. /var/run/nscp.pid is a predictable
  // path, so an unprivileged user could otherwise drop a symlink there
  // pointing at /etc/cron.d/foo (or anything else) and have us write the pid
  // bytes through it as root.
  //
  // O_EXCL means a stale pid file from a previous crashed instance will
  // cause a startup failure - that is the desired conservative behaviour;
  // operator must clean up explicitly so they notice. Mode 0644 because the
  // pid is read by service / monitoring tools that may not be root.
  const std::string path = filepath_.string();
  const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0644);
  if (fd < 0) {
    // Caller logs the failure via the surrounding exception path. We avoid
    // pulling the project logger into this header to keep it standalone.
    return false;
  }
  const std::string contents = std::to_string(static_cast<long long>(pid));
  ssize_t total = 0;
  while (total < static_cast<ssize_t>(contents.size())) {
    const ssize_t n = ::write(fd, contents.data() + total, contents.size() - total);
    if (n < 0) {
      if (errno == EINTR) continue;
      ::close(fd);
      ::unlink(path.c_str());
      return false;
    }
    total += n;
  }
  if (::close(fd) != 0) {
    ::unlink(path.c_str());
    return false;
  }
  return true;
#endif
}

bool pidfile::create() const { return create(get_pid()); }

pid_t pidfile::get_pid() const {
#ifdef WIN32
  return ::_getpid();
#else
  return ::getpid();
#endif
}

bool pidfile::remove() const { return boost::filesystem::remove(filepath_); }
