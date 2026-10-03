// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <exception>
#include <win/pdh/thread_safe_impl.hpp>

namespace PDH {
// mutex_ is held for the whole reload. The subscribers (PDHQuery) react to
// on_unload() / on_reload() by calling straight back into this object -
// PdhRemoveCounter, PdhCloseQuery, PdhOpenQuery - which take it again on this
// thread, hence recursive. Holding it throughout is what keeps every other
// thread's Pdh* calls, and a PDHQuery's own reads of its handles (made under
// lock()), from running against a half-reloaded query or a freed library.
bool ThreadedSafePDH::reload() {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for reload");
  // Every subscriber is called back even when one throws: stopping at the
  // first failure left the ones already unloaded closed, with nothing to
  // reopen them. The first error is rethrown once all have run.
  std::exception_ptr first_error;
  for (subscriber* sub : subscribers_) {
    try {
      sub->on_unload();
    } catch (...) {
      if (!first_error) first_error = std::current_exception();
    }
  }
  unload_procs();
  load_procs();
  for (subscriber* sub : subscribers_) {
    try {
      sub->on_reload();
    } catch (...) {
      if (!first_error) first_error = std::current_exception();
    }
  }
  if (first_error) std::rethrow_exception(first_error);
  return true;
}

void ThreadedSafePDH::lock() { mutex_.lock(); }
void ThreadedSafePDH::unlock() { mutex_.unlock(); }

void ThreadedSafePDH::add_listener(subscriber* sub) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  subscribers_.push_back(sub);
}
void ThreadedSafePDH::remove_listener(subscriber* sub) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  // Previously this loop did `it = erase(it)` and then the for-loop's `++it`
  // ran on the result — skipping the element after the erased one, so a
  // listener that appeared twice (or two adjacent listeners equal to `sub`)
  // would not be fully removed. std::list::remove handles all occurrences
  // safely in one pass.
  subscribers_.remove(sub);
}

pdh_error ThreadedSafePDH::PdhLookupPerfIndexByName(const LPCTSTR szMachineName, const LPCTSTR szName, DWORD* dwIndex) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhLookupPerfIndexByName");
  if (pPdhLookupPerfIndexByName == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
  return pdh_error(pPdhLookupPerfIndexByName(szMachineName, szName, dwIndex));
}

pdh_error ThreadedSafePDH::PdhLookupPerfNameByIndex(const LPCTSTR szMachineName, const DWORD dwNameIndex, const LPTSTR szNameBuffer,
                                                    const LPDWORD pcchNameBufferSize) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhLookupPerfNameByIndex");
  if (pPdhLookupPerfNameByIndex == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
  return pdh_error(pPdhLookupPerfNameByIndex(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
}

pdh_error ThreadedSafePDH::PdhExpandCounterPath(const LPCTSTR szWildCardPath, const LPTSTR mszExpandedPathList, const LPDWORD pcchPathListLength) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhExpandCounterPath");
  if (pPdhExpandCounterPath == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
  return pdh_error(pPdhExpandCounterPath(szWildCardPath, mszExpandedPathList, pcchPathListLength));
}
pdh_error ThreadedSafePDH::PdhGetCounterInfo(const PDH_HCOUNTER hCounter, const BOOLEAN bRetrieveExplainText, const LPDWORD pdwBufferSize,
                                             PDH_COUNTER_INFO* lpBuffer) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetCounterInfo");
  if (pPdhGetCounterInfo == nullptr) throw pdh_exception("Failed to initialize PdhGetCounterInfo :(");
  return pdh_error(pPdhGetCounterInfo(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
}
pdh_error ThreadedSafePDH::PdhAddCounter(const PDH_HQUERY hQuery, const LPCWSTR szFullCounterPath, const DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhAddCounter");
  if (pPdhAddCounter == nullptr) throw pdh_exception("Failed to initialize PdhAddCounter :(");
  return pdh_error(pPdhAddCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
}
pdh_error ThreadedSafePDH::PdhAddEnglishCounter(const PDH_HQUERY hQuery, const LPCWSTR szFullCounterPath, const DWORD_PTR dwUserData,
                                                PDH_HCOUNTER* phCounter) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhAddEnglishCounter");
  if (pPdhAddEnglishCounter == nullptr) throw pdh_exception("PdhAddEnglishCounter is only available on Vista and later you need to use localized counters.");
  return pdh_error(pPdhAddEnglishCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
}
pdh_error ThreadedSafePDH::PdhRemoveCounter(const PDH_HCOUNTER hCounter) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhRemoveCounter");
  if (pPdhRemoveCounter == nullptr) throw pdh_exception("Failed to initialize PdhRemoveCounter :(");
  return pdh_error(pPdhRemoveCounter(hCounter));
}
pdh_error ThreadedSafePDH::PdhGetRawCounterValue(const PDH_HCOUNTER hCounter, const LPDWORD dwFormat, const PPDH_RAW_COUNTER pValue) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetRawCounterValue");
  if (pPdhGetRawCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetRawCounterValue :(");
  return pdh_error(pPdhGetRawCounterValue(hCounter, dwFormat, pValue));
}
pdh_error ThreadedSafePDH::PdhGetFormattedCounterValue(const PDH_HCOUNTER hCounter, const DWORD dwFormat, const LPDWORD lpdwType,
                                                       const PPDH_FMT_COUNTERVALUE pValue) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetFormattedCounterValue");
  if (pPdhGetFormattedCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue :(");
  return pdh_error(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
}
pdh_error ThreadedSafePDH::PdhOpenQuery(const LPCWSTR szDataSource, const DWORD_PTR dwUserData, PDH_HQUERY* phQuery) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhOpenQuery");
  if (pPdhOpenQuery == nullptr) throw pdh_exception("Failed to initialize PdhOpenQuery :(");
  return pdh_error(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
}
pdh_error ThreadedSafePDH::PdhCloseQuery(const PDH_HQUERY hQuery) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhCloseQuery");
  if (pPdhCloseQuery == nullptr) throw pdh_exception("Failed to initialize PdhCloseQuery :(");
  return pdh_error(pPdhCloseQuery(hQuery));
}
pdh_error ThreadedSafePDH::PdhCollectQueryData(const PDH_HQUERY hQuery) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhCollectQueryData");
  if (pPdhCollectQueryData == nullptr) throw pdh_exception("Failed to initialize PdhCollectQueryData :(");
  return pdh_error(pPdhCollectQueryData(hQuery));
}
pdh_error ThreadedSafePDH::validate_path_locked(const LPCWSTR szFullPathBuffer) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhValidatePath");
  if (pPdhValidatePath == nullptr) throw pdh_exception("Failed to initialize PdhValidatePath :(");
  return pdh_error(pPdhValidatePath(szFullPathBuffer));
}
pdh_error ThreadedSafePDH::PdhValidatePath(const LPCWSTR szFullPathBuffer, const bool force_reload) {
  pdh_error status = validate_path_locked(szFullPathBuffer);
  if (status.is_error() && force_reload) {
    reload();
    status = validate_path_locked(szFullPathBuffer);
  }
  return status;
}
pdh_error ThreadedSafePDH::PdhEnumObjects(const LPCWSTR szDataSource, const LPCWSTR szMachineName, const LPWSTR mszObjectList, const LPDWORD pcchBufferSize,
                                          const DWORD dwDetailLevel, const BOOL bRefresh) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhEnumObjects");
  if (pPdhEnumObjects == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjects :(");
  return pdh_error(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
}
pdh_error ThreadedSafePDH::PdhEnumObjectItems(const LPCWSTR szDataSource, const LPCWSTR szMachineName, const LPCWSTR szObjectName, const LPWSTR mszCounterList,
                                              const LPDWORD pcchCounterListLength, const LPWSTR mszInstanceList, const LPDWORD pcchInstanceListLength,
                                              const DWORD dwDetailLevel, const DWORD dwFlags) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhEnumObjectItems");
  if (pPdhEnumObjectItems == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjectItems :(");
  return pdh_error(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList,
                                       pcchInstanceListLength, dwDetailLevel, dwFlags));
}
pdh_error ThreadedSafePDH::PdhExpandWildCardPath(const LPCTSTR szDataSource, const LPCTSTR szWildCardPath, const LPWSTR mszExpandedPathList,
                                                 const LPDWORD pcchPathListLength, const DWORD dwFlags) {
  boost::unique_lock<boost::recursive_mutex> guard(mutex_);
  if (!guard.owns_lock()) throw pdh_exception("Failed to get mutex for PdhExpandWildCardPath");
  if (pPdhExpandWildCardPath == nullptr) throw pdh_exception("Failed to initialize PdhExpandWildCardPath :(");
  return pdh_error(pPdhExpandWildCardPath(szDataSource, szWildCardPath, mszExpandedPathList, pcchPathListLength, dwFlags));
}
}  // namespace PDH