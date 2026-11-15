// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Linux reader for the diagnostics process tables: /proc/<pid>/stat and
// /proc/<pid>/statm of every process, two small reads each, and nothing that
// needs privileges (no exe link, no cmdline).

#include <dirent.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "process_top.h"
#include "procfs_linux.h"

namespace process_top {

table read_processes() {
  // Closed however the loop is left: reading a process can throw.
  const std::unique_ptr<DIR, int (*)(DIR *)> proc(opendir("/proc"), &closedir);
  if (!proc) throw std::runtime_error("cannot read /proc");
  const long page = sysconf(_SC_PAGESIZE);
  const unsigned long long page_size = page > 0 ? static_cast<unsigned long long>(page) : 4096ULL;
  table out;
  while (const struct dirent *entry = readdir(proc.get())) {
    const std::string name = entry->d_name;
    if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
    const std::string base = "/proc/" + name;
    process_times p;
    // A process that exited between the listing and the read leaves empty
    // files: it is simply not in this sample.
    if (!parse_stat(procfs::read_file(base + "/stat"), p)) continue;
    if (!parse_statm(procfs::read_file(base + "/statm"), page_size, p)) continue;
    out[std::stoll(name)] = p;
  }
  if (out.empty()) throw std::runtime_error("no process could be read from /proc");
  return out;
}

}  // namespace process_top
