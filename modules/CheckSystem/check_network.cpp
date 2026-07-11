// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_network.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/locks.hpp>
#include <map>
#include <utility>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>

namespace po = boost::program_options;

namespace network_check {

std::string helper::nif_query =
    "select NetConnectionID, MACAddress, Name, NetConnectionStatus, NetEnabled, Speed from Win32_NetworkAdapter where PhysicalAdapter=True and MACAddress <> "
    "null";
// Win32_PerfRawData counters are cumulative totals despite the "Persec" suffix.
// We use PerfRawData (not PerfFormattedData) so we can control the sampling interval
// and compute the per-second rate ourselves in read_prd().
std::string helper::prd_query =
    "select Name, BytesReceivedPersec, BytesSentPersec, BytesTotalPersec, PacketsReceivedPersec, PacketsSentPersec, PacketsReceivedErrors, "
    "PacketsOutboundErrors, PacketsReceivedDiscarded, PacketsOutboundDiscarded from Win32_PerfRawData_Tcpip_NetworkInterface";
std::string helper::prd_adapter_query =
    "select Name, BytesReceivedPersec, BytesSentPersec, BytesTotalPersec, PacketsReceivedPersec, PacketsSentPersec, PacketsReceivedErrors, "
    "PacketsOutboundErrors, PacketsReceivedDiscarded, PacketsOutboundDiscarded from Win32_PerfRawData_Tcpip_NetworkAdapter";

std::string helper::parse_nif_name(std::string name) { return name; }
std::string helper::parse_prd_name(std::string name) {
  boost::replace_all(name, "[", "(");
  boost::replace_all(name, "]", ")");
  boost::replace_all(name, "#", "_");
  boost::replace_all(name, "/", "_");
  boost::replace_all(name, "\\", "_");
  return name;
}

void network_interface::read_wna(wmi_impl::row r) {
  name = helper::parse_nif_name(r.get_string("Name"));
  NetConnectionID = r.get_string("NetConnectionID");
  MACAddress = r.get_string("MACAddress");
  NetConnectionStatus = r.get_string("NetConnectionStatus");
  NetEnabled = r.get_int("NetEnabled") == 0 ? "true" : "false";
  Speed = r.get_string("Speed");
  // Best-effort parse of the WMI Speed property into bits/sec. WMI returns
  // it as a stringified uint64 in most cases but virtual adapters often
  // report "Unknown" or empty. stox<>() throws on non-numeric input - we
  // catch and store 0, which is the "unknown" sentinel callers downstream
  // already handle (getUsage*Pct returns -1 when SpeedBps == 0).
  try {
    SpeedBps = Speed.empty() ? 0 : str::stox<long long>(Speed);
    if (SpeedBps < 0) SpeedBps = 0;
  } catch (...) {
    SpeedBps = 0;
  }
  has_nif = true;
}

namespace {
// Best-effort percent-of-link-speed. bytes/sec * 8 -> bits/sec, then * 100
// / speed. Returns 0 when the link speed is unknown - graphs and
// `<`-style alert rules behave naturally that way (a sentinel like -1
// would draw negative lines on dashboards and would also trip alert rules
// of the form `usage_total < threshold` on every unknown-speed interface).
// The downside: "speed unknown" looks the same as "genuinely idle". To
// distinguish them, filter on `speed_bps > 0` before applying any
// percent-based threshold. The doc for this check spells out which
// interface kinds report unknown speed (virtuals, some teams, wireless).
//
// Integer truncation is intentional: percent is reported as a whole
// number, matching how CPU/memory checks elsewhere in CheckSystem report.
long long compute_usage_pct(long long bytes_per_sec, long long speed_bps) {
  if (speed_bps <= 0) return 0;
  // Cap negative samples (which the rate calculation can briefly emit
  // around counter rollover) at 0 to avoid surprising negative
  // percentages.
  if (bytes_per_sec < 0) bytes_per_sec = 0;
  // *8 (bits) before /speed_bps to keep precision; *100 last.
  return (bytes_per_sec * 8 * 100) / speed_bps;
}
}  // namespace

long long network_interface::getUsageInPct() const { return compute_usage_pct(BytesReceivedPersec, SpeedBps); }
long long network_interface::getUsageOutPct() const { return compute_usage_pct(BytesSentPersec, SpeedBps); }
long long network_interface::getUsageTotalPct() const { return compute_usage_pct(BytesTotalPersec, SpeedBps); }

void network_interface::read_prd(wmi_impl::row r, long long delta) {
  // The "Persec" fields from Win32_PerfRawData are raw cumulative byte counts,
  // NOT pre-computed rates. To derive the actual bytes/sec we compute:
  //   (current_cumulative - previous_cumulative) / elapsed_seconds
  if (delta == 0) {
    // First sample or no time elapsed — can't compute a rate yet.
    BytesReceivedPersec = 0;
    BytesSentPersec = 0;
    BytesTotalPersec = 0;
    PacketsReceivedPersec = 0;
    PacketsSentPersec = 0;
    PacketsReceivedErrors = 0;
    PacketsOutboundErrors = 0;
    PacketsReceivedDiscarded = 0;
    PacketsOutboundDiscarded = 0;
  } else {
    long long v = r.get_int("BytesReceivedPersec");
    BytesReceivedPersec = (v - oldBytesReceivedPersec) / delta;
    oldBytesReceivedPersec = v;
    v = r.get_int("BytesSentPersec");
    BytesSentPersec = (v - oldBytesSentPersec) / delta;
    oldBytesSentPersec = v;
    v = r.get_int("BytesTotalPersec");
    BytesTotalPersec = (v - oldBytesTotalPersec) / delta;
    oldBytesTotalPersec = v;
    v = r.get_int("PacketsReceivedPersec");
    PacketsReceivedPersec = (v - oldPacketsReceivedPersec) / delta;
    oldPacketsReceivedPersec = v;
    v = r.get_int("PacketsSentPersec");
    PacketsSentPersec = (v - oldPacketsSentPersec) / delta;
    oldPacketsSentPersec = v;
    v = r.get_int("PacketsReceivedErrors");
    PacketsReceivedErrors = (v - oldPacketsReceivedErrors) / delta;
    oldPacketsReceivedErrors = v;
    v = r.get_int("PacketsOutboundErrors");
    PacketsOutboundErrors = (v - oldPacketsOutboundErrors) / delta;
    oldPacketsOutboundErrors = v;
    v = r.get_int("PacketsReceivedDiscarded");
    PacketsReceivedDiscarded = (v - oldPacketsReceivedDiscarded) / delta;
    oldPacketsReceivedDiscarded = v;
    v = r.get_int("PacketsOutboundDiscarded");
    PacketsOutboundDiscarded = (v - oldPacketsOutboundDiscarded) / delta;
    oldPacketsOutboundDiscarded = v;
  }
  has_prd = true;
}

void network_interface::build_metrics(PB::Metrics::MetricsBundle *section) const {
  using namespace nscapi::metrics;

  section->set_key("network");
  add_metric(section, name + ".NetConnectionID", NetConnectionID);
  add_metric(section, name + ".MACAddress", MACAddress);
  add_metric(section, name + ".NetConnectionStatus", NetConnectionStatus);
  add_metric(section, name + ".NetEnabled", NetEnabled);
  add_metric(section, name + ".Speed", Speed);
  if (has_prd) {
    add_metric(section, name + ".BytesReceivedPersec", BytesReceivedPersec);
    add_metric(section, name + ".BytesSentPersec", BytesSentPersec);
    add_metric(section, name + ".BytesTotalPersec", BytesTotalPersec);
    add_metric(section, name + ".PacketsReceivedPersec", PacketsReceivedPersec);
    add_metric(section, name + ".PacketsSentPersec", PacketsSentPersec);
    add_metric(section, name + ".PacketsReceivedErrors", PacketsReceivedErrors);
    add_metric(section, name + ".PacketsOutboundErrors", PacketsOutboundErrors);
    add_metric(section, name + ".PacketsReceivedDiscarded", PacketsReceivedDiscarded);
    add_metric(section, name + ".PacketsOutboundDiscarded", PacketsOutboundDiscarded);
  }
}

void network_data::query_nif(netmap_type &netmap) {
  wmi_impl::query wmiQuery1(helper::nif_query, "root\\cimv2", "", "");
  wmi_impl::row_enumerator row = wmiQuery1.execute();
  while (row.has_next()) {
    wmi_impl::row r = row.get_next();
    std::string name = helper::parse_nif_name(r.get_string("Name"));
    netmap_type::iterator it = netmap.find(name);
    if (it == netmap.end()) {
      network_interface nif;
      nif.read_wna(r);
      netmap[nif.name] = nif;
    } else {
      it->second.read_wna(r);
    }
  }
  std::string keys;
  for (const netmap_type::value_type &v : netmap) {
    str::format::append_list(keys, v.first);
  }
}

void network_data::query_prd(netmap_type &netmap, long long delta, const std::string &query, bool allow_insert) {
  wmi_impl::query wmiQuery(query, "root\\cimv2", "", "");
  wmi_impl::row_enumerator row = wmiQuery.execute();
  while (row.has_next()) {
    wmi_impl::row r = row.get_next();
    std::string name = helper::parse_prd_name(r.get_string("Name"));
    netmap_type::iterator it = netmap.find(name);
    if (it == netmap.end()) {
      if (!allow_insert) continue;
      // Synthesise an entry for a perfraw row with no Win32_NetworkAdapter match
      // (the team aggregate case). Metadata fields stay empty; counters are read.
      network_interface nif;
      nif.name = name;
      nif.read_prd(r, delta);
      netmap[name] = nif;
    } else {
      it->second.read_prd(r, delta);
    }
  }
}

void network_data::query_team(netmap_type &if_netmap, netmap_type &ad_netmap) {
  if (!fetch_team_) return;
  // member interface name -> (team name, raw operational status)
  std::map<std::string, std::pair<std::string, std::string> > members;
  try {
    wmi_impl::query q("select Name, Team, OperationalStatus from MSFT_NetLbfoTeamMember", "root\\StandardCimv2", "", "");
    wmi_impl::row_enumerator row = q.execute();
    while (row.has_next()) {
      wmi_impl::row r = row.get_next();
      members[r.get_string("Name")] = std::make_pair(r.get_string("Team"), str::xtos(r.get_int("OperationalStatus")));
    }
  } catch (const wmi_impl::wmi_exception &) {
    // The LBFO WMI provider / ROOT\StandardCimv2 namespace is absent on many
    // systems (client SKUs, older Windows) and having no teams is not an error.
    // Disable so we don't pay the failed query on every fetch; leave team fields empty.
    fetch_team_ = false;
    return;
  }
  if (members.empty()) return;
  // MSFT_NetLbfoTeamMember.Name is the member adapter's connection/friendly
  // name, so match on NetConnectionID first, then fall back to the perf name.
  auto annotate = [&members](netmap_type &m) {
    for (netmap_type::value_type &v : m) {
      std::map<std::string, std::pair<std::string, std::string> >::const_iterator it = members.find(v.second.NetConnectionID);
      if (it == members.end()) it = members.find(v.second.name);
      if (it != members.end()) {
        v.second.team = it->second.first;
        v.second.team_status = it->second.second;
      }
    }
  };
  annotate(if_netmap);
  annotate(ad_netmap);
}

void network_data::fetch() {
  if (!fetch_network_) return;

  nics_type tmp;
  // Two separate maps keyed by name: NetworkInterface and NetworkAdapter use
  // partially overlapping but non-identical names for the same physical NIC,
  // so we can't share a single map without one source overwriting the other.
  netmap_type if_netmap;
  netmap_type ad_netmap;
  boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
  boost::posix_time::ptime last;
  {
    boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!readLock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading");
    last = last_;
    // Restore previous counter values so read_prd can compute deltas.
    for (const network_interface &v : nics_) {
      if (v.get_source() == "adapter") {
        ad_netmap[v.get_name()] = v;
      } else {
        if_netmap[v.get_name()] = v;
      }
    }
  }
  try {
    boost::posix_time::time_duration diff = now - last;
    long long delta = diff.total_seconds();

    // Win32_NetworkAdapter metadata applies to both perfraw sources, so we run
    // it once and merge into whichever map(s) end up holding a matching entry.
    query_nif(if_netmap);
    for (const netmap_type::value_type &v : if_netmap) {
      netmap_type::iterator it = ad_netmap.find(v.first);
      if (it == ad_netmap.end()) {
        ad_netmap[v.first] = v.second;
      } else if (!it->second.has_nif && v.second.has_nif) {
        // Carry over metadata, preserve adapter-source counters.
        network_interface merged = v.second;
        merged.has_prd = it->second.has_prd;
        merged.oldBytesReceivedPersec = it->second.oldBytesReceivedPersec;
        merged.oldBytesSentPersec = it->second.oldBytesSentPersec;
        merged.oldBytesTotalPersec = it->second.oldBytesTotalPersec;
        merged.BytesReceivedPersec = it->second.BytesReceivedPersec;
        merged.BytesSentPersec = it->second.BytesSentPersec;
        merged.BytesTotalPersec = it->second.BytesTotalPersec;
        merged.oldPacketsReceivedPersec = it->second.oldPacketsReceivedPersec;
        merged.oldPacketsSentPersec = it->second.oldPacketsSentPersec;
        merged.oldPacketsReceivedErrors = it->second.oldPacketsReceivedErrors;
        merged.oldPacketsOutboundErrors = it->second.oldPacketsOutboundErrors;
        merged.oldPacketsReceivedDiscarded = it->second.oldPacketsReceivedDiscarded;
        merged.oldPacketsOutboundDiscarded = it->second.oldPacketsOutboundDiscarded;
        merged.PacketsReceivedPersec = it->second.PacketsReceivedPersec;
        merged.PacketsSentPersec = it->second.PacketsSentPersec;
        merged.PacketsReceivedErrors = it->second.PacketsReceivedErrors;
        merged.PacketsOutboundErrors = it->second.PacketsOutboundErrors;
        merged.PacketsReceivedDiscarded = it->second.PacketsReceivedDiscarded;
        merged.PacketsOutboundDiscarded = it->second.PacketsOutboundDiscarded;
        it->second = merged;
      }
    }

    query_prd(if_netmap, delta, helper::prd_query, false);
    // allow_insert=true so the team aggregate (no nif match) surfaces.
    query_prd(ad_netmap, delta, helper::prd_adapter_query, true);

    // Best-effort NIC-team membership annotation (no-op / self-disabling when
    // the LBFO provider is unavailable).
    query_team(if_netmap, ad_netmap);

    for (netmap_type::value_type &v : if_netmap) {
      if (!v.second.is_compleate()) continue;
      v.second.source = "interface";
      tmp.push_back(v.second);
    }
    for (netmap_type::value_type &v : ad_netmap) {
      if (!v.second.is_compleate()) continue;
      v.second.source = "adapter";
      tmp.push_back(v.second);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    if (e.get_code() == WBEM_E_INVALID_QUERY) {
      fetch_network_ = false;
      throw nsclient::nsclient_exception("Failed to fetch network metrics, disabling...");
    }
    throw nsclient::nsclient_exception("Failed to fetch network metrics: " + e.reason());
  }
  {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!writeLock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for writing");
    nics_ = tmp;
    last_ = now;
  }
}

nics_type network_data::get() {
  boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!readLock.owns_lock()) throw nsclient::nsclient_exception("Failed to get mutex for reading");
  return nics_;
}

namespace check {

typedef network_interface filter_obj;

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("name", &filter_obj::get_name, "Network interface name")
      .add_string_var("net_connection_id", &filter_obj::get_NetConnectionID, "Network connection id")
      .add_string_var("MAC", &filter_obj::get_MACAddress, "The MAC address")
      .add_string_var("status", &filter_obj::get_NetConnectionStatus, "Network connection status")
      .add_string_var("enabled", &filter_obj::get_NetEnabled, "True if the network interface is enabled")
      .add_string_var("speed", &filter_obj::get_Speed, "The network interface speed (raw WMI value, e.g. \"1000000000\" or \"Unknown\")")
      .add_string_var("source", &filter_obj::get_source, "WMI source: 'interface' or 'adapter'")
      .add_string_var("team", &filter_obj::get_team, "NIC team this adapter belongs to (empty if not a team member / LBFO unavailable)")
      .add_string_var("team_status", &filter_obj::get_team_status,
                      "Raw MSFT_NetLbfoTeamMember.OperationalStatus of this team member (empty if not a team member)");

  // Byte-rate counters. `add_int_perf` is NOT a "register as perf"
  // shorthand - it chains onto the last-registered variable and marks
  // that one as also emitting a perf entry. So the pattern is
  // `add_int_var(...).add_int_perf("UOM")` per metric. Issue #329 #1: now
  // each direction emits its own perf entry ('<iface>_received',
  // '<iface>_sent', '<iface>_total') instead of being collapsed into a
  // single combined value. Unit "Bps" (bytes/sec) matches the underlying
  // counter; consumers that prefer bits-per-sec can multiply by 8.
  registry_.add_int_var("received", &filter_obj::getBytesReceivedPersec, "Bytes received per second")
      .add_int_perf("Bps")
      .add_int_var("sent", &filter_obj::getBytesSentPersec, "Bytes sent per second")
      .add_int_perf("Bps")
      .add_int_var("total", &filter_obj::getBytesTotalPersec, "Bytes total per second")
      .add_int_perf("Bps");

  // Packet-rate, error-rate and discard-rate counters. All are per-second rates
  // derived from the cumulative Win32_PerfRawData_Tcpip counters, so a healthy
  // NIC reports ~0 errors/sec and any sustained rate (errors_in > 0, discards_out
  // > 0, ...) is an alertable signal. Each emits its own perf entry.
  // Distinct perf suffixes so co-referenced counters each get their own series
  // ('<iface>_packets_in', ...) instead of collapsing onto the shared ${name}
  // perf alias.
  registry_.add_int_var("packets_in", &filter_obj::getPacketsReceivedPersec, "Packets received per second")
      .add_int_perf("", "", "_packets_in")
      .add_int_var("packets_out", &filter_obj::getPacketsSentPersec, "Packets sent per second")
      .add_int_perf("", "", "_packets_out")
      .add_int_var("errors_in", &filter_obj::getPacketsReceivedErrors, "Inbound packet errors per second")
      .add_int_perf("", "", "_errors_in")
      .add_int_var("errors_out", &filter_obj::getPacketsOutboundErrors, "Outbound packet errors per second")
      .add_int_perf("", "", "_errors_out")
      .add_int_var("discards_in", &filter_obj::getPacketsReceivedDiscarded, "Inbound packets discarded per second")
      .add_int_perf("", "", "_discards_in")
      .add_int_var("discards_out", &filter_obj::getPacketsOutboundDiscarded, "Outbound packets discarded per second")
      .add_int_perf("", "", "_discards_out");

  // Best-effort negotiated link speed (bits/sec). WMI's Speed property is
  // unreliable on virtual / wireless / NIC-team adapters - see the
  // SpeedBps field comment in check_network.hpp. Exposed as a where-engine
  // variable so users can filter on `speed_bps > 0` to gate "we know the
  // link rate" before applying percent thresholds. Deliberately NOT
  // marked as perf - it's metadata, not a time-series sample.
  registry_.add_int_var("speed_bps", &filter_obj::getSpeedBps,
                        "Negotiated link speed in bits/sec, parsed from the WMI Speed property. "
                        "BEST-EFFORT: 0 when the speed is Unknown/empty (virtual adapters, some teams). "
                        "Filter on speed_bps > 0 before relying on usage_in/out/total.");

  // Percent-usage variables, derived from speed_bps. Each returns -1 when
  // the link speed is unknown so the operator can distinguish "0% load"
  // from "we don't know the denominator". Same `add_int_var().add_int_perf()`
  // pattern as above so they show up in perfdata with UOM "%" by default;
  // descriptions repeat the best-effort caveat so the auto-generated
  // reference doc carries the warning to operators.
  registry_
      .add_int_var("usage_in", &filter_obj::getUsageInPct,
                   "Percent of negotiated link speed used by received traffic. "
                   "BEST-EFFORT: reads as 0 when speed is unknown - "
                   "filter on speed_bps > 0 to distinguish idle from unknown.")
      .add_int_perf("%")
      .add_int_var("usage_out", &filter_obj::getUsageOutPct,
                   "Percent of negotiated link speed used by sent traffic. "
                   "BEST-EFFORT: reads as 0 when speed is unknown - "
                   "filter on speed_bps > 0 to distinguish idle from unknown.")
      .add_int_perf("%")
      .add_int_var("usage_total", &filter_obj::getUsageTotalPct,
                   "Percent of negotiated link speed used by total traffic. "
                   "BEST-EFFORT: reads as 0 when speed is unknown - "
                   "filter on speed_bps > 0 to distinguish idle from unknown.")
      .add_int_perf("%");

  // Human-readable byte-rate strings for use in detail-syntax templates
  // (issue #329 #3). Issued as `*_human` rather than touching `sent` /
  // `received` so perfdata stays numeric. format_byte_units auto-scales
  // to B/KB/MB/GB/... base-1024.
  registry_
      .add_string_var(
          "sent_human", [](auto obj) { return str::format::format_byte_units(obj->getBytesSentPersec()); },
          "Bytes sent per second, formatted as a human-readable string (auto-scaled).")
      .add_string_var(
          "received_human", [](auto obj) { return str::format::format_byte_units(obj->getBytesReceivedPersec()); },
          "Bytes received per second, formatted as a human-readable string (auto-scaled).")
      .add_string_var(
          "total_human", [](auto obj) { return str::format::format_byte_units(obj->getBytesTotalPersec()); },
          "Bytes total per second, formatted as a human-readable string (auto-scaled).");
}

void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, nics_type nicdata) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  std::string mode = "interface";

  filter_type filter;
  filter_helper.add_options("total > 10000", "total > 100000", "", filter.get_filter_syntax(), "critical");
  // Default top-syntax now uses the human-readable byte-rate strings
  // (issue #329 #3) so operators see ">1.2 MB/s" instead of ">1234567 bps".
  // The raw byte-rate values are still available via `sent` / `received`
  // / `total` for filter expressions, and they're emitted as perfdata
  // automatically (issue #329 #1). For percent-of-link thresholds use
  // `usage_in` / `usage_out` / `usage_total` after gating on
  // `speed_bps > 0` (issue #329 #2).
  filter_helper.add_syntax("${status}: ${list}", "${name} >${sent_human}/s <${received_human}/s", "${name}", "", "%(status): Network interfaces seem ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("mode", po::value<std::string>(&mode)->default_value("interface"),
     "Which WMI source to report from: 'interface' (default; Win32_PerfRawData_Tcpip_NetworkInterface, physical adapters only), "
     "'adapter' (Win32_PerfRawData_Tcpip_NetworkAdapter, includes NIC team aggregates), or 'both' (every interface reported under both sources)")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (mode != "interface" && mode != "adapter" && mode != "both") {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid mode '" + mode + "': expected interface, adapter, or both");
  }

  if (!filter_helper.build_filter(filter)) return;
  for (const nics_type::value_type v : nicdata) {
    if (mode != "both" && v.get_source() != mode) continue;
    const std::shared_ptr<filter_obj> record(new filter_obj(v));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}
}  // namespace check

}  // namespace network_check