// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#ifndef NSCP_CHECK_NETWORK_H
#define NSCP_CHECK_NETWORK_H

#include <list>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

class pdh_thread;

namespace network_check {

// One network interface, mirroring the field set exposed by the Windows
// check_network so queries are portable. Byte/packet rates are per-second values
// computed by the collector from the cumulative /proc/net/dev counters; metadata
// (status/MAC/speed) comes from /sys/class/net/<iface>/.
struct network_interface {
  std::string name;
  std::string status;  // operstate: "up", "down", "unknown"
  std::string mac;     // hardware address
  // Negotiated link speed in bits/sec, parsed from /sys/class/net/<iface>/speed
  // (MBit/s). 0 when unknown (virtual interfaces, link down). Filter on
  // speed_bps > 0 before relying on the usage_* percentages.
  long long speed_bps;

  long long rx_bytes_per_sec;
  long long tx_bytes_per_sec;
  long long rx_packets_per_sec;
  long long tx_packets_per_sec;
  // Cumulative error counters (since boot), as reported by /proc/net/dev.
  long long rx_errors;
  long long tx_errors;

  network_interface()
      : speed_bps(0),
        rx_bytes_per_sec(0),
        tx_bytes_per_sec(0),
        rx_packets_per_sec(0),
        tx_packets_per_sec(0),
        rx_errors(0),
        tx_errors(0) {}
  network_interface(const network_interface &other) = default;
  network_interface &operator=(const network_interface &other) = default;

  std::string get_name() const { return name; }
  std::string get_status() const { return status; }
  std::string get_enabled() const { return status == "up" ? "true" : "false"; }
  std::string get_mac() const { return mac; }
  long long get_speed_bps() const { return speed_bps; }

  long long get_received() const { return rx_bytes_per_sec; }
  long long get_sent() const { return tx_bytes_per_sec; }
  long long get_total() const { return rx_bytes_per_sec + tx_bytes_per_sec; }
  long long get_received_packets() const { return rx_packets_per_sec; }
  long long get_sent_packets() const { return tx_packets_per_sec; }
  long long get_rx_errors() const { return rx_errors; }
  long long get_tx_errors() const { return tx_errors; }

  // Best-effort percent-of-link-speed. Reads as 0 when the link speed is
  // unknown so dashboards / `<`-style alerts behave naturally; filter on
  // speed_bps > 0 to distinguish "idle" from "speed unknown".
  long long get_usage_in() const { return speed_bps <= 0 ? 0 : (rx_bytes_per_sec * 8 * 100) / speed_bps; }
  long long get_usage_out() const { return speed_bps <= 0 ? 0 : (tx_bytes_per_sec * 8 * 100) / speed_bps; }
  long long get_usage_total() const { return speed_bps <= 0 ? 0 : (get_total() * 8 * 100) / speed_bps; }

  std::string get_received_human() const;
  std::string get_sent_human() const;
  std::string get_total_human() const;

  std::string show() const { return name; }
};

typedef std::list<network_interface> nics_type;

void check_network(std::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                   PB::Commands::QueryResponseMessage::Response *response);

void build_network_metrics(PB::Metrics::MetricsBundle *parent, const nics_type &data);

}  // namespace network_check

#endif  // NSCP_CHECK_NETWORK_H
