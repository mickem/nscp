// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <exception>
#include <functional>
#include <list>
#include <string>
#include <win/pdh/thread_safe_impl.hpp>

namespace PDH {
// mutex_ is held for the whole reload. The subscribers (PDHQuery) react to
// on_unload() / on_reload() by calling straight back into this object -
// PdhRemoveCounter, PdhCloseQuery, PdhOpenQuery - which take it again on this
// thread, hence recursive. Holding it throughout is what keeps every other
// thread's Pdh* calls, and a PDHQuery's own reads of its handles (made under
// lock()), from running against a half-reloaded query or a freed library.
//
// Every step runs even when an earlier one fails, and every failure is
// reported: stopping at the first one left the subscribers already unloaded
// closed with nothing to reopen them. A query that still ends up closed (PDH
// could not be loaded again) reopens itself on its next gatherData(), and the
// next call through this object retries the load (require_locked()).
bool ThreadedSafePDH::reload() {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  std::list<std::string> errors;
  const auto run = [&errors](const std::function<void()>& step) {
    std::string error;
    try {
      step();
      return;
    } catch (const std::exception& e) {
      error = e.what();
    } catch (...) {
      error = "unknown error";
    }
    if (std::find(errors.begin(), errors.end(), error) == errors.end()) errors.push_back(error);
  };
  for (subscriber* sub : subscribers_) run([sub] { sub->on_unload(); });
  run([] {
    unload_procs();
    load_procs();
  });
  for (subscriber* sub : subscribers_) run([sub] { sub->on_reload(); });
  if (errors.empty()) return true;
  std::string message = "PDH reload failed:";
  for (const std::string& error : errors) message += " " + error + ";";
  throw pdh_exception(message);
}

void ThreadedSafePDH::lock() { mutex_.lock(); }
void ThreadedSafePDH::unlock() { mutex_.unlock(); }

void ThreadedSafePDH::add_listener(subscriber* sub) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  subscribers_.push_back(sub);
}
void ThreadedSafePDH::remove_listener(subscriber* sub) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  // Previously this loop did `it = erase(it)` and then the for-loop's `++it`
  // ran on the result — skipping the element after the erased one, so a
  // listener that appeared twice (or two adjacent listeners equal to `sub`)
  // would not be fully removed. std::list::remove handles all occurrences
  // safely in one pass.
  subscribers_.remove(sub);
}

pdh_error ThreadedSafePDH::PdhLookupPerfIndexByName(const LPCTSTR szMachineName, const LPCTSTR szName, DWORD* dwIndex) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhLookupPerfIndexByName, "PdhLookupPerfIndexByName")(szMachineName, szName, dwIndex));
}

pdh_error ThreadedSafePDH::PdhLookupPerfNameByIndex(const LPCTSTR szMachineName, const DWORD dwNameIndex, const LPTSTR szNameBuffer,
                                                    const LPDWORD pcchNameBufferSize) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhLookupPerfNameByIndex, "PdhLookupPerfNameByIndex")(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
}

pdh_error ThreadedSafePDH::PdhExpandCounterPath(const LPCTSTR szWildCardPath, const LPTSTR mszExpandedPathList, const LPDWORD pcchPathListLength) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhExpandCounterPath, "PdhExpandCounterPath")(szWildCardPath, mszExpandedPathList, pcchPathListLength));
}
pdh_error ThreadedSafePDH::PdhGetCounterInfo(const PDH_HCOUNTER hCounter, const BOOLEAN bRetrieveExplainText, const LPDWORD pdwBufferSize,
                                             PDH_COUNTER_INFO* lpBuffer) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhGetCounterInfo, "PdhGetCounterInfo")(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
}
pdh_error ThreadedSafePDH::PdhAddCounter(const PDH_HQUERY hQuery, const LPCWSTR szFullCounterPath, const DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhAddCounter, "PdhAddCounter")(hQuery, szFullCounterPath, dwUserData, phCounter));
}
pdh_error ThreadedSafePDH::PdhAddEnglishCounter(const PDH_HQUERY hQuery, const LPCWSTR szFullCounterPath, const DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhAddEnglishCounter, "PdhAddEnglishCounter")(hQuery, szFullCounterPath, dwUserData, phCounter));
}
pdh_error ThreadedSafePDH::PdhRemoveCounter(const PDH_HCOUNTER hCounter) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhRemoveCounter, "PdhRemoveCounter")(hCounter));
}
pdh_error ThreadedSafePDH::PdhGetRawCounterValue(const PDH_HCOUNTER hCounter, const LPDWORD dwFormat, const PPDH_RAW_COUNTER pValue) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhGetRawCounterValue, "PdhGetRawCounterValue")(hCounter, dwFormat, pValue));
}
pdh_error ThreadedSafePDH::PdhGetFormattedCounterValue(const PDH_HCOUNTER hCounter, const DWORD dwFormat, const LPDWORD lpdwType,
                                                       const PPDH_FMT_COUNTERVALUE pValue) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhGetFormattedCounterValue, "PdhGetFormattedCounterValue")(hCounter, dwFormat, lpdwType, pValue));
}
pdh_error ThreadedSafePDH::PdhOpenQuery(const LPCWSTR szDataSource, const DWORD_PTR dwUserData, PDH_HQUERY* phQuery) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhOpenQuery, "PdhOpenQuery")(szDataSource, dwUserData, phQuery));
}
pdh_error ThreadedSafePDH::PdhCloseQuery(const PDH_HQUERY hQuery) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhCloseQuery, "PdhCloseQuery")(hQuery));
}
pdh_error ThreadedSafePDH::PdhCollectQueryData(const PDH_HQUERY hQuery) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhCollectQueryData, "PdhCollectQueryData")(hQuery));
}
pdh_error ThreadedSafePDH::validate_path_locked(const LPCWSTR szFullPathBuffer) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhValidatePath, "PdhValidatePath")(szFullPathBuffer));
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
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhEnumObjects, "PdhEnumObjects")(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
}
pdh_error ThreadedSafePDH::PdhEnumObjectItems(const LPCWSTR szDataSource, const LPCWSTR szMachineName, const LPCWSTR szObjectName, const LPWSTR mszCounterList,
                                              const LPDWORD pcchCounterListLength, const LPWSTR mszInstanceList, const LPDWORD pcchInstanceListLength,
                                              const DWORD dwDetailLevel, const DWORD dwFlags) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(require_locked(pPdhEnumObjectItems, "PdhEnumObjectItems")(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength,
                                                                             mszInstanceList, pcchInstanceListLength, dwDetailLevel, dwFlags));
}
pdh_error ThreadedSafePDH::PdhExpandWildCardPath(const LPCTSTR szDataSource, const LPCTSTR szWildCardPath, const LPWSTR mszExpandedPathList,
                                                 const LPDWORD pcchPathListLength, const DWORD dwFlags) {
  boost::lock_guard<boost::recursive_mutex> guard(mutex_);
  return pdh_error(
      require_locked(pPdhExpandWildCardPath, "PdhExpandWildCardPath")(szDataSource, szWildCardPath, mszExpandedPathList, pcchPathListLength, dwFlags));
}
}  // namespace PDH
