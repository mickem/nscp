/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#define BUFF_SIZE 4096

#include <NSCAPI.h>
#include <win/tool-helper.h>

#include <boost/thread.hpp>
#include <buffer.hpp>
#include <char_buffer.hpp>
#include <error/error.hpp>
#include <handle.hpp>
#include <iostream>
#include <process/execute_process.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utf8.hpp>
#include <win/userenv.hpp>
#include <win_sysinfo/win_sysinfo.hpp>

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

  hlp::tchar_buffer tmpCmd(utf8::cvt<std::wstring>(args.command));
  tmpCmd[args.command.length()] = 0;

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

    processOK = CreateProcessAsUser(pHandle.get(), nullptr, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE,
                                    creation_flags | CREATE_UNICODE_ENVIRONMENT, environment.get(), utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
    if (!processOK) {
      imp.close();
      const DWORD error = GetLastError();
      if (error == ERROR_PRIVILEGE_NOT_HELD) {
        processOK = CreateProcessWithLogonW(utf8::cvt<std::wstring>(args.user).c_str(), utf8::cvt<std::wstring>(args.domain).c_str(),
                                            utf8::cvt<std::wstring>(args.password).c_str(), LOGON_WITH_PROFILE, nullptr, tmpCmd.get(), creation_flags, nullptr,
                                            utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
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
    processOK = CreateProcess(nullptr, tmpCmd.get(), nullptr, nullptr, args.fork ? FALSE : TRUE, creation_flags, nullptr,
                              utf8::cvt<std::wstring>(args.root_path).c_str(), &si, &pi);
  }

  if (processOK) {
    DWORD state = 0;
    if (args.fork) {
      output = "Command started successfully";
      return NSCAPI::query_return_codes::returnOK;
    }
    register_proc(pi.hProcess);
    DWORD dwAvail = 0;
    std::string str;
    buffer_type buffer(BUFF_SIZE);
    for (unsigned int i = 0; i < args.timeout * 10; i++) {
      if (!::PeekNamedPipe(hChildOutR.get(), nullptr, 0, nullptr, &dwAvail, nullptr)) {
        break;
      }
      if (dwAvail > 0) {
        str += readFromFile(buffer, hChildOutR.get());
      }
      if (dwAvail == 0) {
        state = WaitForSingleObject(pi.hProcess, 100);
        if (state != WAIT_TIMEOUT) {
          break;
        }
      }
    }
    hChildInW.close();
    hChildInR.close();
    hChildOutW.close();

    dwAvail = 0;
    if (::PeekNamedPipe(hChildOutR.get(), nullptr, 0, nullptr, &dwAvail, nullptr) && dwAvail > 0) {
      str += readFromFile(buffer, hChildOutR.get());
    }
    output = utf8::cvt<std::string>(utf8::from_encoding(str, args.encoding));

    remove_proc(pi.hProcess);
    CloseHandle(pi.hThread);
    if (state == WAIT_TIMEOUT) {
      if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, pi.dwProcessId)) {
        if (WaitForSingleObject(pi.hProcess, 2000) == WAIT_OBJECT_0) {
          state = WAIT_OBJECT_0;
        }
      }
      if (state == WAIT_TIMEOUT) {
        if (args.kill_tree) {
          std::cout << "Killing process tree for pid " << pi.dwProcessId << std::endl;
          kill_process_tree(pi.dwProcessId);
        } else {
          TerminateProcess(pi.hProcess, 5);
        }
        output = "Command " + args.alias + " didn't terminate within the timeout period " + str::xtos(args.timeout) + "s";
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