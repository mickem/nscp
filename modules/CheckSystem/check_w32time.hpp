// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>

namespace w32time_check {

// Everything the platform tells us about the Windows Time service, gathered in
// one place so the check itself can be exercised without a live W32Time.
struct w32time_data {
  w32time_data() : installed(false), last_good_filetime(0) {}

  bool installed;             // W32Time is registered with the service control manager
  std::string service_state;  // running, stopped, starting, ... (empty when not installed)
  std::string start_type;     // auto, delayed, demand, disabled, ...

  // HKLM\SYSTEM\CurrentControlSet\Services\W32Time\...
  std::string sync_type;            // Parameters\Type: NT5DS, NTP, AllSync, NoSync
  std::string ntp_server;           // Parameters\NtpServer, raw ("host,0x9 host2,0x8")
  unsigned long long last_good_filetime;  // Config\LastKnownGoodTime, 0 when unknown

  // W32TimeQuerySource(); empty when the service could not be asked - it is not
  // running, or the caller is not privileged enough (an unprivileged process
  // gets ERROR_ACCESS_DENIED, exactly as `w32tm /query /source` does; the agent
  // running as a service does not).
  std::string live_source;

  // "Windows Time Service" PDH counters; empty when the counter could not be
  // read (they only carry data while the service is running). The frequency
  // adjustment is signed, hence real optionals rather than a -1 sentinel.
  boost::optional<long long> offset_us;                 // Computed Time Offset (microseconds, absolute)
  boost::optional<long long> delay_us;                  // NTP Roundtrip Delay (microseconds)
  boost::optional<long long> frequency_adjustment_ppb;  // Clock Frequency Adjustment (PPB)
  boost::optional<long long> time_sources;              // NTP Client Time Source Count
};

// The single row the check thresholds and renders.
struct w32time_obj {
  w32time_obj() : installed(0), running(0), synchronized(0), local_clock(0), peer_count(0) {}

  std::string get_service_state() const { return service_state; }
  std::string get_start_type() const { return start_type; }
  std::string get_sync_type() const { return sync_type; }
  std::string get_source() const { return source; }
  std::string get_source_from() const { return source_from; }
  std::string get_peers() const { return peers; }
  std::string get_last_sync() const { return last_sync; }
  std::string get_state() const { return state; }
  long long get_installed() const { return installed; }
  long long get_running() const { return running; }
  long long get_synchronized() const { return synchronized; }
  long long get_local_clock() const { return local_clock; }
  long long get_peer_count() const { return peer_count; }
  boost::optional<long long> get_offset() const { return offset; }
  boost::optional<long long> get_delay() const { return delay; }
  boost::optional<long long> get_frequency_adjustment() const { return frequency_adjustment; }
  boost::optional<long long> get_time_sources() const { return time_sources; }
  boost::optional<long long> get_last_sync_age() const { return last_sync_age; }
  std::string show() const { return state; }

  std::string service_state;  // running, stopped, ... or "not installed"
  std::string start_type;     // auto, delayed, demand, disabled, ...
  std::string sync_type;      // NT5DS (domain hierarchy), NTP, AllSync, NoSync, unknown
  std::string source;         // the time source in use (or the configured one, see source_from)
  std::string source_from;    // service, configuration or unknown
  std::string peers;          // configured NtpServer peers, comma separated, flags stripped
  std::string last_sync;      // last known good sync as a readable timestamp, "unknown" if none
  std::string state;          // one line verdict used by the default detail syntax

  long long installed;   // W32Time exists on this host
  long long running;     // the service is running
  long long synchronized;// running, not NoSync and not free-running on the local clock
  long long local_clock; // the source is the local CMOS / free-running clock
  long long peer_count;  // number of configured peers

  // Empty when the value is not known; the keyword then renders as "unknown",
  // compares false against every number and emits no perfdata.
  boost::optional<long long> offset;                // absolute clock offset, milliseconds
  boost::optional<long long> delay;                 // NTP round trip delay, milliseconds
  boost::optional<long long> frequency_adjustment;  // clock frequency adjustment, PPB (may be negative)
  boost::optional<long long> time_sources;          // NTP client time sources in use
  boost::optional<long long> last_sync_age;         // seconds since the last known good sync
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<w32time_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<w32time_obj, filter_obj_handler> filter_type;

// Split a W32Time NtpServer value ("time.windows.com,0x9 ntp2.corp,0x8") into
// its host names, dropping the trailing flag field. Exposed for unit testing.
std::vector<std::string> parse_ntp_servers(const std::string &raw);

// True when the named source is the machine's own clock rather than a real time
// source ("Local CMOS Clock", "Free-running System Clock").
bool is_local_clock(const std::string &source);

// Microseconds to whole milliseconds; an unknown value stays unknown.
boost::optional<long long> us_to_ms(const boost::optional<long long> &microseconds);

// Build the row from the gathered data. Pure and side-effect free so it can be
// unit tested without a Windows Time service.
w32time_obj build_w32time_obj(const w32time_data &data, long long now_epoch);

// Gather the live state: service control manager, registry, PDH counters and
// the Windows Time service's own view of its source.
w32time_data gather();

// Testable core: renders and thresholds the supplied data.
void check_w32time_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                        const w32time_data &data, long long now_epoch);

// Live check.
void check_w32time(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace w32time_check
