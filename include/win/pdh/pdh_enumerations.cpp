// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <bytes/buffer.hpp>
#include <error/error.hpp>
#include <list>
#include <str/utf8.hpp>
#include <win/pdh/pdh_enumerations.hpp>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
namespace {
  // RAII guard for a temporary PDH query handle. The destructor swallows the
  // PdhCloseQuery status so we can use it in cleanup-on-error paths without
  // double-reporting; if cleanup itself fails the caller will already have a
  // more useful error from the original failure.
  struct ScopedPdhQuery {
    PDH_HQUERY h = nullptr;
    ~ScopedPdhQuery() {
      if (h != nullptr) {
        try {
          factory::get_impl()->PdhCloseQuery(h);
        } catch (...) {
        }
      }
    }
  };
  struct ScopedPdhCounter {
    PDH_HCOUNTER h = nullptr;
    ~ScopedPdhCounter() {
      if (h != nullptr) {
        try {
          factory::get_impl()->PdhRemoveCounter(h);
        } catch (...) {
        }
      }
    }
  };

  // Resolve a counter path via the temporary-query trick: open a query, add
  // the (possibly English) counter, ask PDH for its full localized path, and
  // return that. All handles are released on every exit path.
  bool resolve_path_via_temp_query(const std::wstring &input, std::wstring &resolved_out, std::string &error_out) {
    ScopedPdhQuery query;
    pdh_error status = factory::get_impl()->PdhOpenQuery(nullptr, 0, &query.h);
    if (status.is_error()) {
      error_out = status.get_message();
      return false;
    }
    ScopedPdhCounter counter;
    status = factory::get_impl()->PdhAddEnglishCounter(query.h, input.c_str(), 0, &counter.h);
    if (status.is_error()) {
      error_out = status.get_message();
      return false;
    }
    hlp::buffer<TCHAR, PDH_COUNTER_INFO *> info_buf(2048);
    auto info_size = static_cast<DWORD>(info_buf.size());
    status = factory::get_impl()->PdhGetCounterInfo(counter.h, FALSE, &info_size, info_buf.get());
    if (status.is_error()) {
      error_out = status.get_message();
      return false;
    }
    resolved_out = info_buf.get()->szFullPath;
    return true;
  }
}  // namespace

std::list<std::string> Enumerations::expand_wild_card_path(const std::string &query, std::string &error) {
  std::list<std::string> ret;
  auto wquery = utf8::cvt<std::wstring>(query);
  hlp::buffer<TCHAR> buffer(1024);
  auto dwBufLen = static_cast<DWORD>(buffer.size());
  try {
    pdh_error status = factory::get_impl()->PdhExpandWildCardPath(nullptr, wquery.c_str(), buffer, &dwBufLen, 0);
    if (status.is_more_data()) {
      buffer.resize(dwBufLen + 10);
      dwBufLen = static_cast<DWORD>(buffer.size());
      status = factory::get_impl()->PdhExpandWildCardPath(nullptr, wquery.c_str(), buffer, &dwBufLen, 0);
    }
    if (status.is_not_found()) {
      // PDH couldn't expand the path directly. Try resolving it via a temp
      // query (which lets PdhAddEnglishCounter translate an English path
      // into its localized form), then recurse with the localized path so
      // wildcard expansion can complete.
      std::wstring resolved;
      if (!resolve_path_via_temp_query(wquery, resolved, error)) {
        return ret;
      }
      error.clear();
      return expand_wild_card_path(utf8::cvt<std::string>(resolved), error);
    }
    if (status.is_error()) {
      error = status.get_message();
      return ret;
    }
    if (dwBufLen > 0) {
      TCHAR *cp = buffer.get();
      while (*cp != L'\0') {
        std::wstring tmp = cp;
        ret.push_back(utf8::cvt<std::string>(tmp));
        cp += wcslen(cp) + 1;
      }
    }
  } catch (std::exception &e) {
    error = utf8::utf8_from_native(e.what());
  } catch (...) {
    error = "Unknown exception";
  }
  return ret;
}
void Enumerations::fetch_object_details(Object &object, bool instances, bool objects, DWORD dwDetailLevel) {
  DWORD dwCounterBufLen = 0;
  DWORD dwInstanceBufLen = 0;
  try {
    // First call with null buffers to learn the required sizes.
    pdh_error status = factory::get_impl()->PdhEnumObjectItems(nullptr, nullptr, utf8::cvt<std::wstring>(object.name).c_str(), nullptr, &dwCounterBufLen,
                                                               nullptr, &dwInstanceBufLen, dwDetailLevel, 0);
    if (!status.is_more_data()) {
      object.error = "Failed to enumerate object: " + object.name;
      return;
    }

    hlp::buffer<TCHAR> counterBuffer(dwCounterBufLen + 1);
    hlp::buffer<TCHAR> instanceBuffer(dwInstanceBufLen + 1);

    status = factory::get_impl()->PdhEnumObjectItems(nullptr, nullptr, utf8::cvt<std::wstring>(object.name).c_str(), counterBuffer.get(), &dwCounterBufLen,
                                                     instanceBuffer.get(), &dwInstanceBufLen, dwDetailLevel, 0);
    if (status.is_error()) {
      object.error = "Failed to enumerate object: " + object.name;
      return;
    }

    if (dwCounterBufLen > 0 && objects) {
      const TCHAR *cp = counterBuffer.get();
      while (*cp != '\0') {
        object.counters.push_back(utf8::cvt<std::string>(cp));
        cp += lstrlen(cp) + 1;
      }
    }
    if (dwInstanceBufLen > 0 && instances) {
      const TCHAR *cp = instanceBuffer.get();
      while (*cp != '\0') {
        object.instances.push_back(utf8::cvt<std::string>(cp));
        cp += lstrlen(cp) + 1;
      }
    }
  } catch (std::exception &e) {
    object.error = e.what();
  } catch (...) {
    object.error = "Exception fetching data for: " + object.name;
  }
}
Enumerations::Objects Enumerations::EnumObjects(bool instances, bool objects, DWORD dwDetailLevel) {
  Objects ret;

  DWORD dwObjectBufLen = 0;
  pdh_error status = factory::get_impl()->PdhEnumObjects(nullptr, nullptr, nullptr, &dwObjectBufLen, dwDetailLevel, FALSE);
  if (!status.is_more_data()) throw pdh_exception("PdhEnumObjects failed when trying to retrieve size of object buffer", status);

  hlp::buffer<TCHAR> objectBuffer(dwObjectBufLen + 1024);
  status = factory::get_impl()->PdhEnumObjects(nullptr, nullptr, objectBuffer.get(), &dwObjectBufLen, dwDetailLevel, FALSE);
  if (status.is_error()) throw pdh_exception("PdhEnumObjects failed when trying to retrieve object buffer", status);

  const TCHAR *cp = objectBuffer.get();
  while (*cp != '\0') {
    Object o;
    o.name = utf8::cvt<std::string>(cp);
    ret.push_back(o);
    cp += lstrlen(cp) + 1;
  }

  if (objects || instances) {
    for (Object &o : ret) {
      fetch_object_details(o, instances, objects, dwDetailLevel);
    }
  }
  return ret;
}

Enumerations::Object Enumerations::EnumObject(const std::string &object, const bool instances, const bool objects, const DWORD dwDetailLevel) {
  Object ret;
  ret.name = object;
  fetch_object_details(ret, instances, objects, dwDetailLevel);
  return ret;
}
}  // namespace PDH