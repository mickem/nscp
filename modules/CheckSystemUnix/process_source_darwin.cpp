// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_process's reader on Darwin, from libproc and sysctl.
//
// What an unprivileged caller gets differs by source, and the fields follow
// that line:
//  - The kernel's process table (sysctl KERN_PROC, what ps reads) and
//    proc_pidpath answer for every process: pid, parent, owner, BSD state,
//    start time and executable path. (proc_pidinfo's BSD info would carry the
//    same, but is refused for other users' processes.)
//  - proc_pidinfo(PROC_PIDTASKINFO) answers for the caller's own processes
//    (and for all of them as root): memory, faults, CPU time and the running
//    thread count. For anyone else's the counters are marked unreadable and
//    render as unknown - never as 0.
//  - The argument vector (KERN_PROCARGS2) follows the same rule; an
//    unreadable command line stays empty, as a kernel thread's does on Linux.

#include <libproc.h>
#include <mach/mach_time.h>
#include <sys/proc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "check_process.h"
#include "mach_stats_darwin.h"

namespace check_proc {
namespace check_proc_filter {

namespace {

unsigned long long mach_to_ns(const unsigned long long ticks) { return mach_stats::mach_ticks_to_ns(ticks); }

std::string basename_of(const std::string &path) {
  const std::size_t pos = path.find_last_of('/');
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

// The argument vector, space-joined. KERN_PROCARGS2 lays it out as argc, the
// executable path, NUL padding, then argc NUL-terminated strings (and the
// environment after them, which is never read). Empty when the kernel refuses
// (another user's process) or the process has gone.
std::string read_command_line(const pid_t pid) {
  static const int argmax = [] {
    int value = 0;
    int mib[2] = {CTL_KERN, KERN_ARGMAX};
    std::size_t length = sizeof(value);
    if (sysctl(mib, 2, &value, &length, nullptr, 0) != 0 || value <= 0) value = 256 * 1024;
    return value;
  }();
  // One buffer per thread, reused: kern.argmax is 1 MiB, and allocating and
  // zeroing that for every process on the host made a full scan cost the best
  // part of a gigabyte of memset.
  thread_local std::vector<char> buffer;
  if (buffer.size() < static_cast<std::size_t>(argmax)) buffer.resize(static_cast<std::size_t>(argmax));
  int mib[3] = {CTL_KERN, KERN_PROCARGS2, pid};
  std::size_t length = buffer.size();
  if (sysctl(mib, 3, buffer.data(), &length, nullptr, 0) != 0 || length <= sizeof(int)) return "";

  int argc = 0;
  std::memcpy(&argc, buffer.data(), sizeof(argc));
  const char *p = buffer.data() + sizeof(int);
  const char *const end = buffer.data() + length;
  // Skip the executable path, then the padding after it.
  while (p < end && *p != '\0') ++p;
  while (p < end && *p == '\0') ++p;

  std::string result;
  for (int i = 0; i < argc && p < end; ++i) {
    const char *const start = p;
    while (p < end && *p != '\0') ++p;
    if (!result.empty()) result += " ";
    result.append(start, static_cast<std::size_t>(p - start));
    ++p;
  }
  return result;
}

// The BSD process state as the Linux letter check_process already knows.
// Darwin marks nearly every live process SRUN whatever its threads are doing,
// so running vs sleeping comes from the task info's count of running threads
// and is unknown ('?') without it.
char state_letter(const int bsd_status, const bool have_task, const int running_threads) {
  switch (bsd_status) {
    case SZOMB:
      return 'Z';
    case SSTOP:
      return 'T';
    case SRUN:
    case SSLEEP:
      if (have_task) return running_threads > 0 ? 'R' : 'S';
      return bsd_status == SSLEEP ? 'S' : '?';
    default:
      return '?';
  }
}

bool read_process_info(const struct kinfo_proc &kp, const bool resolve_owner, filter_obj &info) {
  const pid_t pid = kp.kp_proc.p_pid;
  info.pid = pid;
  info.ppid = static_cast<int>(kp.kp_eproc.e_ppid);
  info.uid = static_cast<long long>(kp.kp_eproc.e_pcred.p_ruid);
  if (resolve_owner) info.username = lookup_username(info.uid);

  char path[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(pid, path, sizeof(path)) > 0) {
    info.filename = path;
    info.exe = basename_of(info.filename);
  } else {
    // No image (kernel_task) or already exiting: the accounting name (up to
    // 16 characters) is what is left.
    info.exe = std::string(kp.kp_proc.p_comm);
  }
  info.command_line = read_command_line(pid);

  // Start time: whole seconds for creation, and microseconds as the value
  // that identifies this incarnation of the pid for delta=true.
  const struct timeval started = kp.kp_proc.p_starttime;
  info.creation_time = static_cast<unsigned long long>(started.tv_sec);
  info.start_time_jiffies = static_cast<unsigned long long>(started.tv_sec) * 1000000ull + static_cast<unsigned long long>(started.tv_usec);
  const unsigned long long now = static_cast<unsigned long long>(::time(nullptr));
  info.elapsed = now > info.creation_time ? now - info.creation_time : 0;

  // macOS keeps no per-process peak sizes.
  info.has_peaks = false;

  struct proc_taskinfo task;
  std::memset(&task, 0, sizeof(task));
  const bool have_task = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task, PROC_PIDTASKINFO_SIZE) == PROC_PIDTASKINFO_SIZE;
  info.has_task_info = have_task;
  if (have_task) {
    info.virtual_size = task.pti_virtual_size;
    info.working_set = task.pti_resident_size;
    // Pageins are the faults that had to read from disk, the counterpart of
    // the major faults Linux reports here.
    info.page_faults = static_cast<unsigned long long>(task.pti_pageins);
    // The raw counters are nanoseconds, the unit read_cpu_capacity uses.
    info.user_time_raw = mach_to_ns(task.pti_total_user);
    info.kernel_time_raw = mach_to_ns(task.pti_total_system);
    info.user_time = info.user_time_raw / 1000000000ull;
    info.kernel_time = info.kernel_time_raw / 1000000000ull;
    info.total_time = info.user_time + info.kernel_time;
  }

  const int status = kp.kp_proc.p_stat;
  info.proc_state = state_letter(status, have_task, have_task ? task.pti_numrunning : 0);
  // Started means alive and able to run: not a zombie, not stopped, not
  // still being created.
  info.started = status == SRUN || status == SSLEEP;
  return true;
}

}  // namespace

std::vector<filter_obj> enumerate_processes(bool resolve_owner) {
  std::vector<filter_obj> result;
  for (const struct kinfo_proc &kp : mach_stats::read_all_processes()) {
    if (kp.kp_proc.p_pid < 0) continue;
    filter_obj info;
    if (!read_process_info(kp, resolve_owner, info)) continue;
    if (!info.exe.empty() || !info.command_line.empty()) result.push_back(info);
  }
  return result;
}

bool read_cpu_capacity(unsigned long long &capacity) {
  // Wall-clock nanoseconds times the online cores: what every core together
  // could have spent, in the unit the per-process counters above are in.
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cores < 1) cores = 1;
  capacity = mach_to_ns(mach_absolute_time()) * static_cast<unsigned long long>(cores);
  return true;
}

}  // namespace check_proc_filter
}  // namespace check_proc
