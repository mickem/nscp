// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <bytes/buffer.hpp>
#include <list>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <win/pdh/pdh_resolver.hpp>

namespace PDH {
std::wstring PDHResolver::PdhLookupPerfNameByIndex(const LPCTSTR szMachineName, const DWORD dwNameIndex) {
  hlp::buffer<TCHAR> buffer(PDH_INDEX_BUF_LEN + 1);
  DWORD bufLen = PDH_INDEX_BUF_LEN;
  const pdh_error status = factory::get_impl()->PdhLookupPerfNameByIndex(szMachineName, dwNameIndex, buffer.get(), &bufLen);
  if (status.is_error()) {
    throw pdh_exception("PdhLookupPerfNameByIndex: Could not find index: " + str::xtos(dwNameIndex), status);
  }
  return std::wstring(buffer.get());
}
std::list<std::string> PDHResolver::PdhExpandCounterPath(const std::string &szWildCardPath, const DWORD buffSize) {
  hlp::buffer<TCHAR> buffer(buffSize + 1);
  DWORD bufLen = buffSize;
  const pdh_error status = factory::get_impl()->PdhExpandCounterPath(utf8::cvt<std::wstring>(szWildCardPath).c_str(), buffer.get(), &bufLen);
  if (status.is_error()) {
    if (buffSize == PDH_INDEX_BUF_LEN && bufLen > buffSize) return PdhExpandCounterPath(szWildCardPath, bufLen + 10);
    throw pdh_exception("PdhExpandCounterPath: Could not find index: " + utf8::cvt<std::string>(szWildCardPath), status);
  }
  return helpers::build_list(buffer.get(), bufLen);
}

DWORD PDHResolver::PdhLookupPerfIndexByName(const LPCTSTR szMachineName, const LPCTSTR indexName) {
  DWORD ret;
  const pdh_error status = factory::get_impl()->PdhLookupPerfIndexByName(szMachineName, indexName, &ret);
  if (status.is_error()) {
    throw pdh_exception("PdhLookupPerfNameByIndex: Could not find index: " + utf8::cvt<std::string>(indexName), status);
  }
  return ret;
}

bool PDHResolver::validate(const std::wstring &counter, std::wstring &error, const bool force_reload) {
  const pdh_error status = factory::get_impl()->PdhValidatePath(counter.c_str(), force_reload);
  if (status.is_error()) error = utf8::cvt<std::wstring>(status.get_message());
  return status.is_ok();
}
bool is_special_char(char c) { return c == '\\' || c == '(' || c == ')'; }

bool PDHResolver::expand_index(std::string &counter) {
  auto pos = 0;
  do {
    const auto p1 = counter.find_first_of("0123456789", pos);
    if (p1 == std::wstring::npos) return true;
    auto p2 = counter.find_first_not_of("0123456789", p1);
    if (p2 == std::string::npos) p2 = counter.size();
    if (p2 <= p1) return false;
    if (p1 > 0) {
      if (!is_special_char(counter[p1 - 1])) {
        pos = p2;
        continue;
      }
    }
    if (p2 < counter.size()) {
      if (!is_special_char(counter[p2])) {
        pos = p2;
        continue;
      }
    }
    const unsigned int index = str::stox<unsigned int>(counter.substr(p1, p2 - p1));
    std::string sindex = lookupIndex(index);
    counter.replace(p1, p2 - p1, sindex);
    pos = p1 + sindex.size();
  } while (true);
}

std::string PDHResolver::lookupIndex(const DWORD index) { return utf8::cvt<std::string>(PdhLookupPerfNameByIndex(nullptr, index)); }
DWORD PDHResolver::lookupIndex(const std::string &name) { return PdhLookupPerfIndexByName(nullptr, utf8::cvt<std::wstring>(name).c_str()); }
}  // namespace PDH