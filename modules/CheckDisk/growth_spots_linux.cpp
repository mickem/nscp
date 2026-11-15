// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Linux growth spots: the temp directories, the log directories and the
// swap files listed in /proc/swaps (a swap partition takes no filesystem
// space and is left out). There is no shadow-copy storage to report.

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <str/utf8.hpp>
#include <string>
#include <vector>

#include "growth_spots.hpp"

namespace growth_spots {

namespace {

spot make(const std::string &path, const char *kind, const spot::type_t type) {
  spot s;
  s.path = path;
  s.kind = kind;
  s.type = type;
  return s;
}

}  // namespace

std::vector<spot> discover(const std::string &agent_log_dir) {
  std::vector<spot> out;
  const char *tmpdir = std::getenv("TMPDIR");
  if (tmpdir != nullptr && tmpdir[0] == '/') out.push_back(make(tmpdir, kind_temp, spot::directory));
  out.push_back(make("/tmp", kind_temp, spot::directory));
  out.push_back(make("/var/tmp", kind_temp, spot::directory));
  if (!agent_log_dir.empty()) out.push_back(make(agent_log_dir, kind_logs, spot::directory));
  out.push_back(make("/var/log", kind_logs, spot::directory));
  std::ifstream swaps("/proc/swaps");
  if (swaps) {
    for (const std::string &p : parse_swap_files(swaps)) out.push_back(make(p, kind_swap_file, spot::file));
  }
  return out;
}

bool file_size(const std::string &path, unsigned long long &size, std::string &error) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) {
    if (errno == ENOENT || errno == ENOTDIR) return false;
    error = "could not be read: " + utf8::utf8_from_native(std::strerror(errno));
    return true;
  }
  size = static_cast<unsigned long long>(st.st_size);
  return true;
}

void lower_priority() {
#ifdef SYS_ioprio_set
  // The idle I/O class: the walk's reads are served only when the disk has
  // nothing else to do. Constants from linux/ioprio.h, which not every libc
  // ships: IOPRIO_WHO_PROCESS = 1 (with 0, the calling thread), class IDLE =
  // 3 in the top bits.
  syscall(SYS_ioprio_set, 1, 0, 3 << 13);
#endif
#ifdef SYS_gettid
  // And the lowest CPU priority, for this thread only (Linux applies a nice
  // value set by thread id to that thread).
  setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 19);
#endif
}

bool paths_ignore_case() { return false; }

}  // namespace growth_spots
