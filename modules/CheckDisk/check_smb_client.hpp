// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

// check_smb_client: the client side of SMB, one record per remote share this
// host has open (\\server\share), from the "SMB Client Shares" performance
// counter set (Windows 8 / Windows Server 2012 and later). It is what an
// application server whose data lives on a file server or a Scale-Out File
// Server sees: the latency of its reads and writes against that share, the
// requests queued for it, and the credit stalls that mean the server is
// throttling the client. None of that shows up on a local disk.
//
// Everything in here is platform-neutral and works on canned counter values;
// the PDH reader is smb_client_source_win.cpp, and smb_client_source_unix.cpp
// reports the counter set as missing everywhere else.
namespace smb_client_check {

// The counter set and the counters the check reads, by their English names.
extern const char counter_set[];
const std::vector<std::string> &counter_names();

// instance name -> (counter name -> formatted value): the shape
// PDH::gather_object_instances() returns, spelled out here so that this half
// does not depend on PDH.
typedef std::map<std::string, std::map<std::string, double> > instance_values;

// What the source read. `error` is set, and `instances` empty, when the
// counter set could not be read at all; an empty `instances` without an error
// is a host that has no SMB share open at the moment.
struct counter_snapshot {
  std::string error;
  instance_values instances;
};

// One remote share the SMB client has open.
struct share_activity {
  std::string name;    // the counter instance, \\server\share
  std::string server;  // as the client addressed it: a host name, an FQDN or an address
  std::string share;   // the share name; empty when the instance names none

  double read_latency = 0;      // Avg. sec/Read, in milliseconds
  double write_latency = 0;     // Avg. sec/Write, in milliseconds
  double total_latency = 0;     // Avg. sec/Data Request, in milliseconds
  double credit_stalls = 0;     // Credit Stalls/sec
  long long queue_length = 0;   // Current Data Queue Length
  double avg_queue_length = 0;  // Avg. Data Queue Length
  long long read_bytes_per_sec = 0;
  long long write_bytes_per_sec = 0;
  double reads_per_sec = 0;
  double writes_per_sec = 0;
  double metadata_per_sec = 0;  // Metadata Requests/sec (opens, closes, queries)

  // The slower of the two directions.
  double get_latency() const { return read_latency > write_latency ? read_latency : write_latency; }
  // IPC$ carries named pipes (RPC, DFS referrals) rather than file data: a
  // pipe read waits for the other end to answer, so its "latency" is how long
  // the server took to have something to say.
  bool is_ipc() const;

  std::string show() const { return name; }
};

// Splits a counter instance into server and share. The instance is
// \\server\share; leading backslashes are optional, and an instance with no
// share part yields an empty share. False when there is no server.
bool split_instance(const std::string &instance, std::string &server, std::string &share);

// The records, one per instance, in instance-name order. Latencies are
// converted from seconds to milliseconds; a counter missing from an instance
// reads as 0.
std::vector<share_activity> build_shares(const instance_values &instances);

// The platform source (smb_client_source_win.cpp / smb_client_source_unix.cpp).
counter_snapshot read_counters();

namespace check {
// Runs the check over what `source` reads. The source is only asked once the
// options have parsed: sampling takes a second, which a request for help or
// one with a typo should not cost. A snapshot that carries an error is
// reported UNKNOWN with that error.
void check_smb_client_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           const std::function<counter_snapshot()> &source);

// Testable variant over a snapshot that was already read.
void check_smb_client_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           const counter_snapshot &snapshot);

// Live check: check_smb_client_with() over read_counters(). Defined next to
// the source, so that a test target can link this file without it.
void check_smb_client(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace smb_client_check
