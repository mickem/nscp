// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
#include <win/com_helpers.hpp>

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

// System-wide pending-reboot signal. Windows Update creates the "RebootRequired"
// key once a reboot is queued (including for updates already installed, which
// per-update reboot_required no longer reflects). The key's mere existence is the
// signal.
bool system_reboot_pending() {
  HKEY key = nullptr;
  const LSTATUS r =
      RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired", 0, KEY_READ, &key);
  if (r == ERROR_SUCCESS) {
    RegCloseKey(key);
    return true;
  }
  return false;
}

}  // namespace

void classify_update(const std::string &category, const std::string &severity, update_info &out) {
  out.category = category;
  out.severity = severity;
  const std::string lc_cat = boost::to_lower_copy(category);
  const std::string lc_sev = boost::to_lower_copy(severity);
  out.is_security = lc_cat.find("security") != std::string::npos;
  out.is_critical = lc_cat.find("critical") != std::string::npos || lc_sev == "critical";
  // Defender/definition updates churn daily and admins threshold them separately
  // from OS patches (categories "Definition Updates" / "Microsoft Defender Antivirus").
  out.is_defender = lc_cat.find("definition") != std::string::npos || lc_cat.find("defender") != std::string::npos;
  // Monthly quality "Update Rollup" category.
  out.is_rollup = lc_cat.find("rollup") != std::string::npos;
}

std::string os_updates_obj::get_titles() const {
  std::string ret;
  for (const auto &u : updates) {
    if (!ret.empty()) ret += "; ";
    ret += u.title;
  }
  return ret;
}

std::string os_updates_obj::get_update_status() const {
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
  add_metric(section, "defender", defender);
  add_metric(section, "rollups", rollups);
  add_metric(section, "reboot_required", reboot_required);
}

void os_updates_obj::recompute() {
  count = static_cast<long long>(updates.size());
  security = 0;
  critical = 0;
  important = 0;
  defender = 0;
  rollups = 0;
  reboot_required = 0;
  for (const auto &u : updates) {
    if (u.is_security) security++;
    if (u.is_critical) critical++;
    if (boost::iequals(u.severity, "Important")) important++;
    if (u.is_defender) defender++;
    if (u.is_rollup) rollups++;
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

// Minimal ISearchCompletedCallback: WUA invokes it (on one of its own threads)
// when an asynchronous search finishes, whether it completed, failed or was
// aborted. It owns the completion event so the handle stays valid for as long
// as WUA holds a reference, even if the searching thread has already given up
// on the job.
class search_completed_callback final : public ISearchCompletedCallback {
  LONG refs_;
  HANDLE done_;

 public:
  search_completed_callback() : refs_(1), done_(CreateEvent(nullptr, TRUE, FALSE, nullptr)) {}
  search_completed_callback(const search_completed_callback &) = delete;
  search_completed_callback &operator=(const search_completed_callback &) = delete;

  HANDLE done_event() const { return done_; }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (ppv == nullptr) return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(ISearchCompletedCallback)) {
      *ppv = static_cast<ISearchCompletedCallback *>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
  ULONG STDMETHODCALLTYPE Release() override {
    const LONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) delete this;
    return static_cast<ULONG>(refs);
  }
  HRESULT STDMETHODCALLTYPE Invoke(ISearchJob *, ISearchCompletedCallbackArgs *) override {
    if (done_ != nullptr) SetEvent(done_);
    return S_OK;
  }

 private:
  ~search_completed_callback() {
    if (done_ != nullptr) CloseHandle(done_);
  }
};

// How long to wait for WUA to acknowledge RequestAbort() before giving up on
// the job. WUA normally completes an aborted search within a second; the cap
// only matters if it does not, in which case the job is released without
// EndSearch() rather than holding shutdown hostage.
const DWORD abort_grace_ms = 5000;

bool abort_requested(HANDLE abort_event) { return abort_event != nullptr && WaitForSingleObject(abort_event, 0) == WAIT_OBJECT_0; }

// Run the WUA search. Returns false when the search was abandoned because
// abort_event was signalled (out is then left as "pending" and must not be
// published); throws on any WUA failure.
//
// The search runs asynchronously (BeginSearch) rather than through the
// blocking Search() so that a stop request can interrupt it: the synchronous
// call goes online to Windows Update / WSUS and routinely takes minutes on a
// server, and the collector thread that issued it cannot be joined until it
// returns. That is what made the service take minutes to stop when the first
// search of a fresh start was still running (#1504).
bool perform_wua_search(os_updates_obj &out, HANDLE abort_event) {
  out.updates.clear();
  out.fetch_succeeded = false;
  out.error.clear();

  // A stop that arrived before we got here: do not even touch WUA.
  if (abort_requested(abort_event)) return false;

  // Scoped COM init (tolerates RPC_E_CHANGED_MODE): balanced on every exit
  // path, including the exceptions thrown below.
  const com_helper::mta_scope com;

  {
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

    com_ptr<search_completed_callback> callback;
    *callback.out() = new search_completed_callback();
    if (callback->done_event() == nullptr) {
      out.error = "Failed to create the WUA search completion event";
      throw nsclient::nsclient_exception(out.error);
    }

    bstr_holder criteria(L"IsInstalled=0 and Type='Software' and IsHidden=0");
    VARIANT state;
    VariantInit(&state);
    com_ptr<ISearchJob> job;
    hr = searcher->BeginSearch(criteria.b, static_cast<IUnknown *>(callback.get()), state, job.out());
    if (FAILED(hr) || !job.get()) {
      out.error = "WUA BeginSearch() failed (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }

    // Block until WUA reports the job complete or a stop is requested.
    HANDLE handles[2] = {callback->done_event(), abort_event};
    const DWORD handle_count = abort_event != nullptr ? 2 : 1;
    const DWORD wait = WaitForMultipleObjects(handle_count, handles, FALSE, INFINITE);
    if (wait != WAIT_OBJECT_0) {
      // Stop requested (or the wait itself failed, which we treat the same
      // way: there is no point sitting on a search nobody will read).
      job->RequestAbort();
      // Let WUA wind the job down so EndSearch() releases it cleanly; if it
      // does not acknowledge in time, just drop our reference and go.
      if (WaitForSingleObject(callback->done_event(), abort_grace_ms) == WAIT_OBJECT_0) {
        com_ptr<ISearchResult> discarded;
        searcher->EndSearch(job.get(), discarded.out());
      }
      return false;
    }

    com_ptr<ISearchResult> results;
    hr = searcher->EndSearch(job.get(), results.out());
    if (FAILED(hr) || !results.get()) {
      out.error = "WUA EndSearch() failed (HRESULT=0x" + str::xtos(static_cast<long>(hr)) + ")";
      throw nsclient::nsclient_exception(out.error);
    }
    OperationResultCode result_code = orcNotStarted;
    if (SUCCEEDED(results->get_ResultCode(&result_code)) && result_code != orcSucceeded && result_code != orcSucceededWithErrors) {
      out.error = "WUA search did not succeed (result code " + str::xtos(static_cast<long>(result_code)) + ")";
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
  }
  return true;
}

}  // namespace

bool os_updates_data::force_fetch(const threads::stop_signal *stop) {
  if (!fetch_supported_) return true;
  os_updates_obj tmp;
  try {
    // An aborted search publishes nothing: the collector is shutting down and
    // a half-finished result would only replace good cached data with
    // "pending".
    if (!perform_wua_search(tmp, stop != nullptr ? stop->native_handle() : nullptr)) return false;
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
  return true;
}

bool os_updates_data::fetch(const threads::stop_signal *stop) {
  if (!fetch_supported_) return true;
  if (last_fetch_ >= 0 && (now_seconds() - last_fetch_) < ttl_seconds_) return true;
  return force_fetch(stop);
}

os_updates_obj os_updates_data::get() {
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!read_lock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading OS updates data");
  return data_;
}

namespace check {

typedef os_updates_obj filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler final : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  // Distinct perf suffixes so each counter renders as its own series
  // ('updates_count', 'updates_security', ...) rather than collapsing onto the
  // shared 'updates' perf-syntax alias.
  registry_.add_int_var("updates", &filter_obj::get_count, "Total number of available updates")
      .add_int_perf("", "", "_count")
      .add_int_var("count", &filter_obj::get_count, "Deprecated alias for updates (the name clashes with the generic count summary keyword).")
      .add_int_perf("", "", "_count")
      .add_int_var("security", &filter_obj::get_security, "Number of security updates")
      .add_int_perf("", "", "_security")
      .add_int_var("critical", &filter_obj::get_critical, "Number of critical updates")
      .add_int_perf("", "", "_critical")
      .add_int_var("important", &filter_obj::get_important, "Number of updates with MSRC severity 'Important'")
      .add_int_perf("", "", "_important")
      .add_int_var("defender", &filter_obj::get_defender, "Number of Defender/definition updates (churn daily; threshold separately)")
      .add_int_perf("", "", "_defender")
      .add_int_var("rollups", &filter_obj::get_rollups, "Number of update-rollup updates")
      .add_int_perf("", "", "_rollups")
      .add_int_var("reboot_required", &filter_obj::get_reboot_required, "Number of updates requiring a reboot")
      .add_int_perf("", "", "_reboot_required")
      .add_int_var("reboot_pending", &filter_obj::get_reboot_pending,
                   "1 if the system has a pending reboot queued (registry RebootRequired), even from already-installed updates")
      .add_int_perf("", "", "_reboot_pending");
  registry_.add_string_var("titles", &filter_obj::get_titles, "Semicolon separated list of available update titles")
      .add_string_var("update_status", &filter_obj::get_update_status, "Aggregated status: ok, warning, critical, pending, error")
      .add_string_var("error", &filter_obj::get_error, "Last error message from the WUA search (if any)");
  // clang-format on
}

void check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, os_updates_obj data) {
  modern_filter::data_container mdata;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, mdata);

  filter_type filter;
  std::string update_filter;
  filter_helper.add_options("updates > 0", "security > 0 or critical > 0", "", filter.get_filter_syntax(), "ok");
  // Top-syntax renders with no record attached (record variables read as 0
  // there), so the update counts are rendered via ${list} from the
  // detail-syntax. The old top-syntax referenced ${count}, which resolved to
  // the generic matched-row count (always 1) rather than the number of updates.
  filter_helper.add_syntax("${status}: ${list}", "${updates} updates available (${security} security, ${critical} critical)", "updates", "",
                           "%(status): No updates available.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("update-filter", boost::program_options::value<std::string>(&update_filter),
     "Only count updates whose title contains this (case-insensitive) substring. The counters and titles are recomputed over the matching subset.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  // Title include-filter: keep only updates whose title contains the requested
  // substring, then recompute the aggregate counters over that subset.
  if (!update_filter.empty()) {
    const std::string needle = boost::to_lower_copy(update_filter);
    std::vector<update_info> kept;
    for (const update_info &u : data.updates) {
      if (boost::to_lower_copy(u.title).find(needle) != std::string::npos) kept.push_back(u);
    }
    data.updates.swap(kept);
    data.recompute();
  }

  // System-wide pending-reboot flag (registry), independent of the update list.
  data.reboot_pending = system_reboot_pending();

  const std::shared_ptr<filter_obj> record(new filter_obj(std::move(data)));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace check

}  // namespace os_updates_check
