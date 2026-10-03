// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <string>
#include <win/pdh/basic_impl.hpp>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
class ThreadedSafePDH : public NativeExternalPDH {
  // Serialises every call made through this object, reload() included, and
  // guards subscribers_. Recursive because reload() holds it across the
  // subscriber callbacks, which call straight back in, and because a
  // PDHQuery holds it (lock()) across work that must not straddle a reload.
  //
  // The proc table it orders access to is static and shared with every
  // NativeExternalPDH, whose constructor and reload() write it without this
  // lock: it covers the calls made through this object, nothing else.
  boost::recursive_mutex mutex_;
  typedef std::list<subscriber*> subscriber_list;
  subscriber_list subscribers_;

  pdh_error validate_path_locked(LPCWSTR szFullPathBuffer);

  // Called with mutex_ held: returns `proc`, loading PDH first when a failed
  // reload() left it unloaded, so the next call retries the load instead of
  // every call failing until the next reload.
  template <class Proc>
  static Proc require_locked(const Proc& proc, const char* name) {
    if (proc == nullptr && PDH_ == nullptr) load_procs();
    if (proc == nullptr) throw pdh_exception(std::string("Failed to initialize ") + name);
    return proc;
  }

 public:
  ThreadedSafePDH() {}

  bool reload() override;

  void add_listener(subscriber* sub) override;
  void remove_listener(subscriber* sub) override;
  void lock() override;
  void unlock() override;

  pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD* dwIndex) override;
  pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) override;
  pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) override;
  pdh_error PdhGetCounterInfo(PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO* lpBuffer) override;
  pdh_error PdhAddCounter(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) override;
  pdh_error PdhAddEnglishCounter(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) override;
  pdh_error PdhRemoveCounter(PDH_HCOUNTER hCounter) override;
  pdh_error PdhGetRawCounterValue(PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER pValue) override;
  pdh_error PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) override;
  pdh_error PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH_HQUERY* phQuery) override;
  pdh_error PdhCloseQuery(PDH_HQUERY hQuery) override;
  pdh_error PdhCollectQueryData(PDH_HQUERY hQuery) override;
  pdh_error PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) override;
  pdh_error PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel,
                           BOOL bRefresh) override;
  pdh_error PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength,
                               LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) override;
  pdh_error PdhExpandWildCardPath(LPCTSTR szDataSource, LPCTSTR szWildCardPath, LPWSTR mszExpandedPathList, LPDWORD pcchPathListLength, DWORD dwFlags) override;
};
}  // namespace PDH