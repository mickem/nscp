// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows growth spots: the temp directories, the log directories, the
// page files, swapfile.sys and the shadow-copy storage per volume. Every path
// comes from the system (environment, registry, WMI); none is spelled with a
// drive letter here.

#include <windows.h>

#include <error/error.hpp>
#include <str/utf8.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_shadowcopy.hpp"
#include "growth_spots.hpp"

namespace growth_spots {

namespace {

std::string to_utf8(const std::wstring &s) { return utf8::cvt<std::string>(s); }

std::wstring strip_separator(std::wstring path) {
  while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
  return path;
}

std::wstring windows_directory() {
  wchar_t buffer[MAX_PATH + 1] = {};
  const UINT n = GetSystemWindowsDirectoryW(buffer, MAX_PATH);
  if (n == 0 || n > MAX_PATH) return L"";
  return strip_separator(buffer);
}

std::wstring system_directory() {
  wchar_t buffer[MAX_PATH + 1] = {};
  const UINT n = GetSystemDirectoryW(buffer, MAX_PATH);
  if (n == 0 || n > MAX_PATH) return L"";
  return strip_separator(buffer);
}

std::wstring temp_directory() {
  wchar_t buffer[MAX_PATH + 2] = {};
  const DWORD n = GetTempPathW(MAX_PATH + 1, buffer);
  if (n == 0 || n > MAX_PATH + 1) return L"";
  return strip_separator(buffer);
}

// "C:" for a Windows directory of "C:\Windows".
std::wstring system_drive(const std::wstring &windows_dir) {
  if (windows_dir.size() >= 2 && windows_dir[1] == L':') return windows_dir.substr(0, 2);
  return L"";
}

// The page files in use, from the memory manager's own list
// (`\??\C:\pagefile.sys`); the configured list (PagingFiles) can name files
// that do not exist yet or a `?:` placeholder for "system managed".
std::vector<std::wstring> page_files() {
  std::vector<std::wstring> out;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS)
    return out;
  DWORD type = 0;
  DWORD bytes = 0;
  if (RegQueryValueExW(key, L"ExistingPageFiles", nullptr, &type, nullptr, &bytes) == ERROR_SUCCESS && type == REG_MULTI_SZ && bytes > 0) {
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2, L'\0');
    if (RegQueryValueExW(key, L"ExistingPageFiles", nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytes) == ERROR_SUCCESS) {
      for (const wchar_t *p = buffer.data(); *p != L'\0'; p += wcslen(p) + 1) {
        std::wstring entry = p;
        if (entry.compare(0, 4, L"\\??\\") == 0) entry = entry.substr(4);
        if (!entry.empty()) out.push_back(entry);
      }
    }
  }
  RegCloseKey(key);
  return out;
}

// "C:\" for a volume's `Volume{GUID}` token, or the volume's own path when it
// has no drive letter or mount point.
std::string volume_path(const std::string &guid) {
  const std::wstring volume = L"\\\\?\\" + utf8::cvt<std::wstring>(guid) + L"\\";
  wchar_t names[1024] = {};
  DWORD length = 0;
  if (GetVolumePathNamesForVolumeNameW(volume.c_str(), names, 1023, &length) && names[0] != L'\0') return to_utf8(names);
  return to_utf8(volume);
}

// Shadow-copy storage in use per volume. Win32_ShadowStorage is a WMI query
// over the VSS provider: cheap, but it needs an elevated caller (the service
// account is). A query that fails is one row saying so rather than no rows,
// which would read as "no shadow storage".
void shadow_storage(std::vector<spot> &out) {
  const com_helper::mta_scope com;
  try {
    wmi_impl::query q("select Volume, UsedSpace from Win32_ShadowStorage", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      spot s;
      s.kind = kind_shadow_storage;
      s.type = spot::measured;
      const std::string guid = shadowcopy_check::volume_guid(r.get_string("Volume"));
      s.path = guid.empty() ? r.get_string("Volume") : volume_path(guid);
      s.has_size = true;
      s.size = r.get_int("UsedSpace");
      out.push_back(s);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    // Already UTF-8 (FormatMessageW).
    spot s;
    s.kind = kind_shadow_storage;
    s.type = spot::measured;
    s.path = "all volumes";
    s.note = "could not be read (needs an elevated agent): " + e.reason();
    out.push_back(s);
  }
}

spot make(const std::wstring &path, const char *kind, const spot::type_t type) {
  spot s;
  s.path = to_utf8(path);
  s.kind = kind;
  s.type = type;
  return s;
}

}  // namespace

std::vector<spot> discover(const std::string &agent_log_dir) {
  std::vector<spot> out;
  const std::wstring windows = windows_directory();
  const std::wstring system = system_directory();
  // The agent's own temp directory (TMP/TEMP, which for the service account
  // is under the Windows directory), then the system's.
  const std::wstring temp = temp_directory();
  if (!temp.empty()) out.push_back(make(temp, kind_temp, spot::directory));
  if (!windows.empty()) out.push_back(make(windows + L"\\Temp", kind_temp, spot::directory));
  if (!agent_log_dir.empty()) out.push_back(make(strip_separator(utf8::cvt<std::wstring>(agent_log_dir)), kind_logs, spot::directory));
  if (!windows.empty()) out.push_back(make(windows + L"\\Logs", kind_logs, spot::directory));
  if (!system.empty()) out.push_back(make(system + L"\\winevt\\Logs", kind_logs, spot::directory));
  for (const std::wstring &p : page_files()) out.push_back(make(p, kind_page_file, spot::file));
  const std::wstring drive = system_drive(windows);
  if (!drive.empty()) out.push_back(make(drive + L"\\swapfile.sys", kind_swap_file, spot::file));
  shadow_storage(out);
  return out;
}

bool file_size(const std::string &path, unsigned long long &size, std::string &error) {
  // FindFirstFile reads the directory entry: GetFileAttributesEx and opening
  // the file both fail on a page file, which the memory manager holds open.
  WIN32_FIND_DATAW data;
  const HANDLE h = FindFirstFileW(utf8::cvt<std::wstring>(path).c_str(), &data);
  if (h == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) return false;
    error = "could not be read: " + error::lookup::last_error(err);
    return true;
  }
  FindClose(h);
  size = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return true;
}

void lower_priority() {
#ifdef THREAD_MODE_BACKGROUND_BEGIN
  // Background mode lowers the thread's I/O and memory priority as well as
  // its CPU priority, which is what a walk over a busy disk wants.
  SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#else
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#endif
}

bool paths_ignore_case() { return true; }

}  // namespace growth_spots
