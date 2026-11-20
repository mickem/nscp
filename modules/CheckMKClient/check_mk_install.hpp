// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The pure part of `nscp mk install`: where the agent's settings live and how
// a `--local` / `--mrpe` argument becomes a settings entry. Kept apart from the
// module so it can be tested without a core.

#include <cctype>
#include <stdexcept>
#include <string>

namespace check_mk_install {

// CheckMKServer's settings root and the two sections default_check_mk.lua
// reads its local checks and MRPE entries from.
static const char *const server_path = "/settings/check_mk/server";
static const char *const local_path = "/settings/check_mk/server/local";
static const char *const mrpe_path = "/settings/check_mk/server/mrpe";

// The modules the default agent dump needs: the listener itself, CheckSystem
// for the check_mk header, mem, ps, services and uptime, and CheckDisk for df.
static const char *const required_modules[] = {"CheckMKServer", "CheckSystem", "CheckDisk"};

// The value the Lua script parses (`parse_spec`): `command=<nscp command> [args...]`.
static const char *const command_prefix = "command=";

inline std::string trim(const std::string &s) {
  std::size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

struct entry {
  std::string name;     // the Checkmk service name, the settings key
  std::string command;  // `<nscp command> [args...]`, without the prefix
  std::string value() const { return command_prefix + command; }
};

// `Service name=check_cpu warn=load>80` -> {"Service name", "check_cpu warn=load>80"}.
// The name ends at the first '=', so the command may carry `key=value`
// arguments. A command already written as `command=...` (the form in the ini
// file) is accepted too. Throws std::invalid_argument, naming `option`, when
// either half is missing.
inline entry parse_entry(const std::string &spec, const std::string &option) {
  const std::size_t eq = spec.find('=');
  if (eq == std::string::npos) throw std::invalid_argument("--" + option + " expects \"<service name>=<command> [arguments]\", got: " + spec);
  entry e;
  e.name = trim(spec.substr(0, eq));
  e.command = trim(spec.substr(eq + 1));
  if (e.command.compare(0, std::string(command_prefix).size(), command_prefix) == 0) e.command = trim(e.command.substr(std::string(command_prefix).size()));
  if (e.name.empty()) throw std::invalid_argument("--" + option + " is missing the service name: " + spec);
  if (e.command.empty()) throw std::invalid_argument("--" + option + " is missing the command for \"" + e.name + "\"");
  // A local check's name is quoted in <<<local>>>, but an MRPE line has no
  // quoting: Checkmk reads the name up to the first space, so `Uptime Check`
  // would make `Check` the return code and the line unparseable.
  if (option == "mrpe") {
    for (const char c : e.name) {
      if (std::isspace(static_cast<unsigned char>(c))) {
        std::string suggestion = e.name;
        for (char &s : suggestion)
          if (std::isspace(static_cast<unsigned char>(s))) s = '_';
        throw std::invalid_argument("--mrpe service names cannot contain spaces (Checkmk reads the name up to the first one): use \"" + suggestion +
                                    "\" instead of \"" + e.name + "\", or add it with --local");
      }
    }
  }
  return e;
}

// The command half of a stored value, for display: `command=check_cpu` -> `check_cpu`.
inline std::string command_of(const std::string &value) {
  const std::string v = trim(value);
  if (v.compare(0, std::string(command_prefix).size(), command_prefix) == 0) return trim(v.substr(std::string(command_prefix).size()));
  return v;
}

// True when `allowed_hosts` lets nothing but the local machine connect, which
// is the shipped default and never what a Checkmk site on another host needs.
inline bool loopback_only(const std::string &allowed_hosts) {
  std::string rest = allowed_hosts;
  bool any = false;
  while (!rest.empty()) {
    const std::size_t comma = rest.find(',');
    const std::string host = trim(rest.substr(0, comma));
    rest = comma == std::string::npos ? std::string() : rest.substr(comma + 1);
    if (host.empty()) continue;
    any = true;
    if (host != "127.0.0.1" && host != "::1" && host != "localhost") return false;
  }
  return any;
}

}  // namespace check_mk_install
