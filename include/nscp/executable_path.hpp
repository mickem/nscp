// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>

#ifdef WIN32
#include <win/shellapi.hpp>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>

#include <vector>
#else
#include <unistd.h>
#endif

// Where the running executable lives.
//
// Every path the agent resolves for itself hangs off this one answer:
// ${exe-path} and ${base-path} are literally its return value, and the
// prefix-relative defaults for modules, settings and logs are derived from it.
// So it lives in one place rather than in each caller - the service
// (path_manager) and the two client binaries (client_path_resolver) each used
// to carry their own copy, and a copy is what drifts.
//
// The three implementations are genuinely different system calls, not a
// portability wrapper around one:
//
//   Windows  GetModuleFileName, via shellapi.
//   Linux    readlink("/proc/self/exe"), the procfs symlink to the image.
//   macOS    _NSGetExecutablePath. Darwin has no procfs at all, so the Linux
//            branch does not merely return something different there - it
//            fails outright and the daemon silently falls back to whatever
//            directory launchd happened to start it in, which is "/". That is
//            how a macOS build loads no modules and finds no configuration.
//
// _NSGetExecutablePath fills the buffer with the path as invoked, which may be
// a symlink or contain "..", so it is canonicalised here; the Linux symlink is
// already resolved by the kernel.
namespace nscp {
namespace paths {

// The directory containing the running executable, or the process's initial
// working directory if the platform call fails (which it only can on a
// pathological system - the image the kernel loaded always has a name).
inline boost::filesystem::path executable_dir() {
#ifdef WIN32
  return shellapi::get_module_file_name();
#elif defined(__APPLE__)
  uint32_t size = 0;
  // First call reports the required size (and returns -1 because 0 is too
  // small); the second fills the buffer.
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buff(size + 1, '\0');
  if (_NSGetExecutablePath(buff.data(), &size) != 0) return boost::filesystem::initial_path();
  boost::system::error_code ec;
  const boost::filesystem::path exe = boost::filesystem::canonical(boost::filesystem::path(std::string(buff.data())), ec);
  if (ec) return boost::filesystem::path(std::string(buff.data())).parent_path();
  return exe.parent_path();
#else
  char buff[1024];
  const ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len == -1) return boost::filesystem::initial_path();
  buff[len] = '\0';
  return boost::filesystem::path(std::string(buff)).parent_path();
#endif
}

}  // namespace paths
}  // namespace nscp
