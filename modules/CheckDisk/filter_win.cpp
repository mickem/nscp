// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows-specific filter_obj construction (from WIN32_FIND_DATA) and the
// PE/DLL file-version lookup. The neutral getters and field registration live
// in filter.cpp.

#include "filter.hpp"

#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>

#include "error/error.hpp"

static unsigned long long ft_to_unix(const FILETIME &ft) {
  const unsigned long long v = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | static_cast<unsigned long long>(ft.dwLowDateTime);
  return str::format::filetime_to_time(v);
}

std::shared_ptr<file_filter::filter_obj> file_filter::filter_obj::get(unsigned long long now, const WIN32_FIND_DATA &info, boost::filesystem::path path) {
  const unsigned long long size = (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | static_cast<unsigned long long>(info.nFileSizeLow);
  const bool is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return create(path, utf8::cvt<std::string>(info.cFileName), size, static_cast<long long>(ft_to_unix(info.ftCreationTime)),
                static_cast<long long>(ft_to_unix(info.ftLastAccessTime)), static_cast<long long>(ft_to_unix(info.ftLastWriteTime)), is_dir, now);
}

std::string file_filter::filter_obj::get_version(parsers::where::evaluation_context context) {
  if (cached_version) return *cached_version;
  const std::string fullpath = (path / filename).string();

  DWORD dwDummy;
  const DWORD dwFVISize = GetFileVersionInfoSize(utf8::cvt<std::wstring>(fullpath).c_str(), &dwDummy);
  if (dwFVISize == 0) {
    context->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
    return "";
  };
  const auto lpVersionInfo = new BYTE[dwFVISize + 1];
  if (!GetFileVersionInfo(utf8::cvt<std::wstring>(fullpath).c_str(), 0, dwFVISize, lpVersionInfo)) {
    delete[] lpVersionInfo;
    context->error("Failed to get version for " + fullpath + ": " + error::lookup::last_error());
    return "";
  }
  UINT uLen;
  VS_FIXEDFILEINFO *lpFfi;
  if (!VerQueryValue(lpVersionInfo, L"\\", reinterpret_cast<LPVOID *>(&lpFfi), &uLen)) {
    delete[] lpVersionInfo;
    context->error("Failed to query version for " + fullpath + ": " + error::lookup::last_error());
    return "";
  }
  const DWORD dwFileVersionMS = lpFfi->dwFileVersionMS;
  const DWORD dwFileVersionLS = lpFfi->dwFileVersionLS;
  delete[] lpVersionInfo;
  const DWORD dwLeftMost = HIWORD(dwFileVersionMS);
  const DWORD dwSecondLeft = LOWORD(dwFileVersionMS);
  const DWORD dwSecondRight = HIWORD(dwFileVersionLS);
  const DWORD dwRightMost = LOWORD(dwFileVersionLS);
  cached_version.reset(str::xtos(dwLeftMost) + "." + str::xtos(dwSecondLeft) + "." + str::xtos(dwSecondRight) + "." + str::xtos(dwRightMost));
  return *cached_version;
}
