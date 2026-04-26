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

#include <strsafe.h>

#include <error/error_w32.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <win/windows.hpp>

namespace error {
namespace win32 {

namespace {
// FormatMessage's longest documented output for system messages is well under
// a kilobyte; 4 KiB leaves comfortable headroom for the "%lx: " prefix and any
// inserts. Using a fixed stack buffer with StringCchPrintfW removes the need
// for new[]/delete[] and the unbounded wsprintf call this code used before.
constexpr size_t kFormattedMessageBufChars = 4096;

std::wstring safe_format(const wchar_t *fmt, ...) {
  wchar_t buf[kFormattedMessageBufChars];
  va_list args;
  va_start(args, fmt);
  const HRESULT hr = StringCchVPrintfW(buf, kFormattedMessageBufChars, fmt, args);
  va_end(args);

  if (SUCCEEDED(hr)) {
    return {buf};
  }
  if (hr == STRSAFE_E_INSUFFICIENT_BUFFER) {
    return std::wstring(buf) + L" [truncated]";
  }
  return L"<format_message: StringCchVPrintfW failed>";
}
}  // namespace

unsigned int lookup() { return GetLastError(); }

std::string failed(unsigned long err1, unsigned long err2) {
  if (err2 == 0) err2 = GetLastError();
  return "failed to lookup error code: " + str::xtos(err1) + " (reason: " + str::xtos(err2) + ")";
}

std::string format_message(unsigned long attrs, std::string module, unsigned long dwError) {
  LPVOID lpMsgBuf;
  HMODULE hMod = nullptr;
  attrs |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
  if (!module.empty()) {
    attrs |= FORMAT_MESSAGE_FROM_HMODULE;
    hMod = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (hMod == nullptr) {
      return failed(dwError);
    }
  } else {
    attrs |= FORMAT_MESSAGE_FROM_SYSTEM;
  }
  const unsigned long dwRet = FormatMessage(attrs, hMod, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&lpMsgBuf), 0, nullptr);
  if (dwRet == 0) {
    FreeLibrary(hMod);
    const DWORD err = GetLastError();
    if (err == ERROR_MR_MID_NOT_FOUND) {
      return "";
    }
    return failed(dwError, err);
  }
  auto str = utf8::cvt<std::string>(safe_format(L"%lx: %s", dwError, static_cast<wchar_t *>(lpMsgBuf)));
  LocalFree(lpMsgBuf);
  FreeLibrary(hMod);
  return str;
}

std::string format_message(unsigned long attrs, std::string module, unsigned long dwError, DWORD *arguments) {
  LPVOID lpMsgBuf;
  HMODULE hMod = nullptr;
  attrs |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
  attrs |= FORMAT_MESSAGE_ARGUMENT_ARRAY;
  if (!module.empty()) {
    attrs |= FORMAT_MESSAGE_FROM_HMODULE;
    hMod = LoadLibraryEx(utf8::cvt<std::wstring>(module).c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (hMod == nullptr) {
      return failed(dwError);
    }
  } else {
    attrs |= FORMAT_MESSAGE_FROM_SYSTEM;
  }
  const unsigned long dwRet = FormatMessage(attrs, hMod, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&lpMsgBuf), 0,
                                            reinterpret_cast<va_list *>(arguments));
  if (dwRet == 0) {
    FreeLibrary(hMod);
    return failed(dwError);
  }
  // %lu (was %d) - dwError is a DWORD (unsigned long).
  auto str = utf8::cvt<std::string>(safe_format(L"%lu: %s", dwError, static_cast<wchar_t *>(lpMsgBuf)));
  LocalFree(lpMsgBuf);
  FreeLibrary(hMod);
  return str;
}
};  // namespace win32
}  // namespace error
