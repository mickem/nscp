// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Unix half of the `system` and `user` nodes: uname(3) for the machine's
// identity and the utmp database (the source `who` reads) for logon sessions.

#include <sys/utsname.h>
#include <utmpx.h>

#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

#include "ncpa_sources.hpp"

namespace ncpa {
namespace sysinfo {

namespace {

// The CPU model as /proc/cpuinfo names it. uname() has no field for it on
// Linux - it repeats the machine architecture - and NCPA reports the model,
// so read the one place that has it. Absent (and left empty) on the BSDs.
std::string read_cpu_model() {
  std::ifstream cpuinfo("/proc/cpuinfo");
  if (!cpuinfo) return "";
  std::string line;
  while (std::getline(cpuinfo, line)) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = line.substr(0, colon);
    if (key.compare(0, 10, "model name") != 0) continue;
    std::string model = line.substr(colon + 1);
    const std::size_t first = model.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    return model.substr(first);
  }
  return "";
}

}  // namespace

system_info gather() {
  system_info out;
  struct utsname name = {};
  if (uname(&name) == 0) {
    out.system = name.sysname;
    out.node = name.nodename;
    out.release = name.release;
    out.version = name.version;
    out.machine = name.machine;
  }
  out.processor = read_cpu_model();

  const std::time_t now = std::time(nullptr);
  struct tm local = {};
  if (localtime_r(&now, &local) != nullptr && local.tm_zone != nullptr) out.timezone = local.tm_zone;
  return out;
}

std::vector<std::string> logged_on_users() {
  std::vector<std::string> out;
  setutxent();
  for (struct utmpx *entry = getutxent(); entry != nullptr; entry = getutxent()) {
    // Only real interactive logins, the same filter `who` applies - otherwise
    // boot records and runlevel changes would be counted as users.
    if (entry->ut_type != USER_PROCESS) continue;
    if (entry->ut_user[0] == '\0') continue;
    out.emplace_back(entry->ut_user, strnlen(entry->ut_user, sizeof(entry->ut_user)));
  }
  endutxent();
  return out;
}

}  // namespace sysinfo
}  // namespace ncpa
