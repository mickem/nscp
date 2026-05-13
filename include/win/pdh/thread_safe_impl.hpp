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

#pragma once

#include <boost/thread.hpp>
#include <win/pdh/basic_impl.hpp>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
class ThreadedSafePDH : public NativeExternalPDH {
  boost::shared_mutex mutex_;
  typedef std::list<subscriber*> subscriber_list;
  subscriber_list subscribers_;

 public:
  ThreadedSafePDH() {}

  bool reload() override;
  bool reload_unsafe();

  void add_listener(subscriber* sub) override;
  void remove_listener(subscriber* sub) override;

  pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD* dwIndex) override;
  pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) override;
  pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) override;
  pdh_error PdhGetCounterInfo(PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO* lpBuffer) override;
  pdh_error PdhAddCounter(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER* phCounter) override;
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
};
}  // namespace PDH