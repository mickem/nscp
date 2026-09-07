// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/tuple/tuple.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/utils.hpp>
#include <str/wstring.hpp>
#include <str/xtos.hpp>
#include <win/windows.hpp>

#include "simple_registry.hpp"

class EventLogRecord : boost::noncopyable {
  const EVENTLOGRECORD *pevlr_;
  std::string file_;

 public:
  EventLogRecord(std::string file, const EVENTLOGRECORD *pevlr) : file_(file), pevlr_(pevlr) {
    if (pevlr == NULL) throw nsclient::nsclient_exception("Invalid eventlog record");
  }

  // Every string inside an EVENTLOGRECORD is read against the record's own
  // Length rather than trusted to be terminated: a record written without a
  // terminator would otherwise be scanned into whatever follows the buffer.
  const wchar_t *record_end() const { return reinterpret_cast<const wchar_t *>(reinterpret_cast<const BYTE *>(pevlr_) + pevlr_->Length); }
  const wchar_t *string_at(DWORD offset) const {
    if (offset >= pevlr_->Length) return record_end();
    return reinterpret_cast<const wchar_t *>(reinterpret_cast<const BYTE *>(pevlr_) + offset);
  }
  // The terminated string starting at p, or as much of it as fits in the record.
  std::wstring bounded_string(const wchar_t *p) const {
    const wchar_t *end = record_end();
    const wchar_t *q = p;
    while (q < end && *q != 0) ++q;
    return std::wstring(p, q);
  }
  // The start of the string after the one at p, clamped to the record end.
  const wchar_t *next_string(const wchar_t *p) const {
    const wchar_t *end = record_end();
    while (p < end && *p != 0) ++p;
    return p < end ? p + 1 : end;
  }

  inline unsigned long long generated() const { return pevlr_->TimeGenerated; }
  inline unsigned long long written() const { return pevlr_->TimeWritten; }
  inline WORD category() const { return pevlr_->EventCategory; }
  inline std::wstring get_source() const { return bounded_string(string_at(sizeof(EVENTLOGRECORD))); }
  inline std::wstring get_computer() const { return bounded_string(next_string(string_at(sizeof(EVENTLOGRECORD)))); }
  inline DWORD eventID() const { return (pevlr_->EventID & 0xffff); }
  inline DWORD severity() const { return (pevlr_->EventID >> 30) & 0x3; }
  inline DWORD facility() const { return (pevlr_->EventID >> 16) & 0xfff; }
  inline WORD customer() const { return (pevlr_->EventID >> 29) & 0x1; }
  inline DWORD raw_id() const { return pevlr_->EventID; }

  inline DWORD eventType() const { return pevlr_->EventType; }

  std::wstring userSID() const {
    if (pevlr_->UserSidOffset == 0) return L" ";
    PSID p = NULL;  // = reinterpret_cast<const void*>(reinterpret_cast<const BYTE*>(pevlr_) + + pevlr_->UserSidOffset);
    DWORD userLen = 0;
    DWORD domainLen = 0;
    SID_NAME_USE sidName;

    LookupAccountSid(NULL, p, NULL, &userLen, NULL, &domainLen, &sidName);
    LPTSTR user = new TCHAR[userLen + 10];
    LPTSTR domain = new TCHAR[domainLen + 10];

    LookupAccountSid(NULL, p, user, &userLen, domain, &domainLen, &sidName);
    user[userLen] = 0;
    domain[domainLen] = 0;
    std::wstring ustr = user;
    std::wstring dstr = domain;
    delete[] user;
    delete[] domain;
    if (!dstr.empty()) dstr = dstr + L"\\";
    if (ustr.empty() && dstr.empty()) return L"missing";

    return dstr + ustr;
  }

  std::wstring enumStrings() const {
    std::wstring ret;
    const wchar_t *p = string_at(pevlr_->StringOffset);
    for (unsigned int i = 0; i < pevlr_->NumStrings && p < record_end(); i++) {
      std::wstring s = bounded_string(p);
      if (!s.empty()) s += L", ";
      ret += s;
      p = next_string(p);
    }
    return ret;
  }

  static WORD appendType(WORD dwType, std::wstring sType) { return dwType | translateType(sType); }
  static WORD subtractType(WORD dwType, std::wstring sType) { return dwType & (!translateType(sType)); }
  static WORD translateType(std::wstring sType) {
    if (sType.empty()) return EVENTLOG_ERROR_TYPE;
    if (sType == L"error") return EVENTLOG_ERROR_TYPE;
    if (sType == L"warning") return EVENTLOG_WARNING_TYPE;
    if (sType == L"success") return EVENTLOG_SUCCESS;
    if (sType == L"info") return EVENTLOG_INFORMATION_TYPE;
    if (sType == L"auditSuccess") return EVENTLOG_AUDIT_SUCCESS;
    if (sType == L"auditFailure") return EVENTLOG_AUDIT_FAILURE;
    return static_cast<WORD>(strEx::stox<WORD>(sType));
  }
  static WORD translateType(std::string sType) {
    if (sType.empty()) return EVENTLOG_ERROR_TYPE;
    if (sType == "error") return EVENTLOG_ERROR_TYPE;
    if (sType == "warning") return EVENTLOG_WARNING_TYPE;
    if (sType == "success") return EVENTLOG_SUCCESS;
    if (sType == "info") return EVENTLOG_INFORMATION_TYPE;
    if (sType == "auditSuccess") return EVENTLOG_AUDIT_SUCCESS;
    if (sType == "auditFailure") return EVENTLOG_AUDIT_FAILURE;
    return str::stox<WORD>(sType);
  }
  static std::wstring translateType(WORD dwType) {
    if (dwType == EVENTLOG_ERROR_TYPE) return L"error";
    if (dwType == EVENTLOG_WARNING_TYPE) return L"warning";
    if (dwType == EVENTLOG_SUCCESS) return L"success";
    if (dwType == EVENTLOG_INFORMATION_TYPE) return L"info";
    if (dwType == EVENTLOG_AUDIT_SUCCESS) return L"auditSuccess";
    if (dwType == EVENTLOG_AUDIT_FAILURE) return L"auditFailure";
    return strEx::xtos(dwType);
  }
  static WORD translateSeverity(std::wstring sType) {
    if (sType.empty()) return 0;
    if (sType == L"success") return 0;
    if (sType == L"informational") return 1;
    if (sType == L"warning") return 2;
    if (sType == L"error") return 3;
    return static_cast<WORD>(strEx::stox<WORD>(sType));
  }
  static WORD translateSeverity(std::string sType) {
    if (sType.empty()) return 0;
    if (sType == "success") return 0;
    if (sType == "informational") return 1;
    if (sType == "warning") return 2;
    if (sType == "error") return 3;
    return str::stox<WORD>(sType);
  }
  static std::wstring translateSeverity(WORD dwType) {
    if (dwType == 0) return L"success";
    if (dwType == 1) return L"informational";
    if (dwType == 2) return L"warning";
    if (dwType == 3) return L"error";
    return strEx::xtos(dwType);
  }
  bool get_dll(std::wstring &file_or_error) const {
    try {
      file_or_error = simple_registry::registry_key::get_string(
          HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + utf8::cvt<std::wstring>(file_) + (std::wstring)L"\\" + get_source(),
          L"EventMessageFile");
      return true;
    } catch (simple_registry::registry_exception &) {
    }
    try {
      std::wstring providerGuid = simple_registry::registry_key::get_string(
          HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + utf8::cvt<std::wstring>(file_) + (std::wstring)L"\\" + get_source(),
          L"ProviderGuid");
      file_or_error = simple_registry::registry_key::get_string(
          HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Publishers" + providerGuid, L"MessageFileName");
      return true;
    } catch (simple_registry::registry_exception &e) {
      file_or_error = L"Could not extract DLL for eventsource: " + get_source() + L": " + utf8::cvt<std::wstring>(e.reason());
      return false;
    }
  }

  struct tchar_array {
    TCHAR **buffer;
    std::size_t size;
    tchar_array(std::size_t size) : buffer(NULL), size(size) {
      buffer = new TCHAR *[size];
      for (std::size_t i = 0; i < size; i++) buffer[i] = NULL;
    }
    ~tchar_array() {
      for (std::size_t i = 0; i < size; i++) delete[] buffer[i];
      delete[] buffer;
    }
    std::size_t set(std::size_t i, const TCHAR *str) {
      std::size_t len = wcslen(str);
      buffer[i] = new TCHAR[len + 2];
      wcsncpy(buffer[i], str, len + 1);
      return len;
    }
    TCHAR **get_buffer_unsafe() { return buffer; }
  };

  boost::tuple<DWORD, std::wstring> safe_format(HMODULE hDLL, DWORD dwLang) const {
    LPVOID lpMsgBuf;
    unsigned long dwRet =
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_IGNORE_INSERTS, hDLL,
                      pevlr_->EventID, dwLang, (LPTSTR)&lpMsgBuf, 0, NULL);
    if (dwRet == 0) {
      return boost::tuple<DWORD, std::wstring>(GetLastError(), L" ");
    }
    std::wstring msg = reinterpret_cast<wchar_t *>(lpMsgBuf);
    LocalFree(lpMsgBuf);
    const wchar_t *p = string_at(pevlr_->StringOffset);
    for (unsigned int i = 0; i < pevlr_->NumStrings && p < record_end(); i++) {
      strEx::replace(msg, L"%" + strEx::xtos(i + 1), bounded_string(p));
      p = next_string(p);
    }
    return boost::make_tuple(0, msg);
  }
  std::wstring render_message(const int truncate_message, DWORD dwLang = 0) const {
    std::vector<std::wstring> args;
    std::wstring ret;
    std::wstring file;
    if (!get_dll(file)) {
      return file;
    }
    for (const std::wstring &dll : strEx::splitEx(file, L";")) {
      // std::wstring msg = error::format::message::from_module((*cit), eventID(), _sz);
      std::wstring msg;
      try {
        HMODULE hDLL = LoadLibraryEx(dll.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
        if (hDLL == NULL) {
          msg = L"failed to load: " + dll + L", reason: " + strEx::xtos(GetLastError());
          continue;
        }
        if (dwLang == 0) dwLang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
        boost::tuple<DWORD, std::wstring> formated_data = safe_format(hDLL, dwLang);
        if (formated_data.get<0>() != 0) {
          FreeLibrary(hDLL);
          if (formated_data.get<0>() == 15100) {
            // Invalid MUI file (wrong language)
            msg = L" ";
            continue;
          }
          if (formated_data.get<0>() == 317) {
            // Missing message
            msg = L" ";
            continue;
          }
          msg = L"failed to lookup error code: " + strEx::xtos(eventID()) + L" from DLL: " + dll + L"( reason: " + strEx::xtos(formated_data.get<0>()) + L")";
          continue;
        }
        FreeLibrary(hDLL);
        msg = formated_data.get<1>();
      } catch (...) {
        msg = L"Unknown exception getting message";
      }
      strEx::replace(msg, L"\n", L" ");
      strEx::replace(msg, L"\t", L" ");
      std::string::size_type pos = msg.find_last_not_of(L"\n\t ");
      if (pos != std::string::npos) {
        msg = msg.substr(0, pos);
      }
      if (!msg.empty()) {
        if (!ret.empty()) ret += L", ";
        ret += msg;
      }
    }
    if (truncate_message > 0 && ret.length() > truncate_message) ret = ret.substr(0, truncate_message);
    return ret;
  }
  SYSTEMTIME get_time(DWORD time) const {
    FILETIME FileTime, LocalFileTime;
    SYSTEMTIME SysTime;
    __int64 lgTemp;
    __int64 SecsTo1970 = 116444736000000000;

    lgTemp = Int32x32To64(time, 10000000) + SecsTo1970;

    FileTime.dwLowDateTime = (DWORD)lgTemp;
    FileTime.dwHighDateTime = (DWORD)(lgTemp >> 32);

    FileTimeToLocalFileTime(&FileTime, &LocalFileTime);
    FileTimeToSystemTime(&LocalFileTime, &SysTime);
    return SysTime;
  }

  SYSTEMTIME get_time_generated() const { return get_time(pevlr_->TimeGenerated); }
  SYSTEMTIME get_time_written() const { return get_time(pevlr_->TimeWritten); }
  inline std::string get_log() const { return file_; }
};