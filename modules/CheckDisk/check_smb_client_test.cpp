// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_smb_client.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

// nscapi::plugin_singleton is defined once per test binary: in
// check_disk_io_test.cpp (check_disk_test) and check_disk_unix_test.cpp
// (check_disk_unix_test).

using smb_client_check::build_shares;
using smb_client_check::counter_snapshot;
using smb_client_check::instance_values;
using smb_client_check::share_activity;
using smb_client_check::split_instance;

namespace {

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::vector<std::string> perf_labels(const PB::Commands::QueryResponseMessage::Response &r) {
  std::vector<std::string> out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (const auto &p : r.lines(i).perf()) out.push_back(p.alias());
  }
  return out;
}

bool has_label(const PB::Commands::QueryResponseMessage::Response &r, const std::string &label) {
  for (const std::string &l : perf_labels(r)) {
    if (l == label) return true;
  }
  return false;
}

PB::Commands::QueryRequestMessage::Request request_for(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_smb_client");
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

// Counter values the way PDH formats them: latencies in seconds.
std::map<std::string, double> share_counters(const double sec_read, const double sec_write, const double credit_stalls = 0, const double queue = 0) {
  std::map<std::string, double> v;
  v["Avg. sec/Read"] = sec_read;
  v["Avg. sec/Write"] = sec_write;
  v["Avg. sec/Data Request"] = (sec_read + sec_write) / 2;
  v["Credit Stalls/sec"] = credit_stalls;
  v["Current Data Queue Length"] = queue;
  v["Avg. Data Queue Length"] = 0.25;
  v["Read Bytes/sec"] = 1048576;
  v["Write Bytes/sec"] = 65536;
  v["Read Requests/sec"] = 16;
  v["Write Requests/sec"] = 1.5;
  v["Metadata Requests/sec"] = 3;
  return v;
}

counter_snapshot snapshot_of(const instance_values &instances) {
  counter_snapshot s;
  s.instances = instances;
  return s;
}

PB::Commands::QueryResponseMessage::Response run(const counter_snapshot &snapshot, const std::vector<std::string> &args = {}) {
  PB::Commands::QueryResponseMessage::Response response;
  smb_client_check::check::check_smb_client_from(request_for(args), &response, snapshot);
  return response;
}

}  // namespace

// ---------------------------------------------------------------------------
// Instance names
// ---------------------------------------------------------------------------

TEST(SmbClientInstance, SplitsServerAndShare) {
  std::string server, share;
  ASSERT_TRUE(split_instance("\\\\fs01\\data", server, share));
  EXPECT_EQ(server, "fs01");
  EXPECT_EQ(share, "data");

  ASSERT_TRUE(split_instance("\\\\sofs.contoso.com\\VMs", server, share));
  EXPECT_EQ(server, "sofs.contoso.com");
  EXPECT_EQ(share, "VMs");
}

TEST(SmbClientInstance, LeadingBackslashesAreOptional) {
  std::string server, share;
  ASSERT_TRUE(split_instance("fs01\\data", server, share));
  EXPECT_EQ(server, "fs01");
  EXPECT_EQ(share, "data");
}

TEST(SmbClientInstance, ShareNamesKeepTheirSpacesAndDollars) {
  std::string server, share;
  ASSERT_TRUE(split_instance("\\\\10.0.0.5\\Old Data (2019)", server, share));
  EXPECT_EQ(server, "10.0.0.5");
  EXPECT_EQ(share, "Old Data (2019)");

  ASSERT_TRUE(split_instance("\\\\fs01\\C$", server, share));
  EXPECT_EQ(share, "C$");
}

TEST(SmbClientInstance, ServerOnly) {
  std::string server, share;
  ASSERT_TRUE(split_instance("\\\\fs01", server, share));
  EXPECT_EQ(server, "fs01");
  EXPECT_EQ(share, "");
}

TEST(SmbClientInstance, NoServer) {
  std::string server, share;
  EXPECT_FALSE(split_instance("", server, share));
  EXPECT_FALSE(split_instance("\\\\", server, share));
}

// ---------------------------------------------------------------------------
// Records
// ---------------------------------------------------------------------------

TEST(SmbClientRecords, ConvertsSecondsToMilliseconds) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.0123456, 0.0004);
  const std::vector<share_activity> shares = build_shares(in);
  ASSERT_EQ(shares.size(), 1u);
  const share_activity &a = shares[0];
  EXPECT_EQ(a.name, "\\\\fs01\\data");
  EXPECT_EQ(a.server, "fs01");
  EXPECT_EQ(a.share, "data");
  EXPECT_DOUBLE_EQ(a.read_latency, 12.346);
  EXPECT_DOUBLE_EQ(a.write_latency, 0.4);
  EXPECT_DOUBLE_EQ(a.total_latency, 6.373);
  EXPECT_DOUBLE_EQ(a.get_latency(), 12.346);
  EXPECT_EQ(a.read_bytes_per_sec, 1048576);
  EXPECT_EQ(a.write_bytes_per_sec, 65536);
  EXPECT_DOUBLE_EQ(a.reads_per_sec, 16);
  EXPECT_DOUBLE_EQ(a.writes_per_sec, 1.5);
  EXPECT_DOUBLE_EQ(a.metadata_per_sec, 3);
  EXPECT_DOUBLE_EQ(a.avg_queue_length, 0.25);
}

TEST(SmbClientRecords, NameIsTheUncPathWhateverPdhSpellsIt) {
  // PDH hands the instances back with a single leading backslash (seen on
  // the Windows CI runners); the name keyword, the docs and the detail line
  // promise the UNC path, so it is rebuilt from server and share.
  instance_values in;
  in["\\localhost\\C$"] = share_counters(0, 0);
  in["\\\\fs01\\data"] = share_counters(0, 0);
  in["sofs\\VMs"] = share_counters(0, 0);
  const std::vector<share_activity> shares = build_shares(in);
  ASSERT_EQ(shares.size(), 3u);
  std::vector<std::string> names;
  for (const share_activity &a : shares) names.push_back(a.name);
  EXPECT_NE(std::find(names.begin(), names.end(), "\\\\localhost\\C$"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "\\\\fs01\\data"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "\\\\sofs\\VMs"), names.end());
}

TEST(SmbClientRecords, LatencyIsTheSlowerDirection) {
  instance_values in;
  in["\\\\fs01\\logs"] = share_counters(0.002, 0.080);
  const std::vector<share_activity> shares = build_shares(in);
  ASSERT_EQ(shares.size(), 1u);
  EXPECT_DOUBLE_EQ(shares[0].get_latency(), 80.0);
}

TEST(SmbClientRecords, MissingCountersReadAsZero) {
  // A counter the host's counter set lacks (an older build) must not drop the
  // share or invent a value.
  instance_values in;
  in["\\\\fs01\\data"]["Avg. sec/Read"] = 0.005;
  const std::vector<share_activity> shares = build_shares(in);
  ASSERT_EQ(shares.size(), 1u);
  EXPECT_DOUBLE_EQ(shares[0].read_latency, 5.0);
  EXPECT_DOUBLE_EQ(shares[0].write_latency, 0.0);
  EXPECT_DOUBLE_EQ(shares[0].credit_stalls, 0.0);
  EXPECT_EQ(shares[0].queue_length, 0);
}

TEST(SmbClientRecords, IpcIsRecognisedWhateverTheCase) {
  instance_values in;
  in["\\\\fs01\\IPC$"] = share_counters(0, 0);
  in["\\\\fs02\\ipc$"] = share_counters(0, 0);
  in["\\\\fs01\\data"] = share_counters(0, 0);
  const std::vector<share_activity> shares = build_shares(in);
  ASSERT_EQ(shares.size(), 3u);
  int ipc = 0;
  for (const share_activity &a : shares) ipc += a.is_ipc() ? 1 : 0;
  EXPECT_EQ(ipc, 2);
}

// ---------------------------------------------------------------------------
// The check
// ---------------------------------------------------------------------------

TEST(SmbClientCheck, HealthyShareIsOk) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.004, 0.006);
  const auto r = run(snapshot_of(in));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << join_lines(r);
  EXPECT_EQ(join_lines(r), "OK: \\\\fs01\\data: read 4ms, write 6ms, 0 credit stalls/s, queue 0");
}

TEST(SmbClientCheck, SlowReadsWarn) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.030, 0.001);
  const auto r = run(snapshot_of(in));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING) << join_lines(r);
  EXPECT_NE(join_lines(r).find("\\\\fs01\\data: read 30ms"), std::string::npos) << join_lines(r);
}

TEST(SmbClientCheck, SlowWritesAreNotHiddenByFastReads) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.001, 0.075);
  const auto r = run(snapshot_of(in));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL) << join_lines(r);
}

TEST(SmbClientCheck, CreditStallsWarn) {
  instance_values in;
  in["\\\\sofs\\VMs"] = share_counters(0.002, 0.002, 4.0);
  const auto r = run(snapshot_of(in));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING) << join_lines(r);
  EXPECT_NE(join_lines(r).find("4 credit stalls/s"), std::string::npos) << join_lines(r);
}

TEST(SmbClientCheck, IpcIsLeftOutByDefault) {
  // A named pipe read waits for the server to answer: a slow IPC$ is not a
  // storage problem.
  instance_values in;
  in["\\\\dc01\\IPC$"] = share_counters(2.5, 0.0);
  in["\\\\fs01\\data"] = share_counters(0.001, 0.001);
  const auto r = run(snapshot_of(in));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << join_lines(r);
  EXPECT_EQ(join_lines(r).find("IPC$"), std::string::npos) << join_lines(r);
  EXPECT_NE(join_lines(r).find("\\\\fs01\\data"), std::string::npos) << join_lines(r);
}

TEST(SmbClientCheck, FilterNoneTakesIpcIn) {
  instance_values in;
  in["\\\\dc01\\IPC$"] = share_counters(2.5, 0.0);
  const auto r = run(snapshot_of(in), {"filter=none"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL) << join_lines(r);
}

TEST(SmbClientCheck, NoSharesIsOk) {
  // SMB sessions close when idle: an empty set is the normal state.
  const auto r = run(snapshot_of(instance_values()));
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << join_lines(r);
  EXPECT_NE(join_lines(r).find("No SMB client shares in use"), std::string::npos) << join_lines(r);
}

TEST(SmbClientCheck, EmptyStateIsConfigurable) {
  // An application server that must always have its share open can say so.
  const auto r = run(snapshot_of(instance_values()), {"empty-state=critical"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::CRITICAL) << join_lines(r);
}

TEST(SmbClientCheck, UnreadableCounterSetIsUnknown) {
  counter_snapshot s;
  s.error = "The SMB Client Shares performance counters are not available";
  const auto r = run(s);
  EXPECT_EQ(r.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(r);
  EXPECT_NE(join_lines(r).find("not available"), std::string::npos) << join_lines(r);
}

TEST(SmbClientCheck, ServerAndShareKeywordsFilter) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.100, 0.0);
  in["\\\\fs02\\data"] = share_counters(0.001, 0.0);
  const auto r = run(snapshot_of(in), {"filter=server = 'fs02'"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::OK) << join_lines(r);
}

TEST(SmbClientCheck, PerfDataPerShare) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.004, 0.006);
  const auto r = run(snapshot_of(in));
  EXPECT_TRUE(has_label(r, "fs01/data_latency")) << join_lines(r);
  EXPECT_TRUE(has_label(r, "fs01/data_credit_stalls")) << join_lines(r);
  EXPECT_TRUE(has_label(r, "fs01/data_read_latency")) << join_lines(r);
  EXPECT_TRUE(has_label(r, "fs01/data_write_latency")) << join_lines(r);
  EXPECT_TRUE(has_label(r, "fs01/data_queue_length")) << join_lines(r);
}

TEST(SmbClientCheck, ShortThresholdsBoundLatency) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.012, 0.001);
  const auto warn = run(snapshot_of(in), {"-w", "10", "-c", "20"});
  EXPECT_EQ(warn.result(), PB::Common::ResultCode::WARNING) << join_lines(warn);
  const auto crit = run(snapshot_of(in), {"-w", "5", "-c", "10"});
  EXPECT_EQ(crit.result(), PB::Common::ResultCode::CRITICAL) << join_lines(crit);
}

TEST(SmbClientCheck, CustomThresholdsOnOtherKeywords) {
  instance_values in;
  in["\\\\fs01\\data"] = share_counters(0.001, 0.001, 0, 12);
  const auto r = run(snapshot_of(in), {"warning=queue_length > 10", "critical=none"});
  EXPECT_EQ(r.result(), PB::Common::ResultCode::WARNING) << join_lines(r);
}

#ifndef WIN32
TEST(SmbClientCheck, LiveCheckIsUnavailableOutsideWindows) {
  PB::Commands::QueryResponseMessage::Response response;
  smb_client_check::check::check_smb_client(request_for({}), &response);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("only available on Windows"), std::string::npos) << join_lines(response);
}
#endif
