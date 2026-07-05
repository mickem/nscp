// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <list>
#include <win/pdh/pdh_counters.hpp>

namespace PDH {
class PDHResolver {
 public:
#define PDH_INDEX_BUF_LEN 2048

  static std::wstring PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex);
  static std::list<std::string> PdhExpandCounterPath(const std::string &szWildCardPath, DWORD buffSize = PDH_INDEX_BUF_LEN);
  static DWORD PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR indexName);
  static bool validate(const std::wstring &counter, std::wstring &error, bool force_reload);
  // static bool is_speacial_char(char c);

  static bool PDHResolver::expand_index(std::string &counter);

  static std::string lookupIndex(DWORD index);
  static DWORD lookupIndex(const std::string &name);
};
}  // namespace PDH
