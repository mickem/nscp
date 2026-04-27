/*
 * Copyright (C) 2004-2026 Michael Medin
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

#include "check_os_updates.hpp"

#include <Windows.h>
#include <comdef.h>
#include <wuapi.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/locks.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <utility>

namespace os_updates_check {

namespace {

std::string bstr_to_string(const BSTR b) {
  if (!b) return {};
  const std::wstring ws(b, SysStringLen(b));
  return utf8::cvt<std::string>(ws);
}

// RAII helper for IUnknown derived pointers. Avoids requiring _COM_SMARTPTR_TYPEDEF in headers.
template <typename T>
struct com_ptr {
  T *p;
  com_ptr() : p(nullptr) {}
  ~com_ptr() {
    if (p) p->Release();
  }
  T **out() { return &p; }
  T *operator->() const { return p; }
  T *get() const { return p; }
  com_ptr(const com_ptr &) = delete;
  com_ptr &operator=(const com_ptr &) = delete;
};

struct bstr_holder {
  BSTR b;
  explicit bstr_holder(const wchar_t *s) : b(SysAllocString(s)) {}
  ~bstr_holder() {
    if (b) SysFreeString(b);
  }
  bstr_holder(const bstr_holder &) = delete;
  bstr_holder &operator=(const bstr_holder &) = delete;
};

long long now_seconds() { return std::time(nullptr); }

}  // namespace

void classify_update(const std::string &category, const std::string &severity, update_info &out) {
  out.category = category;
  out.severity = severity;
  const std::string lc_cat = boost::to_lower_copy(category);
  const std::string lc_sev = boost::to_lower_copy(severity);
  out.is_security = lc_cat.find("security") != std::string::npos;
  out.is_critical = lc_cat.find("critical") != std::string::npos || lc_sev == "critical";
}

std::string os_updates_obj::get_titles() const {
  std::string ret;
  for (const auto &u : updates) {
    if (!ret.empty()) ret += "; ";
    ret += u.title;
  }
  return ret;
}

std::string os_updates_obj::get_status() const {
  if (!fetch_succeeded) return error.empty() ? "pending" : "error";
  if (count == 0) return "ok";
  if (security > 0 || critical > 0) return "critical";
  return "warning";
}

std::string os_updates_obj::show() const {
  if (!fetch_succeeded) {
    return error.empty() ? "update status pending" : ("update query failed: " + error);
  }
  if (count == 0) return "no updates available";
  std::string ret = str::xtos(count) + " updates available";
  if (security > 0 || critical > 0) {
    ret += " (";
    bool first = true;
    if (critical > 0) {
      ret += str::xtos(critical) + " critical";
      first = false;
    }
    if (security > 0) {
      if (!first) ret += ", ";
      ret += str::xtos(security) + " security";
    }
    ret += ")";
  }
  return ret;
}

void os_updates_obj::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;
  add_metric(section, "count", count);
  add_metric(section, "security", security);
  add_metric(section, "critical", critical);
  add_metric(section, "important", important);
  add_metric(section, "reboot_required", reboot_required);
}

void os_updates_obj::recompute() {
  count = static_cast<long long>(updates.size());
  security = 0;
  critical = 0;
  important = 0;
  reboot_required = 0;
  for (const auto &u : updates) {
    if (u.is_security) security++;
    if (u.is_critical) critical++;
    if (boost::iequals(u.severity, "Important")) important++;
    if (u.reboot_required) reboot_required++;
  }
}

os_updates_data::os_updates_data() : last_fetch_(-1), ttl_seconds_(3600), fetch_supported_(true) {}

namespace {

// Read the primary category name and is_security/is_critical from an IUpdate.
void read_update(IUpdate *update, update_info &info) {
  BSTR title = nullptr;
  if (SUCCEEDED(update->get_Title(&title)) && title) {
    info.title = bstr_to_string(title);
    SysFreeString(title);
  }
  BSTR severity = nullptr;
  std::string sev_s;
  if (SUCCEEDED(update->get_MsrcSeverity(&severity)) && severity) {
    sev_s = bstr_to_string(severity);
    SysFreeString(severity);
  }

  // Read first category name (if any).
  std::string cat_s;
  com_ptr<ICategoryCollection> categories;
  if (SUCCEEDED(update->get_Categories(categories.out())) && categories.get()) {
    LONG count = 0;
    if (SUCCEEDED(categories->get_Count(&count)) && count > 0) {
      com_ptr<ICategory> cat;
      if (SUCCEEDED(categories->get_Item(0, cat.out())) && cat.get()) {
        BSTR cat_name = nullptr;
        if (SUCCEEDED(cat->get_Name(&cat_name)) && cat_name) {
          cat_s = bstr_to_string(cat_name);
          SysFreeString(cat_name);
        }
      }
    }
  }

  classify_update(cat_s, sev_s, info);

  // Reboot required (only available on IUpdate2).
  com_ptr<IUpdate2> update2;
  if (SUCCEEDED(update->QueryInterface(__uuidof(IUpdate2), reinterpret_cast<void **>(update2.out()))) && update2.get()) {
    VARIANT_BOOL reboot = VARIANT_FALSE;
    if (SUCCEEDED(update2->get_RebootRequired(&reboot))) {
      info.reboot_required = (reboot != VARIANT_FALSE);
    }
  }
}

void perform_wua_search(os_updates_obj &out) {
  out.updates.clear();
  out.fetch_succeeded = false;
  out.error.clear();

  HRESULT hr_init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool needs_uninit = SUCCEEDED(hr_init);
  // RPC_E_CHANGED_MODE means COM is already initialized in a different mode; we should not call CoUninitialize.
  if (hr_init == RPC_E_CHANGED_MODE) needs_uninit = false;

  try {
    com_ptr<IUpdateSession> session;
    HRESULT hr = CoCreateInstance(__uuidof(UpdateSession), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUpdateSession), reinterpret_cast<void **>(session.out()));
    if (FAILED(hr) || !session.get()) {
      out.error = "Failed to create IUpdateSession (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }

    com_ptr<IUpdateSearcher> searcher;
    hr = session->CreateUpdateSearcher(searcher.out());
    if (FAILED(hr) || !searcher.get()) {
      out.error = "Failed to create IUpdateSearcher (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }

    bstr_holder criteria(L"IsInstalled=0 and Type='Software' and IsHidden=0");
    com_ptr<ISearchResult> results;
    hr = searcher->Search(criteria.b, results.out());
    if (FAILED(hr) || !results.get()) {
      out.error = "WUA Search() failed (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }

    com_ptr<IUpdateCollection> collection;
    hr = results->get_Updates(collection.out());
    if (FAILED(hr) || !collection.get()) {
      out.error = "Failed to get update collection (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }

    LONG count = 0;
    collection->get_Count(&count);
    for (LONG i = 0; i < count; ++i) {
      com_ptr<IUpdate> update;
      if (FAILED(collection->get_Item(i, update.out())) || !update.get()) continue;
      update_info info;
      read_update(update.get(), info);
      out.updates.push_back(info);
    }
    out.recompute();
    out.fetch_succeeded = true;
  } catch (...) {
    if (needs_uninit) CoUninitialize();
    throw;
  }
  if (needs_uninit) CoUninitialize();
}

}  // namespace

void os_updates_data::force_fetch() {
  if (!fetch_supported_) return;
  os_updates_obj tmp;
  try {
    perform_wua_search(tmp);
  } catch (const std::exception &e) {
    tmp.fetch_succeeded = false;
    if (tmp.error.empty()) tmp.error = e.what();
  } catch (...) {
    tmp.fetch_succeeded = false;
    if (tmp.error.empty()) tmp.error = "unknown WUA error";
  }
  {
    boost::unique_lock<boost::shared_mutex> write_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!write_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing OS updates data");
    data_ = tmp;
    last_fetch_ = now_seconds();
  }
}

void os_updates_data::fetch() {
  if (!fetch_supported_) return;
  if (last_fetch_ >= 0 && (now_seconds() - last_fetch_) < ttl_seconds_) return;
  force_fetch();
}

os_updates_obj os_updates_data::get() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading OS updates data");
  return data_;
}

namespace check {

typedef os_updates_obj filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler final : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_int_x("count", &filter_obj::get_count, "Total number of available updates")
      .add_int_perf("")
      .add_int_x("security", &filter_obj::get_security, "Number of security updates")
      .add_int_perf("")
      .add_int_x("critical", &filter_obj::get_critical, "Number of critical updates")
      .add_int_perf("")
      .add_int_x("important", &filter_obj::get_important, "Number of updates with MSRC severity 'Important'")
      .add_int_perf("")
      .add_int_x("reboot_required", &filter_obj::get_reboot_required, "Number of updates requiring a reboot")
      .add_int_perf("");
  registry_.add_string("titles", &filter_obj::get_titles, "Semicolon separated list of available update titles")
      .add_string("status", &filter_obj::get_status, "Aggregated status: ok, warning, critical, pending, error")
      .add_string("error", &filter_obj::get_error, "Last error message from the WUA search (if any)");
  // clang-format on
}

void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, os_updates_obj data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  filter_helper.add_options("count > 0", "security > 0 or critical > 0", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${count} updates available (${security} security, ${critical} critical)",
                           "${count} updates (${security} security, ${critical} critical)", "updates", "", "%(status): No updates available.");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  const boost::shared_ptr<filter_obj> record(new filter_obj(std::move(data)));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace os_updates_check
