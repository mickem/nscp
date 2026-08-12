// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_disk_io command-level tests: performance-data labels and the byte
// formatting functions (#1392). Built into both the Windows and the Unix test
// binary - the check itself is platform-neutral, only the fetch is not - so
// neither the singleton nor any platform data source is defined here.

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "check_disk_io.hpp"

namespace {
disk_io_check::disks_type one_disk() {
  disk_io_check::disks_type disks;
  disk_io_check::disk_io d;
  d.name = "C:";
  d.percent_disk_time = 10;
  d.queue_length = 3;
  d.reads_per_sec = 54;
  d.writes_per_sec = 15;
  d.read_bytes_per_sec = 21967407;
  d.write_bytes_per_sec = 640073;
  disks.push_back(d);
  return disks;
}

PB::Common::ResultCode run_io_check(const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_disk_io");
  for (const std::string &a : args) request.add_arguments(a);
  disk_io_check::check::check_disk_io(request, &response, one_disk());
  return response.result();
}

std::vector<std::string> perf_aliases(const PB::Commands::QueryResponseMessage::Response &r) {
  std::vector<std::string> out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) out.push_back(r.lines(i).perf(j).alias());
  }
  return out;
}

bool has_perf(const PB::Commands::QueryResponseMessage::Response &r, const std::string &alias) {
  const std::vector<std::string> aliases = perf_aliases(r);
  return std::find(aliases.begin(), aliases.end(), alias) != aliases.end();
}

std::string all_messages(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}
}  // namespace

// ============================================================================
// Performance data labels
//
// queue_length and percent_disk_time used to register their generator with
// neither prefix nor suffix, so both were emitted as the bare perf-syntax
// alias and a store that keys by label kept only one of them.
// ============================================================================

TEST(CheckDiskIo, LoadKeywordsCarryTheirOwnPerfLabel) {
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "perf-config=extra(queue_length)"}, response);

  EXPECT_TRUE(has_perf(response, "C:_queue_length")) << all_messages(response);
  EXPECT_TRUE(has_perf(response, "C:_percent_disk_time"));
  EXPECT_FALSE(has_perf(response, "C:"));
}

TEST(CheckDiskIo, CoReferencedKeywordsGetDistinctLabels) {
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "warning=queue_length > 100 or percent_disk_time > 100 or iops > 10000"}, response);

  const std::vector<std::string> aliases = perf_aliases(response);
  ASSERT_EQ(aliases.size(), 3u) << all_messages(response);
  const std::set<std::string> unique(aliases.begin(), aliases.end());
  EXPECT_EQ(unique.size(), aliases.size());
}

TEST(CheckDiskIo, PerfConfigCanPinTheOldBareLabel) {
  // The escape hatch for anyone who wants the pre-#1392 name back.
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "perf-config=percent_disk_time(suffix:none)"}, response);

  EXPECT_TRUE(has_perf(response, "C:")) << all_messages(response);
}

// ============================================================================
// Byte formatting (the filter grammar has no arithmetic of its own)
// ============================================================================

TEST(CheckDiskIo, FormatBytesRendersByteRatesReadably) {
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "detail-syntax=%(name) read=%(format_bytes(read_bytes_per_sec))/s"}, response);

  const std::string message = all_messages(response);
  EXPECT_NE(message.find("read=20.95MB/s"), std::string::npos) << message;
}

TEST(CheckDiskIo, FormatBytesAcceptsAnExplicitUnit) {
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "detail-syntax=%(format_bytes(total_bytes_per_sec, 'KB'))"}, response);

  const std::string message = all_messages(response);
  EXPECT_NE(message.find("22077.6"), std::string::npos) << message;
}

TEST(CheckDiskIo, ConvertBytesDrivesNumericThresholds) {
  PB::Commands::QueryResponseMessage::Response response;
  // ~20.9MB/s read: warn above 10MB/s, and confirm it stays OK above 100MB/s
  // (a string comparison would order "20.9" above "100").
  run_io_check({"filter=none", "warning=convert_bytes(read_bytes_per_sec, 'MB') > 10"}, response);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << all_messages(response);

  PB::Commands::QueryResponseMessage::Response quiet;
  run_io_check({"filter=none", "warning=convert_bytes(read_bytes_per_sec, 'MB') > 100"}, quiet);
  EXPECT_EQ(quiet.result(), PB::Common::ResultCode::OK) << all_messages(quiet);
}

TEST(CheckDiskIo, ScaleDrivesNumericThresholds) {
  PB::Commands::QueryResponseMessage::Response response;
  run_io_check({"filter=none", "warning=scale(read_bytes_per_sec, 1000000) > 20"}, response);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << all_messages(response);
}
