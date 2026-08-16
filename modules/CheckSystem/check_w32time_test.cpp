// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// check_w32time judges whether the machine is following a time source at all,
// which is the failure that breaks Kerberos long before the clock looks wrong.
// The Windows-specific half only gathers values; everything below is the part
// that decides what they mean.

#include "check_w32time.hpp"

#include <gtest/gtest.h>

using w32time_check::build_w32time_obj;
using w32time_check::is_local_clock;
using w32time_check::parse_ntp_servers;
using w32time_check::us_to_ms;
using w32time_check::w32time_data;
using w32time_check::w32time_obj;

namespace {

const long long now = 1760000000;  // fixed "current time" for age arithmetic

// FILETIME for an instant `seconds_ago` before `now` (100ns ticks since 1601).
unsigned long long filetime_ago(const long long seconds_ago) {
  const long long epoch = now - seconds_ago;
  return (static_cast<unsigned long long>(epoch) + 11644473600ULL) * 10000000ULL;
}

// A healthy domain member: service up, following its domain hierarchy.
w32time_data healthy() {
  w32time_data data;
  data.installed = true;
  data.service_state = "running";
  data.start_type = "auto";
  data.sync_type = "NT5DS";
  data.live_source = "dc01.corp.example.com";
  data.last_good_filetime = filetime_ago(600);
  data.offset_us = 3500LL;  // 3ms
  data.delay_us = 21000LL;  // 21ms
  data.frequency_adjustment_ppb = 1200LL;
  data.time_sources = 1LL;
  return data;
}

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run(const w32time_data &data, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_w32time");
  for (const std::string &a : args) request.add_arguments(a);
  w32time_check::check_w32time_from(request, &response, data, now);
  return response.result();
}

}  // namespace

TEST(CheckW32Time, ParsesTheConfiguredPeerList) {
  const std::vector<std::string> one = parse_ntp_servers("time.windows.com,0x9");
  ASSERT_EQ(1u, one.size());
  EXPECT_EQ("time.windows.com", one[0]);

  const std::vector<std::string> many = parse_ntp_servers("0.pool.ntp.org,0x8 1.pool.ntp.org,0x8 ntp.corp.example.com,0x1");
  ASSERT_EQ(3u, many.size());
  EXPECT_EQ("0.pool.ntp.org", many[0]);
  EXPECT_EQ("ntp.corp.example.com", many[2]);

  // A bare host without the flag field is just as valid.
  const std::vector<std::string> bare = parse_ntp_servers("ntp1.example.com ntp2.example.com");
  ASSERT_EQ(2u, bare.size());
  EXPECT_EQ("ntp2.example.com", bare[1]);

  EXPECT_TRUE(parse_ntp_servers("").empty());
  EXPECT_TRUE(parse_ntp_servers("   ").empty());
}

TEST(CheckW32Time, RecognisesTheLocalClockSources) {
  EXPECT_TRUE(is_local_clock("Local CMOS Clock"));
  EXPECT_TRUE(is_local_clock("Free-running System Clock"));
  EXPECT_FALSE(is_local_clock("dc01.corp.example.com"));
  EXPECT_FALSE(is_local_clock("VM IC Time Synchronization Provider"));
  EXPECT_FALSE(is_local_clock(""));
}

TEST(CheckW32Time, ConvertsCounterMicrosecondsToMilliseconds) {
  EXPECT_EQ(0, *us_to_ms(boost::optional<long long>(999)));
  EXPECT_EQ(1, *us_to_ms(boost::optional<long long>(1000)));
  EXPECT_EQ(42, *us_to_ms(boost::optional<long long>(42999)));
  EXPECT_FALSE(us_to_ms(boost::none));  // unknown stays unknown
}

TEST(CheckW32Time, HealthyDomainMemberIsSynchronized) {
  const w32time_obj obj = build_w32time_obj(healthy(), now);

  EXPECT_EQ(1, obj.get_installed());
  EXPECT_EQ(1, obj.get_running());
  EXPECT_EQ(1, obj.get_synchronized());
  EXPECT_EQ(0, obj.get_local_clock());
  EXPECT_EQ("NT5DS", obj.get_sync_type());
  EXPECT_EQ("dc01.corp.example.com", obj.get_source());
  EXPECT_EQ("service", obj.get_source_from());
  ASSERT_TRUE(obj.get_offset());
  EXPECT_EQ(3, *obj.get_offset());
  ASSERT_TRUE(obj.get_delay());
  EXPECT_EQ(21, *obj.get_delay());
  ASSERT_TRUE(obj.get_last_sync_age());
  EXPECT_EQ(600, *obj.get_last_sync_age());
  EXPECT_EQ("synchronizing with dc01.corp.example.com (offset 3ms)", obj.get_state());
}

TEST(CheckW32Time, StoppedServiceIsNotSynchronized) {
  w32time_data data = healthy();
  data.service_state = "stopped";
  data.start_type = "demand";
  data.live_source = "";

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(0, obj.get_running());
  EXPECT_EQ(0, obj.get_synchronized());
  EXPECT_EQ("the Windows Time service is stopped (start type demand)", obj.get_state());
}

TEST(CheckW32Time, MissingServiceIsReportedAsNotInstalled) {
  w32time_data data;
  data.installed = false;

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(0, obj.get_installed());
  EXPECT_EQ("not installed", obj.get_service_state());
  EXPECT_EQ(0, obj.get_synchronized());
  EXPECT_EQ("the Windows Time service is not installed", obj.get_state());
}

TEST(CheckW32Time, NoSyncConfigurationIsNotSynchronized) {
  w32time_data data = healthy();
  data.sync_type = "NoSync";

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(1, obj.get_running());
  EXPECT_EQ(0, obj.get_synchronized());
  EXPECT_EQ("time synchronization is turned off (Type=NoSync)", obj.get_state());
}

TEST(CheckW32Time, FallingBackToTheLocalClockIsNotSynchronized) {
  // The domain hierarchy broke: w32time keeps running but follows nothing.
  w32time_data data = healthy();
  data.live_source = "Local CMOS Clock";

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(1, obj.get_running());
  EXPECT_EQ(1, obj.get_local_clock());
  EXPECT_EQ(0, obj.get_synchronized());
  EXPECT_EQ("not synchronizing: falling back to Local CMOS Clock", obj.get_state());
}

TEST(CheckW32Time, UsingNoTimeSourceIsNotSynchronizedEvenWhenTheServiceRuns) {
  // W32TimeQuerySource needs privilege the caller may not have, so the source
  // can be unknown while the service runs. "NTP Client Time Source Count = 0"
  // then settles it: the peer is configured but nothing is being followed.
  w32time_data data = healthy();
  data.live_source = "";
  data.sync_type = "NTP";
  data.ntp_server = "time.windows.com,0x9";
  data.time_sources = 0LL;

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(1, obj.get_running());
  EXPECT_EQ(0, obj.get_synchronized());
  EXPECT_EQ("not synchronizing: no time source in use (configured: time.windows.com)", obj.get_state());
}

TEST(CheckW32Time, ALiveSourceOutranksTheTimeSourceCount) {
  // When the service told us what it follows, that answer decides - a momentary
  // zero in the counter must not override it.
  w32time_data data = healthy();
  data.time_sources = 0LL;

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(1, obj.get_synchronized());
  EXPECT_EQ("synchronizing with dc01.corp.example.com (offset 3ms)", obj.get_state());
}

TEST(CheckW32Time, WithoutAnyEvidenceTheCheckDoesNotCryWolf) {
  // Neither the source nor the counter is readable: say what it is configured
  // to do rather than inventing a verdict either way.
  w32time_data data = healthy();
  data.live_source = "";
  data.ntp_server = "ntp.example.com,0x8";
  data.time_sources = boost::none;

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ(1, obj.get_synchronized());
  EXPECT_EQ("configured to synchronize with ntp.example.com (offset 3ms)", obj.get_state());
}

TEST(CheckW32Time, FallsBackToTheConfiguredPeersWhenTheServiceCannotBeAsked) {
  w32time_data data = healthy();
  data.service_state = "stopped";
  data.live_source = "";
  data.sync_type = "NTP";
  data.ntp_server = "time.windows.com,0x9 ntp2.example.com,0x8";

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_EQ("time.windows.com, ntp2.example.com", obj.get_peers());
  EXPECT_EQ(2, obj.get_peer_count());
  EXPECT_EQ("time.windows.com, ntp2.example.com", obj.get_source());
  EXPECT_EQ("configuration", obj.get_source_from());
}

TEST(CheckW32Time, UnknownCountersAndSyncTimeStayUnknown) {
  w32time_data data = healthy();
  data.offset_us = boost::none;
  data.delay_us = boost::none;
  data.frequency_adjustment_ppb = boost::none;
  data.time_sources = boost::none;
  data.last_good_filetime = 0;

  const w32time_obj obj = build_w32time_obj(data, now);
  EXPECT_FALSE(obj.get_offset());
  EXPECT_FALSE(obj.get_delay());
  EXPECT_FALSE(obj.get_frequency_adjustment());
  EXPECT_FALSE(obj.get_time_sources());
  EXPECT_FALSE(obj.get_last_sync_age());
  EXPECT_EQ("unknown", obj.get_last_sync());
  // Without a measurement the state names the source and nothing else.
  EXPECT_EQ("synchronizing with dc01.corp.example.com", obj.get_state());
}

TEST(CheckW32Time, ANegativeFrequencyAdjustmentIsAValueNotAnUnknown) {
  // The service slows the clock down as readily as it speeds it up, so a
  // negative correction must survive as a number.
  w32time_data data = healthy();
  data.frequency_adjustment_ppb = -4500;

  const w32time_obj obj = build_w32time_obj(data, now);
  ASSERT_TRUE(obj.get_frequency_adjustment());
  EXPECT_EQ(-4500, *obj.get_frequency_adjustment());
}

// ── thresholds ───────────────────────────────────────────────────────────────

TEST(CheckW32Time, DefaultsAreOkForAHealthyHost) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(healthy(), {}, response));
  EXPECT_NE(std::string::npos, join_lines(response).find("synchronizing with dc01.corp.example.com"));
}

TEST(CheckW32Time, DefaultsAreCriticalWhenNotFollowingAnySource) {
  w32time_data data = healthy();
  data.service_state = "stopped";
  data.live_source = "";

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(data, {}, response));
  EXPECT_NE(std::string::npos, join_lines(response).find("the Windows Time service is stopped"));
}

TEST(CheckW32Time, DefaultsWarnOnAModestDriftAndGoCriticalOnALargeOne) {
  w32time_data drifting = healthy();
  drifting.offset_us = 4LL * 1000 * 1000;  // 4 seconds, past the 1s warning
  PB::Commands::QueryResponseMessage::Response warn;
  EXPECT_EQ(PB::Common::ResultCode::WARNING, run(drifting, {}, warn));

  w32time_data far_off = healthy();
  far_off.offset_us = 120LL * 1000 * 1000;  // 2 minutes, Kerberos is next
  PB::Commands::QueryResponseMessage::Response crit;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(far_off, {}, crit));
}

TEST(CheckW32Time, AnUnknownOffsetDoesNotTripTheOffsetThresholds) {
  w32time_data data = healthy();
  data.offset_us = boost::none;

  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(data, {}, response));

  // ...and it renders (and can be tested for) as 'unknown' rather than a number.
  PB::Commands::QueryResponseMessage::Response rendered;
  run(data, {"warning=none", "critical=none", "detail-syntax=offset=${offset}"}, rendered);
  EXPECT_NE(std::string::npos, join_lines(rendered).find("offset=unknown"));
}

TEST(CheckW32Time, ThresholdsCanBeWrittenAgainstEveryKeyword) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL,
            run(healthy(), {"critical=sync_type = 'NT5DS' and peer_count = 0", "warning=none"}, response));

  PB::Commands::QueryResponseMessage::Response quiet;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(healthy(), {"critical=last_sync_age > 86400", "warning=none"}, quiet));
}

TEST(CheckW32Time, EmitsOffsetPerfdataWhenThresholded) {
  PB::Commands::QueryResponseMessage::Response response;
  run(healthy(), {"warning=offset > 1000"}, response);
  ASSERT_EQ(1, response.lines_size());
  bool found = false;
  for (const auto &perf : response.lines(0).perf()) {
    if (perf.alias() == "w32time_offset") found = true;
  }
  EXPECT_TRUE(found);
}

TEST(CheckW32Time, LastSyncAgeAcceptsDurationThresholds) {
  // Sibling age keywords already take durations; last_sync_age must too, or
  // "last_sync_age > 24h" silently reads as 24 seconds and always fires.
  PB::Commands::QueryResponseMessage::Response quiet;
  EXPECT_EQ(PB::Common::ResultCode::OK, run(healthy(), {"warning=none", "critical=last_sync_age > 24h"}, quiet));  // 600s old

  PB::Commands::QueryResponseMessage::Response fires;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(healthy(), {"warning=none", "critical=last_sync_age > 5m"}, fires));

  // Plain seconds keep working.
  PB::Commands::QueryResponseMessage::Response seconds;
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, run(healthy(), {"warning=none", "critical=last_sync_age > 599"}, seconds));
}
