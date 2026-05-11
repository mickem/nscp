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

#include <win/pdh/thread_safe_impl.hpp>

namespace PDH {
bool ThreadedSafePDH::reload() {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for reload");
  return reload_unsafe();
}

bool ThreadedSafePDH::reload_unsafe() {
  for (subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit) (*cit)->on_unload();
  unload_procs();
  load_procs();
  for (subscriber_list::const_iterator cit = subscribers_.begin(); cit != subscribers_.end(); ++cit) (*cit)->on_reload();
  return true;
}

void ThreadedSafePDH::add_listener(subscriber* sub) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for reload");
  subscribers_.push_back(sub);
}
void ThreadedSafePDH::remove_listener(subscriber* sub) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for reload");
  for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
    if (*it == sub) it = subscribers_.erase(it);
    if (it == subscribers_.end()) break;
  }
}

pdh_error ThreadedSafePDH::PdhLookupPerfIndexByName(const LPCTSTR szMachineName, const LPCTSTR szName, DWORD* dwIndex) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhLookupPerfIndexByName");
  if (pPdhLookupPerfIndexByName == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
  return pdh_error(pPdhLookupPerfIndexByName(szMachineName, szName, dwIndex));
}

pdh_error ThreadedSafePDH::PdhLookupPerfNameByIndex(const LPCTSTR szMachineName, const DWORD dwNameIndex, const LPTSTR szNameBuffer,
                                                    const LPDWORD pcchNameBufferSize) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhLookupPerfNameByIndex");
  if (pPdhLookupPerfNameByIndex == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
  return pdh_error(pPdhLookupPerfNameByIndex(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
}

pdh_error ThreadedSafePDH::PdhExpandCounterPath(const LPCTSTR szWildCardPath, const LPTSTR mszExpandedPathList, const LPDWORD pcchPathListLength) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhExpandCounterPath");
  if (pPdhExpandCounterPath == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex :(");
  return pdh_error(pPdhExpandCounterPath(szWildCardPath, mszExpandedPathList, pcchPathListLength));
}
pdh_error ThreadedSafePDH::PdhGetCounterInfo(const PDH_HCOUNTER hCounter, const BOOLEAN bRetrieveExplainText, const LPDWORD pdwBufferSize,
                                             PDH_COUNTER_INFO* lpBuffer) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetCounterInfo");
  if (pPdhGetCounterInfo == nullptr) throw pdh_exception("Failed to initialize PdhGetCounterInfo :(");
  return pdh_error(pPdhGetCounterInfo(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
}
pdh_error ThreadedSafePDH::PdhAddCounter(const PDH_HQUERY hQuery, const LPCWSTR szFullCounterPath, const DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhAddCounter");
  if (pPdhAddCounter == nullptr) throw pdh_exception("Failed to initialize PdhAddCounter :(");
  return pdh_error(pPdhAddCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
}
pdh_error ThreadedSafePDH::PdhRemoveCounter(const PDH_HCOUNTER hCounter) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhRemoveCounter");
  if (pPdhRemoveCounter == nullptr) throw pdh_exception("Failed to initialize PdhRemoveCounter :(");
  return pdh_error(pPdhRemoveCounter(hCounter));
}
pdh_error ThreadedSafePDH::PdhGetRawCounterValue(const PDH_HCOUNTER hCounter, const LPDWORD dwFormat, const PPDH_RAW_COUNTER pValue) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetRawCounterValue");
  if (pPdhGetRawCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetRawCounterValue :(");
  return pdh_error(pPdhGetRawCounterValue(hCounter, dwFormat, pValue));
}
pdh_error ThreadedSafePDH::PdhGetFormattedCounterValue(const PDH_HCOUNTER hCounter, const DWORD dwFormat, const LPDWORD lpdwType,
                                                       const PPDH_FMT_COUNTERVALUE pValue) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhGetFormattedCounterValue");
  if (pPdhGetFormattedCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue :(");
  return pdh_error(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
}
pdh_error ThreadedSafePDH::PdhOpenQuery(const LPCWSTR szDataSource, const DWORD_PTR dwUserData, PDH_HQUERY* phQuery) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhOpenQuery");
  if (pPdhOpenQuery == nullptr) throw pdh_exception("Failed to initialize PdhOpenQuery :(");
  return pdh_error(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
}
pdh_error ThreadedSafePDH::PdhCloseQuery(const PDH_HQUERY hQuery) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhCloseQuery");
  if (pPdhCloseQuery == nullptr) throw pdh_exception("Failed to initialize PdhCloseQuery :(");
  return pdh_error(pPdhCloseQuery(hQuery));
}
pdh_error ThreadedSafePDH::PdhCollectQueryData(const PDH_HQUERY hQuery) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhCollectQueryData");
  if (pPdhCollectQueryData == nullptr) throw pdh_exception("Failed to initialize PdhCollectQueryData :(");
  return pdh_error(pPdhCollectQueryData(hQuery));
}
pdh_error ThreadedSafePDH::PdhValidatePath(const LPCWSTR szFullPathBuffer, const bool force_reload) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhValidatePath");
  if (pPdhValidatePath == nullptr) throw pdh_exception("Failed to initialize PdhValidatePath :(");
  pdh_error status = pdh_error(pPdhValidatePath(szFullPathBuffer));
  if (status.is_error() && force_reload) {
    reload_unsafe();
    status = pdh_error(pPdhValidatePath(szFullPathBuffer));
  }
  return status;
}
pdh_error ThreadedSafePDH::PdhEnumObjects(const LPCWSTR szDataSource, const LPCWSTR szMachineName, const LPWSTR mszObjectList, const LPDWORD pcchBufferSize,
                                          const DWORD dwDetailLevel, const BOOL bRefresh) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhEnumObjects");
  if (pPdhEnumObjects == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjects :(");
  return pdh_error(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
}
pdh_error ThreadedSafePDH::PdhEnumObjectItems(const LPCWSTR szDataSource, const LPCWSTR szMachineName, const LPCWSTR szObjectName, const LPWSTR mszCounterList,
                                              const LPDWORD pcchCounterListLength, const LPWSTR mszInstanceList, const LPDWORD pcchInstanceListLength,
                                              const DWORD dwDetailLevel, const DWORD dwFlags) {
  boost::unique_lock<boost::shared_mutex> lock(mutex_);
  if (!lock.owns_lock()) throw pdh_exception("Failed to get mutex for PdhEnumObjectItems");
  if (pPdhEnumObjectItems == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjectItems :(");
  return pdh_error(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList,
                                       pcchInstanceListLength, dwDetailLevel, dwFlags));
}
}  // namespace PDH