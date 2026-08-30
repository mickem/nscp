// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#define BUFF_SIZE 4096

// Upper bound on captured child output; see the Unix launcher for the rationale.
// Past the cap we keep reading (so the child never blocks on a full pipe and the
// timeout stays enforceable) but discard the excess.
#define MAX_OUTPUT_BYTES (8u * 1024u * 1024u)

#include <NSCAPI.h>
#include <win/tool-helper.h>

#include <boost/thread.hpp>
#include <bytes/buffer.hpp>
#include <bytes/char_buffer.hpp>
#include <error/error.hpp>
#include <handle.hpp>
#include <iostream>
#include <nscapi/macros.hpp>
#include <process/argv_quote.hpp>
#include <process/execute_process.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <win/sysinfo/win_sysinfo.hpp>
#include <win/userenv.hpp>

void kill_process_tree(const DWORD parent_pid) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 entry;
  entry.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(snapshot, &entry)) {
    do {
      if (entry.th32ParentProcessID == parent_pid) {
        kill_process_tree(entry.th32ProcessID);  // Recursively kill children
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
        if (process) {
          TerminateProcess(process, 5);
          CloseHandle(process);
        }
      }
    } while (Process32Next(snapshot, &entry));
  }
  CloseHandle(snapshot);

  HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, parent_pid);
  if (process) {
    TerminateProcess(process, 5);
    CloseHandle(process);
  }
}

typedef hlp::buffer<char> buffer_type;

// The truncation marker and the content ceiling that leaves room for it, so the
// captured string is a strict <= MAX_OUTPUT_BYTES bound (marker included).
static const char kOutputTruncMarker[] = "\n[output truncated]";
static const std::size_t kOutputContentCap = MAX_OUTPUT_BYTES - (sizeof(kOutputTruncMarker) - 1);

struct generic_closer {
  static void close(HANDLE handle) { ::CloseHandle(handle); }
};
typedef hlp::handle<HANDLE, generic_closer> generic_handle;

struct env_closer {
  static void close(LPVOID handle) { ::DestroyEnvironmentBlock(handle); }
};

typedef hlp::handle<LPVOID, env_closer> env_handle;

struct impersonator {
  bool active;
  explicit impersonator(HANDLE token) : active(false) {
    if (ImpersonateLoggedOnUser(token)) {
      active = true;
    }
  }
  ~impersonator() { close(); }

  void close() {
    if (active) {
      RevertToSelf();
    }
    active = false;
  }

  bool isActive() const { return active; }
};

static std::string readFromFile(buffer_type &buffer, const HANDLE file_handle) {
  DWORD dwRead = 0;
  std::string str;
  const DWORD chunk_size = static_cast<DWORD>(buffer.size()) - 10;
  do {
    const DWORD retval = ReadFile(file_handle, buffer, chunk_size, &dwRead, nullptr);
    if (retval == 0 || dwRead <= 0 || dwRead > chunk_size) return str;
    buffer[dwRead] = 0;
    str += buffer;
  } while (dwRead == chunk_size);
  return str;
}

boost::timed_mutex mutex_;
std::list<HANDLE> pids_;

void process::kill_all() {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) return;
  for (const HANDLE &h : pids_) {
    TerminateProcess(h, 5);
  }
}

void register_proc(HANDLE hProcess) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return;
  pids_.push_back(hProcess);
}
void remove_proc(HANDLE process) {
  const boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
  if (!lock.owns_lock()) return;
  pids_.remove_if([process](HANDLE other) { return other == process; });
}
int process::execute_process(const exec_arguments &args, std::string &output) {
  generic_handle hChildOutR, hChildOutW, hChildInR, hChildInW;
  generic_handle pHandle;

  if (!args.session.empty()) {
    if (!windows::winapi::WTSQueryUserToken(windows::winapi::WTSGetActiveConsoleSessionId(), pHandle.ref())) {
      output = "Failed to WTSQueryUserToken: " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
  } else if (!args.user.empty()) {
    generic_handle tmpHandle;
    if (!LogonUser(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(), utf8::cvt<std::wstring>(args.password).c_str(),
                   LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, tmpHandle.ref())) {
      output = "Failed to login as " + args.user + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }

    if (!DuplicateTokenEx(tmpHandle, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation, TokenPrimary, pHandle.ref())) {
      output = "Failed to duplicate token for " + args.user + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
  }

  SECURITY_ATTRIBUTES sec;
  sec.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec.bInheritHandle = FALSE;
  sec.lpSecurityDescriptor = nullptr;
  if (!args.fork) {
    sec.bInheritHandle = TRUE;
    CreatePipe(hChildInR.ref(), hChildInW.ref(), &sec, 0);
    CreatePipe(hChildOutR.ref(), hChildOutW.ref(), &sec, 0);
  }

  STARTUPINFO si;
  ZeroMemory(&si, sizeof(STARTUPINFOW));
  si.cb = sizeof(STARTUPINFOW);
  if (args.fork) {
    si.dwFlags = STARTF_USESHOWWINDOW;
  } else {
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hChildInR.get();
    si.hStdOutput = hChildOutW.get();
    si.hStdError = hChildOutW.get();
  }
  si.wShowWindow = SW_HIDE;
  if (args.display) si.wShowWindow = SW_SHOW;

  // Build the command line. If the caller supplied an argv vector we lock the
  // executable via lpApplicationName and produce a properly-escaped command
  // line so CreateProcess cannot reinterpret token boundaries: a single argv
  // element that contains spaces stays a single argv element. If argv is
  // empty we fall back to the legacy single-string command, which means the
  // operator is responsible for any quoting.
  std::wstring app_name_storage;
  LPCWSTR lpApplicationName = nullptr;
  std::wstring cmd_line_w;
  if (!args.argv.empty()) {
    app_name_storage = utf8::cvt<std::wstring>(args.argv[0]);
    lpApplicationName = app_name_storage.c_str();
    cmd_line_w = process::build_command_line_w(args.argv);
  } else {
    cmd_line_w = utf8::cvt<std::wstring>(args.command);
  }
  hlp::tchar_buffer tmpCmd(cmd_line_w);
  tmpCmd[cmd_line_w.length()] = 0;

  BOOL processOK = FALSE;
  PROCESS_INFORMATION pi;
  env_handle environment;
  DWORD creation_flags = 0;
  if (!args.fork) {
    creation_flags |= CREATE_NEW_PROCESS_GROUP;
  }
  if (pHandle) {
    impersonator imp(pHandle);
    if (!imp.isActive()) {
      output = "Failed to impersonate " + args.user + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }

    if (!CreateEnvironmentBlock(environment.ref(), pHandle.get(), FALSE)) {
      output = "Failed to create environment for " + args.user + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }

    processOK = CreateProcessAsUser(pHandle.get(), lpApplicationName, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE,
                                    creation_flags | CREATE_UNICODE_ENVIRONMENT, environment.get(), utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
    if (!processOK) {
      imp.close();
      const DWORD error = GetLastError();
      if (error == ERROR_PRIVILEGE_NOT_HELD) {
        processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(),
                                            utf8::cvt<std::wstring>(args.password).c_str(), LOGON_WITH_PROFILE, lpApplicationName, tmpCmd.get(), creation_flags,
                                            nullptr, utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
      } else {
        if (error == ERROR_BAD_EXE_FORMAT) {
          output =
              "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
        } else {
          output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
        }
        return NSCAPI::query_return_codes::returnUNKNOWN;
      }
    }
  } else {
    processOK = CreateProcess(lpApplicationName, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE, creation_flags, nullptr,
                              utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
  }

  if (processOK) {
    DWORD state = 0;
    // Trace the spawn so an operator can correlate "spawn -> kill -> exit"
    // log entries when triaging a hung or runaway script. The full command
    // line was already traced by the caller (CheckExternalScripts).
    // Effective timeout: a caller-supplied 0 falls back to 30s. Compute it once
    // so the deadline and every message that reports it agree (previously the
    // deadline used the 30s fallback while the log lines still printed
    // "timeout=0s").
    const unsigned int effective_timeout = args.timeout > 0 ? args.timeout : 30;
    NSC_TRACE_ENABLED() {
      NSC_TRACE_MSG("Spawned external script: alias='" + args.alias + "' pid=" + str::xtos(pi.dwProcessId) + " timeout=" + str::xtos(effective_timeout) +
                    "s fork=" + (args.fork ? "true" : "false"));
    }
    if (args.fork) {
      output = "Command started successfully";
      return NSCAPI::query_return_codes::returnOK;
    }
    register_proc(pi.hProcess);
    DWORD dwAvail = 0;
    std::string str;
    buffer_type buffer(BUFF_SIZE);
    // Bound the wait by wall-clock time, not iteration count. The previous loop
    // ran a fixed `timeout * 10` iterations: a child that always had output
    // pending never entered the `dwAvail == 0` wait branch, so it burned through
    // every iteration in microseconds and returned with `state` still at its
    // initial value - skipping the timeout/kill block entirely and leaking a
    // still-running (now unwaited) process. `while (1) echo x` in a script was
    // an unkillable per-invocation orphan. Track elapsed time instead, and treat
    // "deadline reached, process still alive" as the timeout path.
    //
    // GetTickCount (not GetTickCount64) so this keeps compiling on the XP
    // toolset (v141_xp / NTDDI_VERSION=0x0501); GetTickCount64 needs Vista+.
    // Its 32-bit millisecond counter wraps every ~49.7 days, but the unsigned
    // subtraction `GetTickCount() - start_ms` yields the correct elapsed time
    // across a single wrap, so a bounded timeout is measured correctly.
    const DWORD start_ms = GetTickCount();
    const DWORD timeout_ms = static_cast<DWORD>(effective_timeout) * 1000u;
    state = WAIT_TIMEOUT;  // "not yet observed to have exited"
    for (;;) {
      if (!::PeekNamedPipe(hChildOutR.get(), nullptr, 0, nullptr, &dwAvail, nullptr)) {
        // Pipe broke (write end closed / child gone). Resolve the real process
        // state so a genuinely-exited child is reaped rather than killed.
        state = WaitForSingleObject(pi.hProcess, 0);
        break;
      }
      if (dwAvail > 0) {
        const std::string chunk = readFromFile(buffer, hChildOutR.get());
        // Append up to the cap; past it drop the excess but keep draining so the
        // child never blocks on a full pipe.
        if (str.size() < kOutputContentCap) {
          str.append(chunk, 0, kOutputContentCap - str.size());
          if (str.size() >= kOutputContentCap) str.append(kOutputTruncMarker);
        }
        // Drained a chunk; re-check the clock before looping so a chatty child
        // cannot hold us here past the deadline.
        if (GetTickCount() - start_ms >= timeout_ms) {
          state = WAIT_TIMEOUT;
          break;
        }
        continue;
      }
      // Nothing pending: wait briefly for either more output or exit.
      state = WaitForSingleObject(pi.hProcess, 100);
      if (state != WAIT_TIMEOUT) {
        break;  // process exited; final drain happens below
      }
      if (GetTickCount() - start_ms >= timeout_ms) {
        state = WAIT_TIMEOUT;
        break;
      }
    }
    hChildInW.close();
    hChildInR.close();
    hChildOutW.close();

    dwAvail = 0;
    if (::PeekNamedPipe(hChildOutR.get(), nullptr, 0, nullptr, &dwAvail, nullptr) && dwAvail > 0) {
      const std::string chunk = readFromFile(buffer, hChildOutR.get());
      if (str.size() < kOutputContentCap) {
        str.append(chunk, 0, kOutputContentCap - str.size());
        if (str.size() >= kOutputContentCap) str.append(kOutputTruncMarker);
      }
    }
    output = utf8::cvt<std::string>(utf8::from_encoding(str, args.encoding));

    remove_proc(pi.hProcess);
    CloseHandle(pi.hThread);
    if (state == WAIT_TIMEOUT) {
      // Internal `timeout=` exceeded. Try a graceful CTRL-C first, then fall
      // back to TerminateProcess. Previously this path was effectively
      // invisible: the message went to the user-visible output only and the
      // tree-kill path used std::cout which is lost when running as a
      // service. Surface it via the proper log so operators can see when
      // NSClient++'s own timeout fired vs. some upstream cutoff.
      NSC_LOG_ERROR("External script '" + args.alias + "' (pid=" + str::xtos(pi.dwProcessId) + ") exceeded timeout=" + str::xtos(effective_timeout) +
                    "s; sending CTRL+BREAK");
      // The child is launched into its own process group (CREATE_NEW_PROCESS_GROUP
      // when !fork), so a group-targeted CTRL+C is discarded - only CTRL+BREAK can
      // be delivered to another group. Use it here; if we share no console (the
      // service case) the call simply fails and we fall through to the hard kill.
      if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId)) {
        if (WaitForSingleObject(pi.hProcess, 2000) == WAIT_OBJECT_0) {
          state = WAIT_OBJECT_0;
          NSC_TRACE_ENABLED() { NSC_TRACE_MSG("External script '" + args.alias + "' (pid=" + str::xtos(pi.dwProcessId) + ") exited after CTRL+BREAK"); }
        }
      }
      if (state == WAIT_TIMEOUT) {
        if (args.kill_tree) {
          NSC_LOG_ERROR("External script '" + args.alias + "' (pid=" + str::xtos(pi.dwProcessId) + ") did not exit; killing process tree");
          kill_process_tree(pi.dwProcessId);
        } else {
          NSC_LOG_ERROR("External script '" + args.alias + "' (pid=" + str::xtos(pi.dwProcessId) + ") did not exit; calling TerminateProcess");
          TerminateProcess(pi.hProcess, 5);
        }
        output = "Command " + args.alias + " didn't terminate within the timeout period " + str::xtos(effective_timeout) + "s";
        return NSCAPI::query_return_codes::returnUNKNOWN;
      }
    }
    NSCAPI::nagiosReturn result;
    DWORD exit_code = 0;
    if (GetExitCodeProcess(pi.hProcess, &exit_code) == 0) {
      output = "Failed to get commands " + args.alias + " return code: " + error::lookup::last_error();
      result = NSCAPI::query_return_codes::returnUNKNOWN;
    } else {
      if (exit_code == 0) {
        result = NSCAPI::query_return_codes::returnOK;
      } else if (exit_code == 1) {
        result = NSCAPI::query_return_codes::returnWARN;
      } else if (exit_code == 2) {
        result = NSCAPI::query_return_codes::returnCRIT;
      } else {
        result = NSCAPI::query_return_codes::returnUNKNOWN;
      }
    }
    CloseHandle(pi.hProcess);
    return result;
  }
  DWORD error = GetLastError();
  if (error == ERROR_BAD_EXE_FORMAT) {
    output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
  } else {
    output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
  }
  return NSCAPI::query_return_codes::returnUNKNOWN;
}