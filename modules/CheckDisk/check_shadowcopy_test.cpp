// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_shadowcopy.hpp"

#include <gtest/gtest.h>

// nscapi::plugin_singleton is defined once in check_disk_io_test.cpp for the
// merged check_disk_test target.

using shadowcopy_check::build_volumes;
using shadowcopy_check::parse_cim_datetime;
using shadowcopy_check::raw_shadow;
using shadowcopy_check::raw_storage;
using shadowcopy_check::shadowcopy;
using shadowcopy_check::volume_guid;
using shadowcopy_check::volumes_type;

namespace {
raw_shadow shadow(const std::string &vol, const std::string &installed) {
  raw_shadow s;
  s.volume = vol;
  s.install_epoch = parse_cim_datetime(installed);
  return s;
}
}  // namespace

TEST(ShadowCopy, ParsesCimDatetimeUtc) {
  // 2024-01-15 08:30:00, zero UTC offset -> a fixed epoch.
  const long long e = parse_cim_datetime("20240115083000.000000-000");
  EXPECT_GT(e, 0);
  // 1705307400 = 2024-01-15 08:30:00 UTC.
  EXPECT_EQ(e, 1705307400LL);
}

TEST(ShadowCopy, CimDatetimeOffsetShiftsToUtc) {
  // Same wall clock, +060 (60 min ahead of UTC) => one hour earlier in UTC.
  const long long utc = parse_cim_datetime("20240115083000.000000-000");
  const long long plus1 = parse_cim_datetime("20240115083000.000000+060");
  EXPECT_EQ(utc - plus1, 3600LL);
}

TEST(ShadowCopy, RejectsBadDatetime) {
  EXPECT_EQ(parse_cim_datetime(""), 0);
  EXPECT_EQ(parse_cim_datetime("not-a-date"), 0);
  EXPECT_EQ(parse_cim_datetime("2024"), 0);
}

TEST(ShadowCopy, VolumeGuidExtraction) {
  EXPECT_EQ(volume_guid("\\\\?\\Volume{abc-123}\\"), "Volume{abc-123}");
  EXPECT_EQ(volume_guid("Win32_Volume.DeviceID=\"\\\\?\\Volume{abc-123}\\\""), "Volume{abc-123}");
  EXPECT_EQ(volume_guid("C:\\"), "");
}

TEST(ShadowCopy, GroupsByVolumeAndFindsNewest) {
  const std::vector<raw_shadow> shadows = {
      shadow("\\\\?\\Volume{v1}\\", "20240115083000.000000-000"),
      shadow("\\\\?\\Volume{v1}\\", "20240116083000.000000-000"),  // newest on v1
      shadow("\\\\?\\Volume{v2}\\", "20240101083000.000000-000"),
  };
  // now = 2024-01-17 08:30:00 UTC
  const long long now = parse_cim_datetime("20240117083000.000000-000");
  const volumes_type vols = build_volumes(shadows, {}, now);
  ASSERT_EQ(vols.size(), 2u);

  const shadowcopy &v1 = vols.front();
  EXPECT_EQ(v1.get_volume(), "\\\\?\\Volume{v1}\\");
  EXPECT_EQ(v1.get_count(), 2);
  // Newest v1 copy is one day old.
  EXPECT_EQ(v1.get_newest(), 86400LL);
}

TEST(ShadowCopy, JoinsShadowStorageByGuid) {
  const std::vector<raw_shadow> shadows = {shadow("\\\\?\\Volume{v1}\\", "20240116083000.000000-000")};
  raw_storage st;
  st.volume = "Win32_Volume.DeviceID=\"\\\\?\\Volume{v1}\\\"";
  st.used = 250;
  st.allocated = 300;
  st.max_size = 1000;
  const long long now = parse_cim_datetime("20240117083000.000000-000");
  const volumes_type vols = build_volumes(shadows, {st}, now);
  ASSERT_EQ(vols.size(), 1u);
  const shadowcopy &v = vols.front();
  EXPECT_EQ(v.get_used(), 250);
  EXPECT_EQ(v.get_max_size(), 1000);
  EXPECT_EQ(v.get_used_pct(), 25);
}

TEST(ShadowCopy, UnboundedMaxSizeIsInert) {
  const std::vector<raw_shadow> shadows = {shadow("\\\\?\\Volume{v1}\\", "20240116083000.000000-000")};
  raw_storage st;
  st.volume = "\\\\?\\Volume{v1}\\";
  st.used = 500;
  st.max_size = -1;  // UINT64_MAX read as -1 => "unbounded"
  const volumes_type vols = build_volumes(shadows, {st}, parse_cim_datetime("20240117083000.000000-000"));
  ASSERT_EQ(vols.size(), 1u);
  EXPECT_EQ(vols.front().get_max_size(), 0);
  EXPECT_EQ(vols.front().get_used_pct(), 0);  // guarded against max_size <= 0
}

TEST(ShadowCopy, UnknownDateGivesNegativeAge) {
  const std::vector<raw_shadow> shadows = {shadow("\\\\?\\Volume{v1}\\", "garbage")};
  const volumes_type vols = build_volumes(shadows, {}, parse_cim_datetime("20240117083000.000000-000"));
  ASSERT_EQ(vols.size(), 1u);
  EXPECT_EQ(vols.front().get_count(), 1);
  EXPECT_EQ(vols.front().get_newest(), -1);  // no parseable date
  EXPECT_EQ(vols.front().get_newest_date(), "unknown");
}

// --- the check over volumes already read --------------------------------------

namespace {
// One volume whose newest copy is `installed`, judged at 2024-01-17 08:30 UTC,
// with `used` bytes of shadow storage.
PB::Commands::QueryResponseMessage::Response evaluate_one(const std::string &installed, const std::vector<std::string> &args, long long used = 0) {
  raw_storage st;
  st.volume = "Win32_Volume.DeviceID=\"\\\\?\\Volume{v1}\\\"";
  st.used = used;
  const volumes_type vols = build_volumes({shadow("\\\\?\\Volume{v1}\\", installed)}, {st}, parse_cim_datetime("20240117083000.000000-000"));
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_shadowcopy");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  shadowcopy_check::check::evaluate(request, &response, vols);
  return response;
}
std::string message_of(const PB::Commands::QueryResponseMessage::Response &r) { return r.lines_size() > 0 ? r.lines(0).message() : ""; }
}  // namespace

TEST(ShadowCopyCheck, DefaultThresholdsAreDurations) {
  // `newest > 26h` / `newest > 50h`. As a plain int keyword the unit was
  // refused and both compared against 0, so every volume was CRITICAL.
  const auto fresh = evaluate_one("20240116223000.000000-000", {});  // 10h old
  EXPECT_EQ(PB::Common::ResultCode::OK, fresh.result()) << message_of(fresh);
  const auto stale = evaluate_one("20240116023000.000000-000", {});  // 30h old
  EXPECT_EQ(PB::Common::ResultCode::WARNING, stale.result()) << message_of(stale);
  const auto old = evaluate_one("20240114203000.000000-000", {});  // 60h old
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, old.result()) << message_of(old);
}

TEST(ShadowCopyCheck, NewestTakesPlainSecondsAndOtherUnits) {
  const std::string ten_hours_old = "20240116223000.000000-000";
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, evaluate_one(ten_hours_old, {"warning=none", "critical=newest > 35999"}).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, evaluate_one(ten_hours_old, {"warning=none", "critical=newest > 36000"}).result());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, evaluate_one(ten_hours_old, {"warning=none", "critical=newest > 599m"}).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, evaluate_one(ten_hours_old, {"warning=none", "critical=newest > 1d"}).result());
}

TEST(ShadowCopyCheck, StorageKeywordsAreByteCounts) {
  const std::string fresh = "20240116223000.000000-000";
  const long long three_gib = 3LL * 1024 * 1024 * 1024;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, evaluate_one(fresh, {"warning=none", "critical=used > 2G"}, three_gib).result());
  EXPECT_EQ(PB::Common::ResultCode::OK, evaluate_one(fresh, {"warning=none", "critical=used > 4G"}, three_gib).result());
}
