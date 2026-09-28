// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include <Windows.h>

#include <chrono>
#include <error/error.hpp>
#include <memory>
#include <str/utf8.hpp>
#include <win/eventlog/modern_eventlog.hpp>
#include <win/pdh/pdh_enumerations.hpp>
#include <win/pdh/pdh_query.hpp>

#include "check_nps_event_values.hpp"

namespace check_nps {
namespace {
struct service_closer {
  void operator()(SC_HANDLE value) const {
    if (value) CloseServiceHandle(value);
  }
};
using service_handle = std::unique_ptr<SC_HANDLE__, service_closer>;

void require_audit(bool accounting) {
  struct policy {
    GUID subcategory;
    ULONG information;
    GUID category;
  };
  using query_type = BOOLEAN(WINAPI *)(const GUID *, ULONG, policy **);
  using free_type = VOID(WINAPI *)(void *);
  HMODULE dll = GetModuleHandleW(L"advapi32.dll");
  auto query = reinterpret_cast<query_type>(GetProcAddress(dll, "AuditQuerySystemPolicy"));
  auto release = reinterpret_cast<free_type>(GetProcAddress(dll, "AuditFree"));
  if (!query || !release) throw std::runtime_error("NPS audit policy API unavailable");
  const GUID nps = {0x0cce9243, 0x69ae, 0x11d9, {0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30}};
  policy *result = nullptr;
  if (!query(&nps, 1, &result)) throw std::runtime_error("Cannot read NPS audit policy: " + error::lookup::last_error());
  const ULONG flags = result->information;
  release(result);
  const ULONG required = accounting ? 2 : 3;  // POLICY_AUDIT_EVENT_FAILURE / SUCCESS
  if ((flags & required) != required)
    throw std::runtime_error(accounting ? "NPS failure auditing is disabled" : "NPS success and failure auditing must both be enabled");
}

std::wstring utc_time(ULONGLONG ticks) {
  FILETIME ft{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
  SYSTEMTIME st{};
  if (!FileTimeToSystemTime(&ft, &st)) throw std::runtime_error("Cannot compute NPS scan window: " + error::lookup::last_error());
  wchar_t text[40]{};
  swprintf_s(text, L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  return text;
}
}  // namespace

void require_nps() {
  service_handle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
  if (!manager) throw std::runtime_error("Cannot open service manager: " + error::lookup::last_error());
  service_handle service(OpenServiceW(manager.get(), L"IAS", SERVICE_QUERY_STATUS));
  if (!service) {
    if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) throw std::runtime_error("NPS role is not installed (IAS service missing)");
    throw std::runtime_error("Cannot inspect IAS service: " + error::lookup::last_error());
  }
}

std::vector<event> read_events(int seconds, int max_events, bool accounting) {
  require_nps();
  require_audit(accounting);
  eventlog::api::load_procs();
  if (!eventlog::api::supports_modern()) throw std::runtime_error("Windows Event Log API unavailable");
  FILETIME ft{};
  GetSystemTimeAsFileTime(&ft);
  const ULONGLONG now = (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  const std::wstring ids = accounting ? L"EventID=6275" : L"EventID=6272 or EventID=6273 or EventID=6274";
  const std::wstring query = L"*[System[Provider[@Name='Microsoft-Windows-Security-Auditing'] and (" + ids + L") and TimeCreated[@SystemTime >= '" +
                             utc_time(now - static_cast<ULONGLONG>(seconds) * 10000000) + L"' and @SystemTime <= '" + utc_time(now) + L"']]]";
  eventlog::evt_handle results(
      eventlog::EvtQuery(nullptr, L"Security", query.c_str(), eventlog::api::EvtQueryChannelPath | eventlog::api::EvtQueryReverseDirection));
  if (!results.get()) throw std::runtime_error("Cannot query NPS Security events: " + error::lookup::last_error());
  LPCWSTR paths[] = {L"Event/System/Provider/@Name", L"Event/System/EventID", L"Event/EventData/Data[@Name='ClientName']",
                     L"Event/EventData/Data[@Name='ClientIPAddress']", L"Event/EventData/Data[@Name='NetworkPolicyName']",
                     L"Event/EventData/Data[@Name='ProxyPolicyName']", L"Event/EventData/Data[@Name='ReasonCode']"};
  static_assert(sizeof(paths) / sizeof(paths[0]) == event_field_count, "NPS render paths must match the field positions");
  eventlog::evt_handle context(eventlog::EvtCreateRenderContext(event_field_count, paths, eventlog::api::EvtRenderContextValues));
  if (!context.get()) throw std::runtime_error("Cannot create NPS event render context: " + error::lookup::last_error());
  // Reuse an aligned buffer across events. Most events need just one render call;
  // grow only when a selected string does not fit, retaining the existing size cap.
  std::vector<eventlog::api::EVT_VARIANT> values(4096 / sizeof(eventlog::api::EVT_VARIANT));
  std::vector<event> out;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  for (;;) {
    if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("NPS event scan exceeded 15 seconds; reduce window");
    HANDLE raw = nullptr;
    DWORD returned = 0;
    if (!eventlog::EvtNext(results.get(), 1, &raw, 1000, 0, &returned)) {
      if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
      throw std::runtime_error("Cannot read NPS Security events: " + error::lookup::last_error());
    }
    eventlog::evt_handle item(raw);
    if (out.size() >= static_cast<std::size_t>(max_events)) throw std::runtime_error("NPS event limit exceeded; reduce window or raise max-events");
    DWORD bytes = 0, properties = 0;
    if (!eventlog::EvtRender(context.get(), item.get(), eventlog::api::EvtRenderEventValues,
                            static_cast<DWORD>(values.size() * sizeof(values[0])), values.data(), &bytes, &properties)) {
      const DWORD status = GetLastError();
      if (status != ERROR_INSUFFICIENT_BUFFER) throw std::runtime_error("Cannot render NPS event values: " + error::lookup::last_error(status));
      if (bytes == 0 || bytes > 1024 * 1024) throw std::runtime_error("NPS event values exceed the render buffer limit");
      values.resize((bytes + sizeof(values[0]) - 1) / sizeof(values[0]));
      if (!eventlog::EvtRender(context.get(), item.get(), eventlog::api::EvtRenderEventValues,
                              static_cast<DWORD>(values.size() * sizeof(values[0])), values.data(), &bytes, &properties))
        throw std::runtime_error("Cannot render NPS event values: " + error::lookup::last_error());
    }
    out.push_back(parse_event_values(values.data(), properties));
  }
  return out;
}

std::vector<counter_value> read_counters(const std::string &object, int sample_ms) {
  require_nps();
  // Enumerate the installed counterset rather than assume names shared by
  // every Windows release. An explicit localized object name is accepted.
  const auto details = PDH::Enumerations::EnumObject(object);
  if (!details.error.empty() || details.counters.empty()) throw std::runtime_error("NPS counter object unavailable: " + object);
  PDH::PDHQuery query;
  struct pending {
    PDH::pdh_instance counter;
    counter_value value;
  };
  std::vector<pending> pending_values;
  for (const auto &name : details.counters) {
    PDH::pdh_object spec;
    spec.set_counter("\\" + object + (details.instances.empty() ? "" : "(*)") + "\\" + name);
    spec.set_alias(name);
    spec.set_instances(details.instances.empty() ? "false" : "true");
    spec.set_strategy_static();
    spec.set_type("double");
    spec.set_resolution("auto");
    auto counter = PDH::factory::create(spec);
    query.addCounter(counter);
    if (details.instances.empty()) {
      pending_values.push_back({counter, {object, name, "", 0}});
    } else {
      for (const auto &child : counter->get_instances()) {
        const std::string prefix = name + "_";
        const std::string instance = child->get_name().substr(prefix.size());
        // Include _Total explicitly: filters can select the aggregate or clients.
        pending_values.push_back({child, {object, name, instance, 0}});
      }
    }
  }
  query.open();
  query.collect();
  Sleep(static_cast<DWORD>(sample_ms));
  query.collect();
  for (const auto &counter : query.counters_) {
    const auto status = counter->collect();
    if (status.is_error()) throw PDH::pdh_exception("NPS counter sample unavailable: " + counter->get_path(), status);
  }
  std::vector<counter_value> out;
  for (auto &entry : pending_values) {
    entry.value.value = entry.counter->get_float_value();
    out.push_back(entry.value);
  }
  query.close();
  return out;
}
}  // namespace check_nps
