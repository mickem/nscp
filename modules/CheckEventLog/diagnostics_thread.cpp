// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_thread.hpp"

#include <algorithm>
#include <ctime>
#include <deque>
#include <error/error.hpp>
#include <map>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <str/utf8.hpp>
#include <threads/guarded_thread.hpp>
#include <utility>
#include <vector>
#include <win/eventlog/modern_eventlog.hpp>

#include "eventlog_wrapper.hpp"
#include "filter.hpp"

namespace {

using event_ptr = std::shared_ptr<eventlog_filter::new_filter_obj>;

std::string time_clause(const std::int64_t seed_seconds) {
  if (seed_seconds <= 0) return "";
  return " and TimeCreated[timediff(@SystemTime) <= " + std::to_string(seed_seconds * 1000) + "]";
}

struct event_closer {
  static void close(HANDLE handle) { CloseHandle(handle); }
};

// One subscription: a log, the events it selects, and the ring they go to.
// It owns the event the subscription signals and the subscription itself, so
// both are released however the thread leaves. `signal` is declared before
// `log` so that the subscription goes first and never signals a closed event.
struct feed {
  std::string channel;
  std::string query;
  bool service = false;
  hlp::handle<HANDLE, event_closer> signal;
  std::unique_ptr<eventlog_wrapper_new> log;
};

struct scm_closer {
  static void close(SC_HANDLE handle) { CloseServiceHandle(handle); }
};

// Renders events into rows, caching what is expensive to look up: the
// providers' message tables and the services' key names.
class renderer {
 public:
  renderer() {
    system_context_ = eventlog::EvtCreateRenderContext(0, nullptr, eventlog::api::EvtRenderContextSystem);
    LPCWSTR paths[] = {L"Event/EventData/Data[@Name='param1']"};
    param1_context_ = eventlog::EvtCreateRenderContext(1, paths, eventlog::api::EvtRenderContextValues);
    user_context_ = eventlog::EvtCreateRenderContext(0, nullptr, eventlog::api::EvtRenderContextUser);
    scm_ = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
  }

  const eventlog::evt_handle &system_context() const { return system_context_; }

  event_tables::event_row row(eventlog_filter::new_filter_obj &e, const bool service) {
    event_tables::event_row r;
    r.time = static_cast<std::int64_t>(e.get_written());
    r.log = e.get_log();
    r.source = e.get_provider();
    r.has_id = true;
    r.id = e.get_id();
    r.level = e.get_el_type_s();
    r.message = message(e);
    const eventlog::api::EVT_VARIANT &record = e.buffer.get()[eventlog::api::EvtSystemEventRecordId];
    if (record.Type != eventlog::api::EvtVarTypeNull) r.key = r.log + "#" + std::to_string(record.UInt64Val);
    if (service) r.service = service_name(e);
    return r;
  }

 private:
  std::string message(eventlog_filter::new_filter_obj &e) {
    const std::string provider = e.get_provider();
    auto it = publishers_.find(provider);
    if (it == publishers_.end()) {
      // A provider without metadata (an uninstalled application) stays null
      // rather than being looked up again for every event.
      it = publishers_.emplace(provider, eventlog::evt_handle(eventlog::EvtOpenPublisherMetadata(nullptr, utf8::cvt<std::wstring>(provider).c_str(), nullptr, 0, 0)))
               .first;
    }
    std::string text;
    if (it->second && eventlog::EvtFormatMessage(it->second, e.hEvent, 0, 0, nullptr, eventlog::api::EvtFormatMessageEvent, text) == ERROR_SUCCESS)
      return event_tables::message(text);
    // No message for this id (a source without a message table, or one that
    // writes ids its table does not have): what Event Viewer shows after
    // "the description cannot be found" - the event's own strings.
    return event_tables::message(strings(e));
  }

  std::string strings(eventlog_filter::new_filter_obj &e) {
    if (!user_context_) return "";
    DWORD used = 0, count = 0;
    std::vector<BYTE> buffer(1024);
    BOOL ok =
        eventlog::EvtRender(user_context_, e.hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.data(), &used, &count);
    if (!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      buffer.resize(used);
      ok = eventlog::EvtRender(user_context_, e.hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.data(), &used, &count);
    }
    if (!ok) return "";
    std::string out;
    const auto *values = reinterpret_cast<const eventlog::api::EVT_VARIANT *>(buffer.data());
    for (DWORD i = 0; i < count; ++i) {
      if (values[i].Type != eventlog::api::EvtVarTypeString || values[i].StringVal == nullptr) continue;
      if (!out.empty()) out += " ";
      out += utf8::cvt<std::string>(std::wstring(values[i].StringVal));
    }
    return out;
  }

  // The service control manager names a service by its display name (param1
  // of 7031/7034/7024); a reader matches it against check_service's name.
  std::string service_name(eventlog_filter::new_filter_obj &e) {
    std::string display;
    if (param1_context_) {
      DWORD used = 0, count = 0;
      std::vector<BYTE> buffer(512);
      BOOL ok =
          eventlog::EvtRender(param1_context_, e.hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.data(), &used, &count);
      if (!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        buffer.resize(used);
        ok = eventlog::EvtRender(param1_context_, e.hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.data(), &used,
                                 &count);
      }
      const auto *v = reinterpret_cast<const eventlog::api::EVT_VARIANT *>(buffer.data());
      if (ok && count > 0 && v->Type == eventlog::api::EvtVarTypeString && v->StringVal != nullptr)
        display = utf8::cvt<std::string>(std::wstring(v->StringVal));
    }
    if (display.empty()) return "";
    const auto cached = service_names_.find(display);
    if (cached != service_names_.end()) return cached->second;
    std::string name = display;
    if (scm_) {
      wchar_t key[257] = {};
      DWORD size = 257;
      if (GetServiceKeyNameW(scm_, utf8::cvt<std::wstring>(display).c_str(), key, &size)) name = utf8::cvt<std::string>(std::wstring(key));
    }
    // Services come and go; the map only grows with the ones that crashed.
    if (service_names_.size() > 256) service_names_.clear();
    service_names_[display] = name;
    return name;
  }

  eventlog::evt_handle system_context_;
  eventlog::evt_handle param1_context_;
  eventlog::evt_handle user_context_;
  hlp::handle<SC_HANDLE, scm_closer> scm_;
  std::map<std::string, eventlog::evt_handle> publishers_;
  std::map<std::string, std::string> service_names_;
};

// The newest events of `channel` that `query` selects, oldest first, at most
// `limit` of them: the seed of a ring.
std::vector<event_ptr> query_backwards(const std::string &channel, const std::string &query, const eventlog::evt_handle &context, const std::size_t limit) {
  std::vector<event_ptr> found;
  const eventlog::evt_handle results(eventlog::EvtQuery(nullptr, utf8::cvt<std::wstring>(channel).c_str(), utf8::cvt<std::wstring>(query).c_str(),
                                                        eventlog::api::EvtQueryChannelPath | eventlog::api::EvtQueryReverseDirection));
  if (!results) throw nsclient::nsclient_exception("Failed to query " + channel + ": " + error::lookup::last_error());
  const auto now = static_cast<unsigned long long>(std::time(nullptr));
  while (found.size() < limit) {
    eventlog::api::EVT_HANDLE events[16] = {};
    DWORD returned = 0;
    const DWORD want = static_cast<DWORD>((std::min)(static_cast<std::size_t>(16), limit - found.size()));
    if (!eventlog::EvtNext(results, want, events, 1000, 0, &returned)) break;
    for (DWORD i = 0; i < returned; ++i) {
      // The object owns the handle from here on.
      found.push_back(std::make_shared<eventlog_filter::new_filter_obj>(now, channel, events[i], context, 0));
    }
    if (returned == 0) break;
  }
  std::reverse(found.begin(), found.end());
  return found;
}

}  // namespace

std::string eventlog_diagnostics::service_events_query(const std::int64_t seed_seconds) {
  return "*[System[Provider[@Name='Service Control Manager'] and (EventID=7031 or EventID=7034 or EventID=7024)" + time_clause(seed_seconds) + "]]";
}

std::string eventlog_diagnostics::errors_query(const std::int64_t seed_seconds) {
  // Level 1 is critical, 2 error.
  return "*[System[(Level=1 or Level=2)" + time_clause(seed_seconds) + "]]";
}

bool eventlog_diagnostics::start() {
  if (!any()) return true;
  std::string error;
  if (!stop_signal_.create(error)) {
    NSC_LOG_ERROR("Failed to create stop event, the event log diagnostics are disabled: " + error);
    set_error(true, "Not collected: " + error);
    set_error(false, "Not collected: " + error);
    return false;
  }
  thread_ = threads::start_guarded_thread("checkeventlog diagnostics", [this]() { this->thread_proc(); }, NSC_THREAD_REPORTER);
  return true;
}

void eventlog_diagnostics::stop() {
  stop_signal_.signal();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
  stop_signal_.close();
}

void eventlog_diagnostics::set_error(const bool service, const std::string &error) {
  const std::lock_guard<std::mutex> guard(state_mutex_);
  (service ? services_error_ : errors_error_) = error;
}

void eventlog_diagnostics::add_sections(PB::Diagnostics::DiagnosticsMessage *out) const {
  event_tables::reader_state services, errors;
  {
    const std::lock_guard<std::mutex> guard(state_mutex_);
    services.error = services_error_;
    errors.error = errors_error_;
  }
  // A live subscription is current as of now: captured_at stays the time of
  // the copy.
  if (services_) event_tables::add_service_events(out, service_events_.copy(), services);
  if (errors_) event_tables::add_errors(out, errors_ring_.copy(), errors);
}

void eventlog_diagnostics::thread_proc() {
  if (!eventlog::api::supports_modern()) {
    // The legacy (pre-Vista) event log API has no XPath subscriptions.
    set_error(true, "Not supported: needs the Windows Vista event log API");
    set_error(false, "Not supported: needs the Windows Vista event log API");
    return;
  }

  std::vector<feed> feeds;
  if (services_) feeds.push_back({"System", service_events_query(), true});
  if (errors_) {
    feeds.push_back({"Application", errors_query(), false});
    feeds.push_back({"System", errors_query(), false});
  }

  // Subscribe first, then seed: an event written in between is in both, and
  // the rings hold it once (by its record id).
  std::vector<HANDLE> handles;
  std::vector<feed *> active;
  handles.push_back(stop_signal_.native_handle());
  for (feed &f : feeds) {
    try {
      f.log = std::make_unique<eventlog_wrapper_new>(f.channel, f.query);
      if (!f.log->notify(*f.signal.ref()) || !f.signal) {
        const std::string error = f.log->last_error.empty() ? "Failed to subscribe to " + f.channel : f.log->last_error;
        f.log.reset();
        f.signal.close();
        set_error(f.service, error);
        continue;
      }
    } catch (const nsclient::nsclient_exception &e) {
      f.log.reset();
      f.signal.close();
      set_error(f.service, e.reason());
      continue;
    }
    handles.push_back(f.signal.get());
    active.push_back(&f);
  }

  renderer render;
  const auto ring_of = [this](const bool service) -> event_tables::ring & { return service ? service_events_ : errors_ring_; };

  // The seed: the last seed_seconds of each log, newest kept.
  for (const bool service : {true, false}) {
    if (service ? !services_ : !errors_) continue;
    const std::vector<std::string> channels = service ? std::vector<std::string>{"System"} : std::vector<std::string>{"Application", "System"};
    const std::string query = service ? service_events_query(event_tables::seed_seconds) : errors_query(event_tables::seed_seconds);
    for (const std::string &channel : channels) {
      try {
        for (const event_ptr &e : query_backwards(channel, query, render.system_context(), nscapi::diagnostics::max_rows)) {
          ring_of(service).push(render.row(*e, service));
        }
      } catch (const nsclient::nsclient_exception &e) {
        // The live feed still works; the ring only starts empty.
        NSC_DEBUG_MSG("Not seeding " + std::string(service ? event_tables::section_service_events : event_tables::section_errors) + " from " + channel + ": " +
                      e.reason());
      }
    }
  }

  while (true) {
    const DWORD rc = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);
    if (rc == WAIT_OBJECT_0) break;
    if (rc > WAIT_OBJECT_0 && rc < WAIT_OBJECT_0 + handles.size()) {
      const std::size_t index = rc - WAIT_OBJECT_0;
      feed &f = *active[index - 1];
      // Reset before draining: an event written while draining signals again
      // rather than waiting for the next one.
      f.log->reset_event(handles[index]);
      // Keep only the newest of a burst before rendering any: the ring holds
      // max_rows, and rendering is the expensive part.
      std::deque<event_ptr> burst;
      std::uint64_t cut = 0;
      try {
        for (eventlog_filter::filter::object_type item = f.log->read_record(handles[index]); item; item = f.log->read_record(handles[index])) {
          event_ptr e = std::dynamic_pointer_cast<eventlog_filter::new_filter_obj>(item);
          if (!e) continue;
          burst.push_back(e);
          if (burst.size() > nscapi::diagnostics::max_rows) {
            burst.pop_front();
            ++cut;
          }
        }
      } catch (const nsclient::nsclient_exception &e) {
        NSC_LOG_ERROR("Failed to read " + f.channel + " for diagnostics: " + e.reason());
      }
      event_tables::ring &ring = ring_of(f.service);
      if (cut > 0) ring.count_dropped(cut);
      for (const event_ptr &e : burst) {
        try {
          ring.push(render.row(*e, f.service));
        } catch (const std::exception &ex) {
          NSC_DEBUG_MSG("Failed to render an event from " + f.channel + ": " + utf8::utf8_from_native(ex.what()));
        }
      }
      continue;
    }
    const std::string error = "Stopped reading the event log: " + error::lookup::last_error();
    NSC_LOG_ERROR(error);
    if (services_) set_error(true, error);
    if (errors_) set_error(false, error);
    break;
  }
}
