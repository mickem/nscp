// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <error/error.hpp>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
class NativeExternalPDH : public impl_interface {
 protected:
  typedef LONG PDH_STATUS;

  typedef PDH_STATUS(WINAPI *fpPdhLookupPerfNameByIndex)(LPCWSTR, DWORD, LPWSTR, LPDWORD);
  typedef PDH_STATUS(WINAPI *fpPdhLookupPerfIndexByName)(LPCWSTR, LPCWSTR, LPDWORD);
  typedef PDH_STATUS(WINAPI *fpPdhExpandCounterPath)(LPCWSTR, LPWSTR, LPDWORD);
  typedef PDH_STATUS(WINAPI *fpPdhGetCounterInfo)(PDH_HCOUNTER, BOOLEAN, LPDWORD, PDH_COUNTER_INFO *);
  typedef PDH_STATUS(WINAPI *fpPdhAddCounter)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
  typedef PDH_STATUS(WINAPI *fpPdhAddEnglishCounter)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
  typedef PDH_STATUS(WINAPI *fpPdhRemoveCounter)(PDH_HCOUNTER);
  typedef PDH_STATUS(WINAPI *fpPdhGetRawCounterValue)(PDH_HCOUNTER, LPDWORD, PPDH_RAW_COUNTER);
  typedef PDH_STATUS(WINAPI *fpPdhGetFormattedCounterValue)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
  typedef PDH_STATUS(WINAPI *fpPdhOpenQuery)(LPCTSTR, DWORD_PTR, PDH_HQUERY *);
  typedef PDH_STATUS(WINAPI *fpPdhCloseQuery)(PDH_HQUERY);
  typedef PDH_STATUS(WINAPI *fpPdhCollectQueryData)(PDH_HQUERY);
  typedef PDH_STATUS(WINAPI *fpPdhValidatePath)(LPCWSTR);
  typedef PDH_STATUS(WINAPI *fpPdhEnumObjects)(LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, DWORD, BOOL);
  typedef PDH_STATUS(WINAPI *fpPdhEnumObjectItems)(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, LPWSTR, LPDWORD, DWORD, DWORD);
  typedef PDH_STATUS(WINAPI *fpPdhExpandWildCardPath)(LPCTSTR, LPCTSTR, LPWSTR, LPDWORD, DWORD);

  static fpPdhLookupPerfNameByIndex pPdhLookupPerfNameByIndex;
  static fpPdhLookupPerfIndexByName pPdhLookupPerfIndexByName;
  static fpPdhExpandCounterPath pPdhExpandCounterPath;
  static fpPdhGetCounterInfo pPdhGetCounterInfo;
  static fpPdhAddCounter pPdhAddCounter;
  static fpPdhAddEnglishCounter pPdhAddEnglishCounter;
  static fpPdhRemoveCounter pPdhRemoveCounter;
  static fpPdhGetRawCounterValue pPdhGetRawCounterValue;
  static fpPdhGetFormattedCounterValue pPdhGetFormattedCounterValue;
  static fpPdhOpenQuery pPdhOpenQuery;
  static fpPdhCloseQuery pPdhCloseQuery;
  static fpPdhCollectQueryData pPdhCollectQueryData;
  static fpPdhValidatePath pPdhValidatePath;
  static fpPdhEnumObjects pPdhEnumObjects;
  static fpPdhEnumObjectItems pPdhEnumObjectItems;
  static fpPdhExpandWildCardPath pPdhExpandWildCardPath;
  static HMODULE PDH_;

 public:
  NativeExternalPDH() { load_procs(); }

  bool reload() override {
    unload_procs();
    load_procs();
    return true;
  }

  void add_listener(subscriber *) override {}
  void remove_listener(subscriber *) override {}

 protected:
  static void unload_procs() {
    pPdhLookupPerfNameByIndex = nullptr;
    pPdhLookupPerfIndexByName = nullptr;
    pPdhExpandCounterPath = nullptr;
    pPdhGetCounterInfo = nullptr;
    pPdhAddCounter = nullptr;
    pPdhAddEnglishCounter = nullptr;
    pPdhRemoveCounter = nullptr;
    pPdhGetRawCounterValue = nullptr;
    pPdhGetFormattedCounterValue = nullptr;
    pPdhOpenQuery = nullptr;
    pPdhCloseQuery = nullptr;
    pPdhCollectQueryData = nullptr;
    pPdhValidatePath = nullptr;
    pPdhEnumObjects = nullptr;
    pPdhEnumObjectItems = nullptr;
    pPdhExpandWildCardPath = nullptr;

    FreeLibrary(PDH_);
    PDH_ = nullptr;
  }

  static void load_procs() {
    PDH_ = ::LoadLibrary(L"PDH");

    if (PDH_ == nullptr) {
      throw pdh_exception("LoadLibrary for PDH failed: " + error::lookup::last_error());
    }
    pPdhLookupPerfNameByIndex = reinterpret_cast<fpPdhLookupPerfNameByIndex>(::GetProcAddress(PDH_, "PdhLookupPerfNameByIndexW"));
    pPdhLookupPerfIndexByName = reinterpret_cast<fpPdhLookupPerfIndexByName>(::GetProcAddress(PDH_, "PdhLookupPerfIndexByNameW"));
    pPdhExpandCounterPath = reinterpret_cast<fpPdhExpandCounterPath>(::GetProcAddress(PDH_, "PdhExpandCounterPathW"));
    pPdhGetCounterInfo = reinterpret_cast<fpPdhGetCounterInfo>(::GetProcAddress(PDH_, "PdhGetCounterInfoW"));
    pPdhAddCounter = reinterpret_cast<fpPdhAddCounter>(::GetProcAddress(PDH_, "PdhAddCounterW"));
    pPdhAddEnglishCounter = reinterpret_cast<fpPdhAddEnglishCounter>(::GetProcAddress(PDH_, "PdhAddEnglishCounterW"));
    pPdhOpenQuery = reinterpret_cast<fpPdhOpenQuery>(::GetProcAddress(PDH_, "PdhOpenQueryW"));
    pPdhValidatePath = reinterpret_cast<fpPdhValidatePath>(::GetProcAddress(PDH_, "PdhValidatePathW"));
    pPdhEnumObjects = reinterpret_cast<fpPdhEnumObjects>(::GetProcAddress(PDH_, "PdhEnumObjectsW"));
    pPdhEnumObjectItems = reinterpret_cast<fpPdhEnumObjectItems>(::GetProcAddress(PDH_, "PdhEnumObjectItemsW"));
    pPdhExpandWildCardPath = reinterpret_cast<fpPdhExpandWildCardPath>(::GetProcAddress(PDH_, "PdhExpandWildCardPathW"));
    pPdhRemoveCounter = reinterpret_cast<fpPdhRemoveCounter>(::GetProcAddress(PDH_, "PdhRemoveCounter"));
    pPdhGetRawCounterValue = reinterpret_cast<fpPdhGetRawCounterValue>(::GetProcAddress(PDH_, "PdhGetRawCounterValue"));
    pPdhGetFormattedCounterValue = reinterpret_cast<fpPdhGetFormattedCounterValue>(::GetProcAddress(PDH_, "PdhGetFormattedCounterValue"));
    pPdhCloseQuery = reinterpret_cast<fpPdhCloseQuery>(::GetProcAddress(PDH_, "PdhCloseQuery"));
    pPdhCollectQueryData = reinterpret_cast<fpPdhCollectQueryData>(::GetProcAddress(PDH_, "PdhCollectQueryData"));
  }

 public:
  pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD *dwIndex) override {
    if (pPdhLookupPerfIndexByName == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfIndexByName");
    return pdh_error(pPdhLookupPerfIndexByName(szMachineName, szName, dwIndex));
  }

  pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) override {
    if (pPdhLookupPerfNameByIndex == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex");
    return pdh_error(pPdhLookupPerfNameByIndex(szMachineName, dwNameIndex, szNameBuffer, pcchNameBufferSize));
  }

  pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) override {
    if (pPdhExpandCounterPath == nullptr) throw pdh_exception("Failed to initialize PdhLookupPerfNameByIndex");
    return pdh_error(pPdhExpandCounterPath(szWildCardPath, mszExpandedPathList, pcchPathListLength));
  }
  pdh_error PdhGetCounterInfo(PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO *lpBuffer) override {
    if (pPdhGetCounterInfo == nullptr) throw pdh_exception("Failed to initialize PdhGetCounterInfo");
    return pdh_error(pPdhGetCounterInfo(hCounter, bRetrieveExplainText, pdwBufferSize, lpBuffer));
  }
  pdh_error PdhAddCounter(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER *phCounter) override {
    if (pPdhAddCounter == nullptr) throw pdh_exception("Failed to initialize PdhAddCounter");
    return pdh_error(pPdhAddCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
  }
  pdh_error PdhAddEnglishCounter(PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH_HCOUNTER *phCounter) override {
    if (pPdhAddEnglishCounter == nullptr) throw pdh_exception("PdhAddEnglishCounter is only available on Vista and later you need to use localized counters.");
    return pdh_error(pPdhAddEnglishCounter(hQuery, szFullCounterPath, dwUserData, phCounter));
  }
  pdh_error PdhRemoveCounter(PDH_HCOUNTER hCounter) override {
    if (pPdhRemoveCounter == nullptr) throw pdh_exception("Failed to initialize PdhRemoveCounter");
    return pdh_error(pPdhRemoveCounter(hCounter));
  }
  pdh_error PdhGetRawCounterValue(PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER pValue) override {
    if (pPdhGetRawCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetRawCounterValue");
    return pdh_error(pPdhGetRawCounterValue(hCounter, dwFormat, pValue));
  }
  pdh_error PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) override {
    if (pPdhGetFormattedCounterValue == nullptr) throw pdh_exception("Failed to initialize PdhGetFormattedCounterValue");
    return pdh_error(pPdhGetFormattedCounterValue(hCounter, dwFormat, lpdwType, pValue));
  }
  pdh_error PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH_HQUERY *phQuery) override {
    if (pPdhOpenQuery == nullptr) throw pdh_exception("Failed to initialize PdhOpenQuery");
    return pdh_error(pPdhOpenQuery(szDataSource, dwUserData, phQuery));
  }
  pdh_error PdhCloseQuery(PDH_HQUERY hQuery) override {
    if (pPdhCloseQuery == nullptr) throw pdh_exception("Failed to initialize PdhCloseQuery");
    return pdh_error(pPdhCloseQuery(hQuery));
  }
  pdh_error PdhCollectQueryData(PDH_HQUERY hQuery) override {
    if (pPdhCollectQueryData == nullptr) throw pdh_exception("Failed to initialize PdhCollectQueryData");
    return pdh_error(pPdhCollectQueryData(hQuery));
  }
  pdh_error PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) override {
    if (pPdhValidatePath == nullptr) throw pdh_exception("Failed to initialize PdhValidatePath");
    pdh_error status = pdh_error(pPdhValidatePath(szFullPathBuffer));
    if (status.is_error() && force_reload) {
      reload();
      status = pdh_error(pPdhValidatePath(szFullPathBuffer));
    }
    return status;
  }
  pdh_error PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel,
                           BOOL bRefresh) override {
    if (pPdhEnumObjects == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjects");
    return pdh_error(pPdhEnumObjects(szDataSource, szMachineName, mszObjectList, pcchBufferSize, dwDetailLevel, bRefresh));
  }
  pdh_error PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength,
                               LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) override {
    if (pPdhEnumObjectItems == nullptr) throw pdh_exception("Failed to initialize PdhEnumObjectItems");
    return pdh_error(pPdhEnumObjectItems(szDataSource, szMachineName, szObjectName, mszCounterList, pcchCounterListLength, mszInstanceList,
                                         pcchInstanceListLength, dwDetailLevel, dwFlags));
  }
  pdh_error PdhExpandWildCardPath(LPCTSTR szDataSource, LPCTSTR szWildCardPath, LPWSTR mszExpandedPathList, LPDWORD pcchPathListLength,
                                  DWORD dwFlags) override {
    if (pPdhExpandWildCardPath == nullptr) throw pdh_exception("Failed to initialize PdhExpandWildCardPath");
    return pdh_error(pPdhExpandWildCardPath(szDataSource, szWildCardPath, mszExpandedPathList, pcchPathListLength, dwFlags));
  }
};
}  // namespace PDH