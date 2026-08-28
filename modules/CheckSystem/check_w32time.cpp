// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_w32time.hpp"

#include <Windows.h>

#include <boost/algorithm/string.hpp>
#include <ctime>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_query.hpp>
#include <win/registry.hpp>
#include <win/services.hpp>

#include <check/duration_keyword.hpp>

namespace w32time_check {

namespace {

const char *w32time_service = "W32Time";
const char *parameters_key = "SYSTEM\\CurrentControlSet\\Services\\W32Time\\Parameters";
const char *config_key = "SYSTEM\\CurrentControlSet\\Services\\W32Time\\Config";

// The names w32time reports when it is not actually following a time source.
// These are provider-internal strings, not MUI resources: a Swedish-locale
// Windows (where w32tm errors come out in Swedish) still reports the source as
// "Local CMOS Clock" verbatim. Should a build surface a translated variant,
// the time_sources counter path below still catches the free-running case
// whenever the live source cannot be read.
const char *local_clock_names[] = {"Local CMOS Clock", "Free-running System Clock"};

}  // namespace

std::vector<std::string> parse_ntp_servers(const std::string &raw) {
  std::vector<std::string> out;
  std::vector<std::string> tokens;
  boost::split(tokens, raw, boost::is_any_of(" \t,;"), boost::token_compress_on);
  for (std::string token : tokens) {
    boost::trim(token);
    // Each entry is "<host>,0x<flags>"; the split above already dropped the
    // comma, so a bare flag field is all that is left of it.
    if (token.empty() || boost::istarts_with(token, "0x")) continue;
    out.push_back(token);
  }
  return out;
}

bool is_local_clock(const std::string &source) {
  for (const char *name : local_clock_names) {
    if (boost::icontains(source, name)) return true;
  }
  return false;
}

boost::optional<long long> us_to_ms(const boost::optional<long long> &microseconds) {
  if (!microseconds) return boost::none;
  return *microseconds / 1000;
}

w32time_obj build_w32time_obj(const w32time_data &data, const long long now_epoch) {
  w32time_obj obj;

  obj.installed = data.installed ? 1 : 0;
  obj.service_state = data.installed ? data.service_state : "not installed";
  obj.start_type = data.start_type;
  obj.running = boost::iequals(data.service_state, "running") ? 1 : 0;
  obj.sync_type = data.sync_type.empty() ? "unknown" : data.sync_type;

  const std::vector<std::string> peers = parse_ntp_servers(data.ntp_server);
  obj.peers = boost::algorithm::join(peers, ", ");
  obj.peer_count = static_cast<long long>(peers.size());

  // Prefer what the service says it is actually following; fall back to what it
  // is configured to follow so the answer is never silently empty.
  if (!data.live_source.empty()) {
    obj.source = data.live_source;
    obj.source_from = "service";
  } else if (!obj.peers.empty()) {
    obj.source = obj.peers;
    obj.source_from = "configuration";
  } else {
    obj.source = "unknown";
    obj.source_from = "unknown";
  }
  obj.local_clock = is_local_clock(data.live_source) ? 1 : 0;

  obj.offset = us_to_ms(data.offset_us);
  obj.delay = us_to_ms(data.delay_us);
  obj.frequency_adjustment = data.frequency_adjustment_ppb;
  obj.time_sources = data.time_sources;

  const long long last_good = win_registry::filetime_to_epoch(data.last_good_filetime);
  if (last_good > 0) {
    obj.last_sync = win_registry::filetime_to_string(data.last_good_filetime);
    obj.last_sync_age = now_epoch > last_good ? now_epoch - last_good : 0;
  } else {
    obj.last_sync = "unknown";
  }

  // "Synchronized" is deliberately about the time hierarchy, not about accuracy:
  // a machine that is not running the service, has synchronisation switched off
  // or is not following any source is unsynchronised whatever the last measured
  // offset says. The evidence is ranked: the service's own answer decides when
  // we could get it (the agent can, running as a service; an unprivileged caller
  // gets ERROR_ACCESS_DENIED), otherwise the count of time sources in use does,
  // and with neither we do not cry wolf.
  const bool no_sync = boost::iequals(obj.sync_type, "NoSync");
  const bool live_source_known = obj.source_from == "service";
  const bool no_source_in_use = !live_source_known && obj.time_sources && *obj.time_sources == 0;
  if (obj.running == 0 || no_sync) {
    obj.synchronized = 0;
  } else if (live_source_known) {
    obj.synchronized = obj.local_clock == 0 ? 1 : 0;
  } else {
    obj.synchronized = no_source_in_use ? 0 : 1;
  }

  const std::string offset_suffix = obj.offset ? " (offset " + str::xtos(*obj.offset) + "ms)" : "";
  if (obj.installed == 0) {
    obj.state = "the Windows Time service is not installed";
  } else if (obj.running == 0) {
    obj.state = "the Windows Time service is " + obj.service_state + " (start type " + (obj.start_type.empty() ? "unknown" : obj.start_type) + ")";
  } else if (no_sync) {
    obj.state = "time synchronization is turned off (Type=NoSync)";
  } else if (obj.local_clock == 1) {
    obj.state = "not synchronizing: falling back to " + obj.source;
  } else if (no_source_in_use) {
    obj.state = "not synchronizing: no time source in use (configured: " + obj.source + ")";
  } else if (live_source_known) {
    obj.state = "synchronizing with " + obj.source + offset_suffix;
  } else {
    // We know what it is meant to follow, not what it is following.
    obj.state = "configured to synchronize with " + obj.source + offset_suffix;
  }

  return obj;
}

using parsers::where::type_bool;
using parsers::where::type_int;

// last_sync_age is seconds; the converter lets `last_sync_age > 24h` mean a day
// rather than being silently read as the number 24.
static const parsers::where::value_type type_custom_age = parsers::where::type_custom_int_1;

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("service_state", &w32time_obj::get_service_state, "State of the W32Time service: running, stopped, starting, ... or 'not installed'")
      .add_string_var("start_type", &w32time_obj::get_start_type, "Start type of the W32Time service: auto, delayed, demand, disabled, ...")
      .add_string_var("sync_type", &w32time_obj::get_sync_type, "Configured synchronization type: NT5DS (domain hierarchy), NTP, AllSync or NoSync")
      .add_string_var("source", &w32time_obj::get_source, "The time source in use; the configured peers when the service could not be asked (see source_from)")
      .add_string_var("source_from", &w32time_obj::get_source_from, "Where source came from: 'service' (live), 'configuration' or 'unknown'")
      .add_string_var("peers", &w32time_obj::get_peers, "Configured NTP peers, comma separated (empty on a domain member, which discovers its source)")
      .add_string_var("last_sync", &w32time_obj::get_last_sync, "Time of the last known good synchronization, in UTC, or 'unknown'")
      .add_string_var("state", &w32time_obj::get_state, "One line verdict: not installed, not running, NoSync, falling back to the local clock or synchronizing with a source");
  registry_.add_int_var("installed", type_bool, &w32time_obj::get_installed, "True when the W32Time service exists on this host")
      .no_perf()
      .add_int_var("running", type_bool, &w32time_obj::get_running, "True when the W32Time service is running")
      .no_perf()
      .add_int_var("synchronized", type_bool, &w32time_obj::get_synchronized,
                   "True when the machine is following a time source: the service runs, synchronization is not turned off, the source is not the local clock "
                   "and - when the source could not be read - at least one time source is in use")
      .no_perf()
      .add_int_var("local_clock", type_bool, &w32time_obj::get_local_clock, "True when the source is the machine's own clock (Local CMOS Clock / free-running)")
      .no_perf()
      .add_int_var("peer_count", type_int, &w32time_obj::get_peer_count, "Number of configured NTP peers")
      .no_perf();
  // The counters only carry data while the service is running, so these are
  // optional: they render as 'unknown', compare false against every number and
  // emit no perfdata until the service has actually measured something.
  registry_
      .add_optional_int_var("offset", [](auto obj) { return obj->get_offset(); }, "unknown",
                            "Absolute clock offset against the time source in milliseconds, as last computed by the service; 'unknown' until it has measured "
                            "one (`offset = 'unknown'` tests for it)")
      .add_int_perf("ms", "", "_offset")
      .add_optional_int_var("delay", [](auto obj) { return obj->get_delay(); }, "unknown", "NTP round trip delay to the time source in milliseconds")
      .add_int_perf("ms", "", "_delay")
      .add_optional_int_var("frequency_adjustment", [](auto obj) { return obj->get_frequency_adjustment(); }, "unknown",
                            "Correction the service applies to the clock frequency, in parts per billion (negative slows the clock down)")
      .add_int_perf("", "", "_frequency_adjustment")
      .add_optional_int_var("time_sources", [](auto obj) { return obj->get_time_sources(); }, "unknown",
                            "Number of NTP time sources the client is currently using")
      .no_perf()
      .add_optional_int_var("last_sync_age", type_custom_age, [](auto obj) { return obj->get_last_sync_age(); }, "unknown",
                            "Seconds since the last synchronization W32Time recorded as good; threshold with durations, e.g. last_sync_age > 24h")
      .add_int_perf("s", "", "_last_sync");
  registry_.add_converter(type_custom_age, &duration_keyword::parse_duration<std::shared_ptr<w32time_obj> >);
  // clang-format on
}

void check_w32time_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const w32time_data &data, const long long now_epoch) {
  modern_filter::data_container container;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, container);
  filter_type filter;

  // Default: CRITICAL when the machine is not following a time source at all -
  // the failure that breaks Kerberos - or when the clock has drifted far enough
  // to be heading that way; WARNING once the offset leaves the range a healthy
  // domain hierarchy keeps. An unknown offset is -1 and never trips either.
  filter_helper.add_options("offset > 1000", "synchronized = 0 or offset > 30000", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${state}", "w32time", "", "");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<w32time_obj> record(new w32time_obj(build_w32time_obj(data, now_epoch)));
  filter.match(record);

  filter_helper.post_process(filter);
}

namespace {

// One-shot read of a single "Windows Time Service" counter. Each counter gets
// its own query so that one missing counter (they differ between Windows
// releases) does not cost us the others; an empty result means "not readable".
boost::optional<long long> read_counter(const std::string &path) {
  try {
    PDH::pdh_object object;
    object.set_counter(path);
    object.set_alias(path);
    object.set_strategy_static();
    object.set_type(PDH::pdh_object::type_large);
    // The counter names are hard-coded in English; ask PDH to translate them
    // rather than relying on the machine language.
    object.set_resolution("english");

    PDH::pdh_instance instance = PDH::factory::create(object);
    PDH::PDHQuery query;
    query.addCounter(instance);
    query.open();
    query.gatherData(false);
    query.close();
    return instance->get_int_value();
  } catch (const std::exception &) {
    return boost::none;
  } catch (...) {
    return boost::none;
  }
}

// The Windows Time service's own view of the source it follows, through the
// documented RPC entry point in w32time.dll. Returns an empty string when the
// service is not running (RPC_S_SERVER_UNAVAILABLE) or the export is missing.
std::string query_live_source() {
  typedef DWORD(WINAPI * W32TimeQuerySource_fn)(LPCWSTR, LPWSTR *);
  typedef void(WINAPI * W32TimeBufferFree_fn)(PVOID);

  const HMODULE library = LoadLibraryW(L"w32time.dll");
  if (library == nullptr) return {};

  std::string result;
  const auto query_source = reinterpret_cast<W32TimeQuerySource_fn>(GetProcAddress(library, "W32TimeQuerySource"));
  const auto buffer_free = reinterpret_cast<W32TimeBufferFree_fn>(GetProcAddress(library, "W32TimeBufferFree"));
  if (query_source != nullptr) {
    LPWSTR source = nullptr;
    if (query_source(nullptr, &source) == ERROR_SUCCESS && source != nullptr) {
      result = utf8::cvt<std::string>(source);
      if (buffer_free != nullptr) buffer_free(source);
    }
  }
  FreeLibrary(library);
  return result;
}

}  // namespace

w32time_data gather() {
  w32time_data data;

  try {
    const win_list_services::service_info info = win_list_services::get_service_info("", w32time_service);
    data.installed = true;
    data.service_state = info.get_state_s();
    data.start_type = info.get_start_type_s();
  } catch (const std::exception &) {
    // No such service: a Windows install without the time service (or one we
    // may not open) is reported as "not installed" rather than as an error.
    data.installed = false;
  }

  const win_registry::value_info type = win_registry::read_value(HKEY_LOCAL_MACHINE, parameters_key, "Type");
  if (type.exists) data.sync_type = type.string_value;
  const win_registry::value_info servers = win_registry::read_value(HKEY_LOCAL_MACHINE, parameters_key, "NtpServer");
  if (servers.exists) data.ntp_server = servers.string_value;
  const win_registry::value_info last_good = win_registry::read_value(HKEY_LOCAL_MACHINE, config_key, "LastKnownGoodTime");
  if (last_good.exists && last_good.int_value > 0) data.last_good_filetime = static_cast<unsigned long long>(last_good.int_value);

  data.live_source = query_live_source();

  data.offset_us = read_counter("\\Windows Time Service\\Computed Time Offset");
  data.delay_us = read_counter("\\Windows Time Service\\NTP Roundtrip Delay");
  data.frequency_adjustment_ppb = read_counter("\\Windows Time Service\\Clock Frequency Adjustment (PPB)");
  data.time_sources = read_counter("\\Windows Time Service\\NTP Client Time Source Count");

  return data;
}

void check_w32time(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_w32time_from(request, response, gather(), static_cast<long long>(std::time(nullptr)));
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read the Windows Time state: " + std::string(e.what()));
  }
}

}  // namespace w32time_check
