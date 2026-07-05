// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/shared_mutex.hpp>
#include <boost/unordered_map.hpp>
#include <list>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <win/wmi/wmi_query.hpp>

namespace network_check {
struct helper {
  static std::string parse_nif_name(std::string name);
  static std::string parse_prd_name(std::string name);

  static std::string nif_query;
  static std::string prd_query;
  // Win32_PerfRawData_Tcpip_NetworkAdapter is a superset of NetworkInterface that
  // also exposes counters for NIC team aggregates (issue #625). It uses the
  // friendlier Windows adapter name rather than the MIB-style name used by
  // NetworkInterface, so the two name spaces partially overlap but are not
  // identical — we fetch both and let the caller pick via mode=.
  static std::string prd_adapter_query;
};

struct network_interface {
  std::string name;
  std::string NetConnectionID;
  std::string MACAddress;
  std::string NetConnectionStatus;
  std::string NetEnabled;
  // Display copy of the Win32_NetworkAdapter Speed property (e.g.
  // "1000000000" or "Unknown"). Use SpeedBps below for arithmetic.
  std::string Speed;
  // Negotiated link speed in bits per second, parsed from `Speed`.
  // **Best-effort**: WMI's Speed property is the negotiated link speed,
  // which is not always what you'd call "usable throughput". Documented
  // unreliable cases:
  //   - Virtual adapters (VPN tunnels, loopback, some Hyper-V vNICs)
  //     report "Unknown" / empty -> we store 0.
  //   - NIC team aggregates may report 0 or ~0ULL on some Windows builds.
  //   - Wireless drivers may report a fixed nominal rate (e.g. 54000000
  //     for legacy 802.11g) regardless of the live MCS rate.
  //   - For NIC teams in mode=adapter, the value may be the sum of the
  //     member links OR the per-link speed depending on the driver.
  // Anything derived from SpeedBps (usage_in / usage_out / usage_total)
  // inherits the same best-effort caveats - see the getUsage*Pct
  // methods. Filter on `speed_bps > 0` to gate on "we know the link speed
  // well enough to compute a percentage".
  long long SpeedBps;
  // Which WMI class this entry was collected from: "interface" (the legacy
  // Win32_PerfRawData_Tcpip_NetworkInterface) or "adapter"
  // (Win32_PerfRawData_Tcpip_NetworkAdapter, which also reports team aggregates).
  std::string source;
  bool has_nif;
  bool has_prd;
  long long oldBytesReceivedPersec;
  long long oldBytesSentPersec;
  long long oldBytesTotalPersec;
  long long BytesReceivedPersec;
  long long BytesSentPersec;
  long long BytesTotalPersec;

  network_interface()
      : SpeedBps(0),
        source("interface"),
        has_nif(false),
        has_prd(false),
        oldBytesReceivedPersec(0),
        oldBytesSentPersec(0),
        oldBytesTotalPersec(0),
        BytesReceivedPersec(0),
        BytesSentPersec(0),
        BytesTotalPersec(0) {}
  network_interface(const network_interface &other)
      : name(other.name),
        NetConnectionID(other.NetConnectionID),
        MACAddress(other.MACAddress),
        NetConnectionStatus(other.NetConnectionStatus),
        NetEnabled(other.NetEnabled),
        Speed(other.Speed),
        SpeedBps(other.SpeedBps),
        source(other.source),
        has_nif(other.has_nif),
        has_prd(other.has_prd),
        oldBytesReceivedPersec(other.oldBytesReceivedPersec),
        oldBytesSentPersec(other.oldBytesSentPersec),
        oldBytesTotalPersec(other.oldBytesTotalPersec),
        BytesReceivedPersec(other.BytesReceivedPersec),
        BytesSentPersec(other.BytesSentPersec),
        BytesTotalPersec(other.BytesTotalPersec) {}

  const network_interface &operator()(const network_interface &other) {
    name = other.name;
    NetConnectionID = other.NetConnectionID;
    MACAddress = other.MACAddress;
    NetConnectionStatus = other.NetConnectionStatus;
    NetEnabled = other.NetEnabled;
    Speed = other.Speed;
    SpeedBps = other.SpeedBps;
    source = other.source;
    has_nif = other.has_nif;
    has_prd = other.has_prd;
    oldBytesReceivedPersec = other.oldBytesReceivedPersec;
    oldBytesSentPersec = other.oldBytesSentPersec;
    oldBytesTotalPersec = other.oldBytesTotalPersec;
    BytesReceivedPersec = other.BytesReceivedPersec;
    BytesSentPersec = other.BytesSentPersec;
    BytesTotalPersec = other.BytesTotalPersec;
    return *this;
  }

  std::string show() const { return name + " (" + NetConnectionID + ")"; }

  void read_wna(wmi_impl::row r);
  void read_prd(wmi_impl::row r, long long delta);

  // Team aggregates show up in NetworkAdapter perfraw but have no matching
  // Win32_NetworkAdapter row, so has_nif is false for them. Accept either
  // metadata or counters as "complete enough to report".
  bool is_compleate() const { return has_nif || has_prd; }
  void build_metrics(PB::Metrics::MetricsBundle *section) const;

  std::string get_name() const { return name; }
  std::string get_NetConnectionID() const { return NetConnectionID; }
  std::string get_MACAddress() const { return MACAddress; }
  std::string get_NetConnectionStatus() const { return NetConnectionStatus; }
  std::string get_NetEnabled() const { return NetEnabled; }
  std::string get_Speed() const { return Speed; }
  std::string get_source() const { return source; }

  long long getBytesReceivedPersec() const { return BytesReceivedPersec; }
  long long getBytesSentPersec() const { return BytesSentPersec; }
  long long getBytesTotalPersec() const { return BytesTotalPersec; }

  // Best-effort: parsed negotiated link speed in bits/sec. Returns 0 when
  // WMI reported the Speed as Unknown/empty/zero. See the SpeedBps field
  // comment above for the full caveat list.
  long long getSpeedBps() const { return SpeedBps; }

  // Best-effort percent usage of the link, derived from SpeedBps. Reads
  // as 0 when the speed is unknown (so dashboards and `<`-style alert
  // rules behave naturally - no negative sentinel to special-case). The
  // unfortunate consequence is that "speed unknown" looks identical to
  // "0% busy"; callers that need to distinguish them must inspect
  // `speed_bps` directly. Inherits all the reliability caveats of
  // SpeedBps - for hard thresholds, filter on `speed_bps > 0` first or
  // stick to absolute byte-rate thresholds.
  long long getUsageInPct() const;
  long long getUsageOutPct() const;
  long long getUsageTotalPct() const;
};

typedef std::list<network_interface> nics_type;
typedef boost::unordered_map<std::string, network_interface> netmap_type;

class network_data {
  boost::shared_mutex mutex_;
  bool fetch_network_;
  nics_type nics_;
  boost::posix_time::ptime last_;

 public:
  network_data() : fetch_network_(true), last_(boost::posix_time::second_clock::local_time()) {}

  void fetch();
  nics_type get();

 private:
  void query_nif(netmap_type &netmap);
  void query_prd(netmap_type &netmap, long long delta, const std::string &query, bool allow_insert);
};

namespace check {
void check_network(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, nics_type data);

}

}  // namespace network_check
