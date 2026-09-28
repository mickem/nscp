// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <boost/asio.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/version.hpp>
// Boost 1.86 introduced the versioned headers; newer releases default to v2.
// This resolver uses v1's child and pipe API explicitly.
#if BOOST_VERSION >= 108600
#include <boost/process/v1.hpp>
#ifdef _WIN32
#include <boost/process/v1/windows.hpp>
#endif
#else
#include <boost/process.hpp>
#ifdef _WIN32
#include <boost/process/windows.hpp>
#endif
#endif
#include <algorithm>
#include <chrono>
#include <net/address_family.hpp>
#include <str/utf8.hpp>
#include <thread>

namespace check_net {
namespace radius {
struct resolved_host {
  boost::asio::ip::address address;
  std::string error;
};

inline resolved_host resolve_process(const boost::filesystem::path &executable, const std::vector<std::string> &args,
                                     std::chrono::steady_clock::time_point deadline) {
#if BOOST_VERSION >= 108600
  namespace bp = boost::process::v1;
#else
  namespace bp = boost::process;
#endif
  if (std::chrono::steady_clock::now() >= deadline) return {{}, "timeout"};
  bp::ipstream output;
#ifdef _WIN32
  std::vector<std::wstring> native_args;
  for (const auto &arg : args) native_args.push_back(utf8::cvt<std::wstring>(arg));
  bp::child child(executable.wstring(), bp::args(native_args), (bp::std_in < bp::null), (bp::std_out > output), (bp::std_err > bp::null),
                  bp::windows::create_no_window);
#else
  bp::child child(executable.string(), bp::args(args), (bp::std_in < bp::null), (bp::std_out > output), (bp::std_err > bp::null));
#endif
  // Boost.Process v1's timed waits are deprecated and change SIGCHLD handling on
  // POSIX. Poll only this child, preserving the agent's other child processes.
  while (child.running()) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      // Use the native termination call so child.wait() can still reap/wait for
      // actual exit (v1::child::terminate marks the child exited immediately).
#ifdef _WIN32
      if (!TerminateProcess(child.native_handle(), 1) && child.running()) throw std::runtime_error("Cannot stop hostname resolver");
#else
      if (::kill(child.id(), SIGKILL) != 0 && child.running()) throw std::runtime_error("Cannot stop hostname resolver");
#endif
      child.wait();
      return {{}, "timeout"};
    }
    std::this_thread::sleep_until(std::min(deadline, now + std::chrono::milliseconds(5)));
  }
  if (std::chrono::steady_clock::now() >= deadline) return {{}, "timeout"};
  if (child.exit_code() != 0) return {{}, "resolve_failed"};
  char line[128]{};
  output.getline(line, sizeof(line));
  if (output.fail()) return {{}, "resolve_failed"};
  std::string address_text(line);
  if (!address_text.empty() && address_text.back() == '\r') address_text.pop_back();
  boost::system::error_code error;
  const auto address = boost::asio::ip::make_address(address_text, error);
  if (error) return {{}, "resolve_failed"};
  return {address, ""};
}

inline resolved_host resolve_host(const std::string &host, net::address_family family, std::chrono::steady_clock::time_point deadline) {
  boost::system::error_code error;
  const auto numeric = boost::asio::ip::make_address(host, error);
  if (!error) {
    if ((family == net::address_family::ipv4 && !numeric.is_v4()) || (family == net::address_family::ipv6 && !numeric.is_v6())) return {{}, "resolve_failed"};
    return {numeric, ""};
  }
  // Launch this installed nscp directly, without a shell or a PATH lookup.
  // The child never receives the RADIUS shared secret or user credentials.
  try {
    return resolve_process(boost::dll::program_location(), {"--resolve-host", net::to_string(family), host}, deadline);
  } catch (const std::exception &) {
    return {{}, "resolve_failed"};
  }
}
}  // namespace radius
}  // namespace check_net
