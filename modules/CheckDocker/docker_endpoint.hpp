// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <string>

namespace docker_checks {

// ASCII case-insensitive prefix test. Deliberately hand-rolled rather than
// boost::algorithm::istarts_with: this header is kept dependency-free so it can
// be unit-tested without the module's protobuf and filter dependencies, and
// pipe path prefixes are pure ASCII.
inline bool starts_with_ci(const std::string &value, const std::string &prefix) {
  if (value.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); i++) {
    char a = value[i];
    char b = prefix[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

// The docker daemon endpoint this platform uses when `--host` is not given.
inline std::string default_docker_endpoint() {
#ifdef WIN32
  return "\\\\.\\pipe\\docker_engine";
#else
  return "/var/run/docker.sock";
#endif
}

// True when `host` names a docker endpoint on this machine.
//
// `--host` is a check argument, so it arrives from whoever can run the check -
// over REST that is any caller holding `queries.execute`, which the stock
// `monitoring` and `client` roles both have. It is handed to the "pipe"
// transport, which on Windows calls CreateFileA on it. A UNC target such as
// \\attacker\pipe\x makes Windows open an SMB session to an arbitrary host
// using the service account's credentials, exposing them for capture or relay
// (NSClient++ commonly runs as LocalSystem). SECURITY_SQOS_PRESENT |
// SECURITY_IDENTIFICATION on that call limits what a hostile pipe server can
// impersonate, but does nothing to prevent the outbound authentication - so
// the endpoint has to be constrained here, before the connect.
//
// On failure `error` explains what was rejected.
//
// Header-only so it can be unit-tested without the module's protobuf/filter
// dependencies; the platform branch is compile-time, so a given build only
// ever exercises one of them.
inline bool is_local_docker_endpoint(const std::string &host, std::string &error) {
  if (host.empty()) {
    error = "No docker endpoint given";
    return false;
  }
#ifdef WIN32
  // Accept only the local-device pipe namespace: \\.\pipe\<name> or the
  // equivalent \\?\pipe\<name>. A UNC server name in that slot (\\host\pipe\x)
  // is what turns this into an outbound SMB authentication, so the third
  // character has to be a literal '.' or '?', and <name> must not contain
  // further separators that could walk back out of the pipe namespace.
  //
  // Matched case-insensitively because Win32 path and pipe names are:
  // \\.\Pipe\docker_engine names the same object as \\.\pipe\docker_engine and
  // is a perfectly ordinary thing for an operator to type. This only ever
  // widens what is accepted into the local namespace - anything that does not
  // match one of these two prefixes is still refused outright.
  const std::string prefix_dot = "\\\\.\\pipe\\";
  const std::string prefix_q = "\\\\?\\pipe\\";
  // Length of the prefix that actually matched, so this stays correct if the
  // two ever stop being the same length.
  std::size_t prefix_len = 0;
  if (starts_with_ci(host, prefix_dot)) {
    prefix_len = prefix_dot.size();
  } else if (starts_with_ci(host, prefix_q)) {
    prefix_len = prefix_q.size();
  } else {
    error = "Refusing docker endpoint '" + host +
            "': only a local named pipe (\\\\.\\pipe\\<name> or \\\\?\\pipe\\<name>) is allowed. A UNC path would make this host authenticate to a "
            "remote server over SMB.";
    return false;
  }
  const std::string name = host.substr(prefix_len);
  if (name.empty() || name.find('\\') != std::string::npos || name.find('/') != std::string::npos) {
    error = "Refusing docker endpoint '" + host + "': the pipe name must be a single path-separator-free component.";
    return false;
  }
  return true;
#else
  // Unix domain socket path. The "pipe" transport is Windows-only, so this
  // never reaches CreateFileA - but keep the endpoint local and unambiguous
  // rather than letting a relative or traversing path through.
  if (host[0] != '/') {
    error = "Refusing docker endpoint '" + host + "': expected an absolute path to the docker socket.";
    return false;
  }
  if (host.find("/../") != std::string::npos || (host.size() >= 3 && host.compare(host.size() - 3, 3, "/..") == 0)) {
    error = "Refusing docker endpoint '" + host + "': path traversal is not allowed.";
    return false;
  }
  return true;
#endif
}

}  // namespace docker_checks
