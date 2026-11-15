// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux reader for system.pressure: /proc/pressure/{cpu,memory,io} and the
// pgmajfault line of /proc/vmstat. A file that is not there (a kernel before
// 4.20, one built without PSI or booted with psi=0, a container that hides
// /proc/pressure) is skipped without a word: it is a property of the host,
// not an error, and it would otherwise be logged on every metrics round.

#include "pressure.h"
#include "procfs_linux.h"

namespace pressure {

sample read_pressure() {
  sample out;
  for (const char *name : {"cpu", "memory", "io"}) {
    const std::string content = procfs::read_file(std::string("/proc/pressure/") + name);
    if (content.empty()) continue;
    resource r;
    r.name = name;
    if (parse_psi(content, r)) out.resources.push_back(r);
  }
  unsigned long long faults = 0;
  if (parse_vmstat_value(procfs::read_file("/proc/vmstat"), "pgmajfault", faults)) {
    out.has_major_faults = true;
    out.major_faults = faults;
  }
  return out;
}

}  // namespace pressure
