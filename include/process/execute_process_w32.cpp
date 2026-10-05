// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#define BUFF_SIZE 4096

// Upper bound on captured child output; see the Unix launcher for the rationale.
// Past the cap we keep reading (so the child never blocks on a full pipe and the
// timeout stays enforceable) but discard the excess.
#define MAX_OUTPUT_BYTES (8u * 1024u * 1024u)

#include <NSCAPI.h>
#include <win/tool-helper.h>

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
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
#include <vector>
#include <win/sysinfo/win_sysinfo.hpp>
#include <win/userenv.hpp>

void kill_process_tree(const DWORD parent_pid) {
  // The snapshot fails with INVALID_HANDLE_VALUE (ERROR_BAD_LENGTH under
  // process churn), not NULL; closing that value terminates under strict
  // handle checking. Without a snapshot the children cannot be found, but
  // the parent below is still terminated.
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot != INVALID_HANDLE_VALUE) {
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
  }

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
// How long the post-wait drain may spend collecting what is still buffered.
// Generous for a process that has exited (the pipe is finite and this only
// reads what is already there), and short enough that a child still writing
// cannot hold the worker thread away from the kill that follows.
static const DWORD kFinalDrainBudgetMs = 2000;

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

// Reads at most `available` bytes - the count PeekNamedPipe just reported - in
// one bounded pass.
//
// The loop this replaces kept calling ReadFile as long as the previous read
// had filled the whole chunk, and ReadFile on a pipe blocks until at least one
// byte arrives. A script that wrote exact multiples of the chunk size and then
// stalled therefore parked the worker thread inside this function with no
// deadline check at all, past the wall-clock timeout, until the child wrote
// again or exited - one wedged worker per invocation, from anyone able to
// influence how much a script prints. Reading only what is known to be
// buffered can never block, and the caller re-peeks, so nothing is lost.
static std::string readFromFile(buffer_type &buffer, const HANDLE file_handle, const DWORD available) {
  const DWORD chunk_size = static_cast<DWORD>(buffer.size()) - 10;
  const DWORD to_read = available < chunk_size ? available : chunk_size;
  if (to_read == 0) return std::string();
  DWORD dwRead = 0;
  const DWORD retval = ReadFile(file_handle, buffer, to_read, &dwRead, nullptr);
  if (retval == 0 || dwRead == 0 || dwRead > to_read) return std::string();
  // Sized from dwRead, not read as a C string. A script writing UTF-16, or any
  // binary, produces NUL bytes, and terminating at the first one silently cut
  // the output at that byte - which for UTF-16 is usually the second one.
  return std::string(static_cast<const char *>(buffer), dwRead);
}

boost::timed_mutex mutex_;
std::list<HANDLE> pids_;

namespace {
// Absolute path of `app` when it is a bare file name that the system's
// executable search can find, and `app` unchanged otherwise.
//
// This is the second half of giving lpApplicationName the reach the command
// line already has. SearchPathW walks the same list CreateProcess walks when
// it parses a module name out of lpCommandLine - the directory the agent
// loaded from, the working directory, the system and Windows directories, then
// PATH - so `command = cmd.exe /c ...` and the `powershell.exe` /
// `cscript.exe` wrappings resolve wherever the agent was started, instead of
// only when its working directory happened to hold a copy.
//
// Deliberately confined to a name with no directory component. Anything
// carrying a folder has already been rooted at ${base-path} by
// resolve_application_path or names a location of its own, and running the
// search over it would re-introduce exactly the working-directory dependency
// that rooting removed.
//
// A name the search cannot place is handed on untouched: CreateProcess then
// fails the way it always did, and the operator gets the same error about the
// same string they configured.
std::string search_path_for(const std::string &app) {
  if (app.empty() || boost::filesystem::path(app).has_parent_path()) return app;
  const std::wstring name = utf8::cvt<std::wstring>(app);
  // Two calls: the first sizes the buffer (return value includes the NUL), the
  // second fills it (return value does not).
  const DWORD needed = SearchPathW(nullptr, name.c_str(), nullptr, 0, nullptr, nullptr);
  if (needed == 0) return app;
  std::vector<wchar_t> buffer(needed);
  const DWORD written = SearchPathW(nullptr, name.c_str(), nullptr, needed, buffer.data(), nullptr);
  if (written == 0 || written >= needed) return app;
  return utf8::cvt<std::string>(std::wstring(buffer.data(), written));
}

// Restrict what a child inherits to its own two pipe ends.
//
// Checks run concurrently, and CreateProcess with bInheritHandles=TRUE hands
// the child *every* inheritable handle in the service: while script A is being
// spawned, script B's stdio pipe ends exist and are inheritable, so A ends up
// holding B's stdout - it can read B's output and write a forged result into
// it. With `user =` configured the child is the lower-privileged account that
// sandbox was meant to contain. PROC_THREAD_ATTRIBUTE_HANDLE_LIST (Vista+)
// names exactly which handles cross, whatever else happens to be inheritable.
//
// The XP toolset (v141_xp, _WIN32_WINNT=0x0501) has none of this in its
// headers, so the types and constants are spelled out here and the three entry
// points are resolved at run time; on an OS without them the spawn falls back
// to being serialised under spawn_mutex_ with the pipe ends made
// non-inheritable again before the lock is released, so two concurrent spawns
// can never see each other's ends. Other inheritable handles still cross on
// that path, which is what the OS offers.
struct startupinfoex_compat {
  STARTUPINFOW StartupInfo;
  PVOID lpAttributeList;
};
const DWORD kExtendedStartupInfoPresent = 0x00080000;         // EXTENDED_STARTUPINFO_PRESENT
const DWORD_PTR kProcThreadAttributeHandleList = 0x00020002;  // PROC_THREAD_ATTRIBUTE_HANDLE_LIST

typedef BOOL(WINAPI *tInitializeProcThreadAttributeList)(PVOID lpAttributeList, DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize);
typedef BOOL(WINAPI *tUpdateProcThreadAttribute)(PVOID lpAttributeList, DWORD dwFlags, DWORD_PTR Attribute, PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue,
                                                 PSIZE_T lpReturnSize);
typedef VOID(WINAPI *tDeleteProcThreadAttributeList)(PVOID lpAttributeList);

struct proc_thread_attribute_api {
  tInitializeProcThreadAttributeList initialize;
  tUpdateProcThreadAttribute update;
  tDeleteProcThreadAttributeList remove;
  bool available() const { return initialize != nullptr && update != nullptr && remove != nullptr; }
};

const proc_thread_attribute_api &attribute_api() {
  // kernel32 is always mapped, so GetModuleHandle (no LoadLibrary, nothing to
  // free) is enough. Resolved once; a null triple means "pre-Vista".
  static const proc_thread_attribute_api api = [] {
    proc_thread_attribute_api a = {nullptr, nullptr, nullptr};
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != nullptr) {
      a.initialize = reinterpret_cast<tInitializeProcThreadAttributeList>(GetProcAddress(kernel32, "InitializeProcThreadAttributeList"));
      a.update = reinterpret_cast<tUpdateProcThreadAttribute>(GetProcAddress(kernel32, "UpdateProcThreadAttribute"));
      a.remove = reinterpret_cast<tDeleteProcThreadAttributeList>(GetProcAddress(kernel32, "DeleteProcThreadAttributeList"));
    }
    return a;
  }();
  return api;
}

// An attribute list carrying one PROC_THREAD_ATTRIBUTE_HANDLE_LIST entry. The
// handle array it points at must outlive the CreateProcess call, so it is
// owned here alongside the list.
struct inherit_list {
  std::vector<BYTE> storage;
  std::vector<HANDLE> handles;
  PVOID list;
  inherit_list() : list(nullptr) {}
  inherit_list(const inherit_list &) = delete;
  inherit_list &operator=(const inherit_list &) = delete;
  ~inherit_list() {
    if (list != nullptr) attribute_api().remove(list);
  }
  // False when the API is missing or the list could not be built; the caller
  // then takes the serialised fallback.
  bool build(HANDLE first, HANDLE second) {
    const proc_thread_attribute_api &api = attribute_api();
    if (!api.available()) return false;
    handles.push_back(first);
    handles.push_back(second);
    SIZE_T size = 0;
    api.initialize(nullptr, 1, 0, &size);
    if (size == 0) return false;
    storage.assign(size, 0);
    if (!api.initialize(storage.data(), 1, 0, &size)) return false;
    list = storage.data();
    if (!api.update(list, 0, kProcThreadAttributeHandleList, handles.data(), handles.size() * sizeof(HANDLE), nullptr, nullptr)) {
      api.remove(list);
      list = nullptr;
      return false;
    }
    return true;
  }
};

boost::mutex spawn_mutex_;

bool set_inheritable(HANDLE handle, bool inheritable) { return SetHandleInformation(handle, HANDLE_FLAG_INHERIT, inheritable ? HANDLE_FLAG_INHERIT : 0) != 0; }

// Takes the child-side pipe ends out of circulation again: explicitly once
// the spawn has returned, and from the destructor on every other way out of
// execute_process, so no early return leaves them inheritable while the
// fallback lock is already released. Runs once; reads the handles through
// their owners so a closed (null) one - or a fork spawn that never created
// pipes - is skipped rather than a stale value touched.
struct inherit_reset {
  generic_handle &first;
  generic_handle &second;
  bool done;
  inherit_reset(generic_handle &first_, generic_handle &second_) : first(first_), second(second_), done(false) {}
  inherit_reset(const inherit_reset &) = delete;
  inherit_reset &operator=(const inherit_reset &) = delete;
  ~inherit_reset() { run(); }
  void run() {
    if (done) return;
    done = true;
    if (first.get() != NULL) set_inheritable(first.get(), false);
    if (second.get() != NULL) set_inheritable(second.get(), false);
  }
};
}  // namespace

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

  // Both pipes are created non-inheritable. Only the two ends the child uses
  // (its stdin read end, its stdout/stderr write end) are marked inheritable,
  // and only immediately before the spawn (arm_inheritance below). The
  // parent's ends never cross into any child: with every end inheritable, a
  // concurrently spawned script inherited this one's read end and could read
  // - or, holding the write end too, forge - its output.
  SECURITY_ATTRIBUTES sec;
  sec.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec.bInheritHandle = FALSE;
  sec.lpSecurityDescriptor = nullptr;
  if (!args.fork) {
    if (!CreatePipe(hChildInR.ref(), hChildInW.ref(), &sec, 0) || !CreatePipe(hChildOutR.ref(), hChildOutW.ref(), &sec, 0)) {
      output = "Failed to create pipes for " + args.alias + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
  }

  // STARTUPINFOEX with the child's two pipe ends as the explicit inherit list.
  // When the attribute API is missing (pre-Vista) the plain STARTUPINFO is
  // passed and the spawn is serialised below instead.
  startupinfoex_compat siex;
  ZeroMemory(&siex, sizeof(siex));
  STARTUPINFOW &si = siex.StartupInfo;
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

  // With the API present the list is the protection, so a failure to build
  // it is a failed spawn rather than a silent fall back to full inheritance
  // (the serialised fallback below only protects when every spawn takes it).
  inherit_list inherit;
  bool restrict_inheritance = false;
  if (!args.fork && attribute_api().available()) {
    if (!inherit.build(hChildInR.get(), hChildOutW.get())) {
      output = "Failed to build the handle inherit list for " + args.alias + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
    restrict_inheritance = true;
    si.cb = sizeof(startupinfoex_compat);
    siex.lpAttributeList = inherit.list;
  }

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
    // Last line of defence for a batch target, independent of whatever the
    // caller validated. CreateProcess re-launches `cmd.exe /c <command line>`
    // for a .bat or .cmd, and cmd.exe treats a CR or LF as a statement
    // separator: everything after one is parsed as a fresh command, with no
    // quote to break out of. There is no legitimate reason for a newline
    // inside an argument to a batch file, so refuse rather than try to escape
    // it - `allow nasty characters` deliberately does not reach here, because
    // this is not about what the argument means to the script but about
    // whether it stays one argument at all.
    //
    // Judged on the configured argv[0], before the lookup below rewrites it:
    // neither resolve_application_path nor search_path_for touches the
    // extension, and refusing before anything is looked up keeps the guard
    // independent of whether the target could be located at all.
    if (process::is_batch_target(args.argv[0])) {
      for (const std::string &arg : args.argv) {
        if (arg.find_first_of("\r\n") != std::string::npos) {
          output = "Refusing to run " + args.alias +
                   ": an argument contains a line break and the command is a .bat/.cmd file, which Windows runs through cmd.exe - a line break there ends "
                   "the statement and everything after it is executed as a separate command.";
          return NSCAPI::query_return_codes::returnUNKNOWN;
        }
      }
    }

    // Locate the executable ourselves before naming it.
    //
    // lpApplicationName gets none of the lookup the command line gets: it is
    // resolved against the working directory of the *calling* process and
    // nothing else - not PATH, not the system directory, and not
    // lpCurrentDirectory below, which only sets where the child runs. So
    // `scripts\check_foo.bat` was found solely when the agent had been started
    // from the installation directory, and a bare `cmd.exe` solely when the
    // working directory happened to contain one. The legacy single-string form
    // has neither problem because CreateProcess does the search itself there;
    // these two steps are what give the argv path the same reach without
    // giving up the locked executable.
    std::vector<std::string> argv = args.argv;
    argv[0] = process::resolve_application_path(args.root_path, argv[0]);
    argv[0] = search_path_for(argv[0]);
    // argv[0] goes into the command line as well, not just into
    // lpApplicationName. Windows runs a .bat by handing the command line to
    // cmd.exe, which resolves the script a second time out of that string; a
    // command line still carrying the configured `scripts/check_foo.bat` puts
    // cmd in the same position the launcher was just taken out of (and, with a
    // forward slash, has it read `/check_foo.bat` as a switch).
    app_name_storage = utf8::cvt<std::wstring>(argv[0]);
    lpApplicationName = app_name_storage.c_str();
    cmd_line_w = process::build_command_line_w(argv);
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
  if (restrict_inheritance) {
    creation_flags |= kExtendedStartupInfoPresent;
  }
  // Without an inherit list, two spawns must not overlap: a spawn's child-side
  // ends are inheritable only while it holds this lock, from arm_inheritance()
  // just before its CreateProcess call until reset_inherit runs after it. The
  // lock is taken before the ends are marked, never after - otherwise a spawn
  // already inside CreateProcess under the lock would inherit them.
  boost::unique_lock<boost::mutex> spawn_lock(spawn_mutex_, boost::defer_lock);
  // Declared after the lock so it runs before the lock is released on any
  // early return below.
  inherit_reset reset_inherit(hChildInR, hChildOutW);
  const auto arm_inheritance = [&]() -> bool {
    if (args.fork) return true;
    if (!restrict_inheritance && !spawn_lock.owns_lock()) spawn_lock.lock();
    return set_inheritable(hChildInR.get(), true) && set_inheritable(hChildOutW.get(), true);
  };
  // CreateProcessWithLogonW runs through the secondary logon service and
  // takes no inherit list; it duplicates only the std handles into the child.
  const DWORD logon_creation_flags = creation_flags & ~kExtendedStartupInfoPresent;
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

    if (!arm_inheritance()) {
      output = "Failed to prepare pipes for " + args.alias + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
    processOK = CreateProcessAsUser(pHandle.get(), lpApplicationName, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE,
                                    creation_flags | CREATE_UNICODE_ENVIRONMENT, environment.get(), utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
    if (!processOK) {
      imp.close();
      const DWORD error = GetLastError();
      if (error == ERROR_PRIVILEGE_NOT_HELD) {
        STARTUPINFOW logon_si = si;
        logon_si.cb = sizeof(STARTUPINFOW);
        processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(),
                                            utf8::cvt<std::wstring>(args.password).c_str(), LOGON_WITH_PROFILE, lpApplicationName, tmpCmd.get(),
                                            logon_creation_flags, nullptr, utf8::cvt<std::wstring>(args.root_path).c_str(), &logon_si, &pi);
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
    if (!arm_inheritance()) {
      output = "Failed to prepare pipes for " + args.alias + ": " + error::lookup::last_error();
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
    processOK = CreateProcess(lpApplicationName, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE, creation_flags, nullptr,
                              utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
  }
  // Captured here: the reset and unlock below may touch the thread's last
  // error, and the failure report at the end reads this local instead.
  const DWORD spawn_error = GetLastError();
  // The child has its copies now (or was never created); nothing spawned from
  // here on may pick these up. Done before the fallback lock is released.
  reset_inherit.run();
  if (spawn_lock.owns_lock()) spawn_lock.unlock();

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
    // Own both handles from here on. Every early return below - the fork
    // shortcut, the timeout branch, a failed GetExitCodeProcess - used to skip
    // the CloseHandle at the very bottom, so each forked or timed-out
    // invocation leaked a process handle and kept the process object alive in
    // a service that runs for months. A caller who can make a script hang
    // through its arguments (an unreachable ping host with a long wait) drives
    // that remotely.
    generic_handle process_handle(pi.hProcess);
    generic_handle thread_handle(pi.hThread);
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
        const std::string chunk = readFromFile(buffer, hChildOutR.get(), dwAvail);
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

    // Final drain. Each pass reads only what PeekNamedPipe reports, so several
    // are needed for more than one buffer's worth - and re-peeking is what
    // keeps this from blocking if a write end is still open elsewhere.
    //
    // Bounded, because closing our write ends does not stop the child: it
    // holds its own inherited copy. A child that never stops writing
    // (`:loop / echo x / goto loop`) refills the pipe as fast as this empties
    // it, so an unbounded drain never returns - and on the timeout path the
    // kill below is what it is standing in front of, so the worker would hang
    // exactly where the timeout was supposed to save it. The deadline is the
    // drain's own, not the command's: a process that exited normally is owed
    // its remaining output even if it used its whole timeout producing it.
    const DWORD drain_start_ms = GetTickCount();
    for (;;) {
      dwAvail = 0;
      if (!::PeekNamedPipe(hChildOutR.get(), nullptr, 0, nullptr, &dwAvail, nullptr) || dwAvail == 0) break;
      const std::string chunk = readFromFile(buffer, hChildOutR.get(), dwAvail);
      if (chunk.empty()) break;
      if (str.size() < kOutputContentCap) {
        str.append(chunk, 0, kOutputContentCap - str.size());
        if (str.size() >= kOutputContentCap) str.append(kOutputTruncMarker);
      }
      if (GetTickCount() - drain_start_ms >= kFinalDrainBudgetMs) break;
    }
    output = utf8::cvt<std::string>(utf8::from_encoding(str, args.encoding));

    remove_proc(pi.hProcess);
    thread_handle.close();
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
    return result;
  }
  const DWORD error = spawn_error;
  if (error == ERROR_BAD_EXE_FORMAT) {
    output = "Failed to execute " + args.alias + " seems more like a script maybe you need a script executable first: " + error::lookup::last_error(error);
  } else {
    output = "Failed to execute " + args.alias + ": " + error::lookup::last_error(error);
  }
  return NSCAPI::query_return_codes::returnUNKNOWN;
}