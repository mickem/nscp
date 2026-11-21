// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifndef _WIN32
#include <fcntl.h>
#endif

namespace socket_helpers {

// Keep a socket out of every child the agent starts. asio opens sockets
// without SOCK_CLOEXEC, so a listener is inherited by whatever is forked while
// it is open - journalctl, dpkg-query, an external script - and stays bound in
// that child after the agent has closed its own copy. A settings reload closes
// the listener and binds the port again a moment later; with such a child
// still running the bind fails with "Address already in use" and the server
// stays down until the service is restarted.
//
// Set right after open()/accept(). A fork between the two still slips through,
// which is why the launchers close what they inherit as well (see
// process/execute_process_unix.cpp and CheckSystemUnix/exec_command.cpp).
// Windows sockets are not handed to children by the launchers used there.
template <class NativeHandle>
inline void set_close_on_exec(NativeHandle handle) {
#ifndef _WIN32
  const int flags = ::fcntl(handle, F_GETFD);
  if (flags != -1) ::fcntl(handle, F_SETFD, flags | FD_CLOEXEC);
#else
  (void)handle;
#endif
}

}  // namespace socket_helpers
