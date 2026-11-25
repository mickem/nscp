// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// safe_file on Windows. See safe_file.hpp for what is refused and why.

#include <windows.h>

#include <lm.h>

#include <error/error.hpp>
#include <files/safe_file.hpp>
#include <mutex>
#include <str/utf8.hpp>
#include <vector>

#pragma comment(lib, "netapi32.lib")

namespace safe_file {

namespace {
HANDLE as_handle(void *h) { return h == nullptr ? INVALID_HANDLE_VALUE : static_cast<HANDLE>(h); }

// Names reach a check as UTF-8, except that a path_policy in `allowed` mode
// hands back the path it resolved through boost::filesystem, which spells
// it in the ANSI code page. Text that is not UTF-8 is read as that.
std::wstring to_wide(const std::string &text) { return utf8::is_valid(text) ? utf8::cvt<std::wstring>(text) : utf8::to_unicode(text); }

std::string ascii_lower(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return text;
}

// Shares seen to be file systems, by lower-cased root. What a share is does
// not change under a caller's hands - the server defines it - so remembering
// a yes for a minute is safe; a no or a failure is never remembered.
struct seen_share {
  std::string root;
  DWORD tick;
};
const DWORD seen_share_ttl_ms = 60 * 1000;
const std::size_t seen_share_max = 32;
std::mutex seen_shares_mutex;
std::vector<seen_share> seen_shares;

bool seen_as_file_system(const std::string &key) {
  const DWORD now = GetTickCount();
  std::lock_guard<std::mutex> lock(seen_shares_mutex);
  for (const seen_share &s : seen_shares) {
    // Unsigned subtraction, so the tick counter wrapping is harmless.
    if (s.root == key && now - s.tick < seen_share_ttl_ms) return true;
  }
  return false;
}

void remember_file_system(const std::string &key) {
  const DWORD now = GetTickCount();
  std::lock_guard<std::mutex> lock(seen_shares_mutex);
  for (seen_share &s : seen_shares) {
    if (s.root == key) {
      s.tick = now;
      return;
    }
  }
  if (seen_shares.size() >= seen_share_max) seen_shares.erase(seen_shares.begin());
  seen_shares.push_back(seen_share{key, now});
}

// Ask the server what the share is. Used only when the root of the share
// could not be opened for lack of access; many servers refuse this question
// too, and then there is no answer.
share_state ask_server_for_share_type(const std::string &host, const std::string &share, std::string &reason) {
  std::wstring wide_host = L"\\\\" + to_wide(host);
  std::wstring wide_share = to_wide(share);
  LPBYTE buffer = nullptr;
  const NET_API_STATUS status = NetShareGetInfo(&wide_host[0], &wide_share[0], 1, &buffer);
  if (status != NERR_Success || buffer == nullptr) {
    if (buffer != nullptr) NetApiBufferFree(buffer);
    reason = error::lookup::last_error(static_cast<unsigned long>(status));
    return share_state::unreachable;
  }
  // The low byte is the kind of share; the bits above it only say that it is
  // hidden or temporary.
  const DWORD type = reinterpret_cast<const SHARE_INFO_1 *>(buffer)->shi1_type & 0xFF;
  NetApiBufferFree(buffer);
  return type == STYPE_DISKTREE ? share_state::file_system : share_state::not_file_system;
}
}  // namespace

share_state examine_share(const std::string &path, std::string &reason) {
  std::string root;
  if (!unc_share_root(path, root)) return share_state::not_remote;
  const std::string key = ascii_lower(root);
  if (seen_as_file_system(key)) return share_state::file_system;

  share_state state = share_state::unreachable;
  const HANDLE h = CreateFileW(to_wide(root).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    state = GetFileType(h) == FILE_TYPE_DISK ? share_state::file_system : share_state::not_file_system;
    CloseHandle(h);
  } else {
    const DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
      std::size_t prefix = 0;
      std::string host, share;
      detail::split_unc(detail::fold_separators(path), prefix, host, share);
      state = ask_server_for_share_type(host, share, reason);
    } else {
      reason = error::lookup::last_error(static_cast<unsigned long>(err));
    }
  }
  if (state == share_state::file_system) remember_file_system(key);
  return state;
}

void *detail::create_for_read(const std::string &path) {
  const std::wstring wide = to_wide(path);
  // SECURITY_IDENTIFICATION: should the name still reach a local named pipe
  // (a symbolic link, a spelling the name check does not know), its server
  // learns who connected and cannot act as that account. Without the flags,
  // the default is SecurityImpersonation. They mean nothing for a file - and
  // nothing over the network either, which is why file::open() examines the
  // share of a UNC path before it gets here.
  const HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
  return h == INVALID_HANDLE_VALUE ? nullptr : h;
}

file::file(file &&other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }

file &file::operator=(file &&other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

file::~file() { close(); }

bool file::is_open() const { return handle_ != nullptr; }

void file::close() {
  if (handle_ != nullptr) CloseHandle(static_cast<HANDLE>(handle_));
  handle_ = nullptr;
}

bool file::open(const std::string &path, std::string &error) {
  close();
  const std::string shown = utf8::make_valid(path);
  if (is_win32_device_path(path)) {
    error = "Refusing to open " + shown + ": it names a device or a pipe, not a file";
    return false;
  }
  std::string share_reason;
  switch (examine_share(path, share_reason)) {
    case share_state::not_file_system:
      error = "Refusing to open " + shown + ": its share is not a file system";
      return false;
    case share_state::unreachable:
      error = "Failed to open file: " + shown + ": " + share_reason;
      return false;
    case share_state::file_system:
    case share_state::not_remote:
    default:
      break;
  }
  const HANDLE h = as_handle(detail::create_for_read(path));
  if (h == INVALID_HANDLE_VALUE) {
    const std::string reason = error::lookup::last_error();
    error = "Failed to open file: " + shown + ": " + reason;
    return false;
  }
  handle_ = h;
  // Only a file on a disk (local or on a share). A pipe or a character
  // device would also block ReadFile until its other end writes. A
  // directory cannot be opened without FILE_FLAG_BACKUP_SEMANTICS, but check
  // the attributes anyway: a raw volume reached through a link would report
  // FILE_TYPE_DISK too, and has no file information to give.
  //
  // Not every file system answers GetFileInformationByHandle, though, and a
  // file on one that does not is still a file. So a failure there is only a
  // refusal when the handle will not give a size either - which a file on
  // any file system does, and a volume opened raw does not.
  BY_HANDLE_FILE_INFORMATION info;
  bool regular = GetFileType(h) == FILE_TYPE_DISK;
  if (regular) {
    if (GetFileInformationByHandle(h, &info)) {
      regular = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    } else {
      LARGE_INTEGER size;
      regular = GetFileSizeEx(h, &size) != 0;
    }
  }
  if (!regular) {
    error = "Not a regular file: " + shown;
    close();
    return false;
  }
  return true;
}

bool file::read(char *buf, const std::size_t len, std::size_t &got) {
  got = 0;
  const DWORD want = len > 0x40000000 ? 0x40000000 : static_cast<DWORD>(len);
  DWORD n = 0;
  if (!ReadFile(as_handle(handle_), buf, want, &n, nullptr)) return false;
  got = n;
  return true;
}

bool file::seek(const std::uint64_t offset) {
  LARGE_INTEGER li;
  li.QuadPart = static_cast<LONGLONG>(offset);
  return SetFilePointerEx(as_handle(handle_), li, nullptr, FILE_BEGIN) != 0;
}

bool file::size(std::uint64_t &out) {
  LARGE_INTEGER li;
  if (!GetFileSizeEx(as_handle(handle_), &li) || li.QuadPart < 0) return false;
  out = static_cast<std::uint64_t>(li.QuadPart);
  return true;
}

}  // namespace safe_file
