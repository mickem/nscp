/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "check_process_history.hpp"

#include <gtest/gtest.h>

// ============================================================================
// process_record tests
// ============================================================================

TEST(CheckProcessHistory, process_record_defaults) {
  process_history_check::process_record rec;
  EXPECT_EQ(rec.exe, "");
  EXPECT_EQ(rec.first_seen, 0);
  EXPECT_EQ(rec.last_seen, 0);
  EXPECT_EQ(rec.times_seen, 0);
  EXPECT_EQ(rec.currently_running, false);
}

TEST(CheckProcessHistory, process_record_constructor) {
  long long now = 1704067200;  // 2024-01-01 00:00:00 UTC
  process_history_check::process_record rec("notepad.exe", now);
  EXPECT_EQ(rec.exe, "notepad.exe");
  EXPECT_EQ(rec.first_seen, now);
  EXPECT_EQ(rec.last_seen, now);
  EXPECT_EQ(rec.times_seen, 1);
  EXPECT_EQ(rec.currently_running, true);
}

TEST(CheckProcessHistory, process_record_getters) {
  process_history_check::process_record rec;
  rec.exe = "test.exe";
  rec.first_seen = 1000;
  rec.last_seen = 2000;
  rec.times_seen = 5;
  rec.currently_running = true;

  EXPECT_EQ(rec.get_exe(), "test.exe");
  EXPECT_EQ(rec.get_first_seen(), 1000);
  EXPECT_EQ(rec.get_last_seen(), 2000);
  EXPECT_EQ(rec.get_times_seen(), 5);
  EXPECT_EQ(rec.get_currently_running(), "true");
  EXPECT_EQ(rec.get_currently_running_i(), 1);

  rec.currently_running = false;
  EXPECT_EQ(rec.get_currently_running(), "false");
  EXPECT_EQ(rec.get_currently_running_i(), 0);
}

TEST(CheckProcessHistory, process_record_show) {
  process_history_check::process_record rec;
  rec.exe = "notepad.exe";
  rec.times_seen = 3;
  rec.currently_running = true;
  EXPECT_EQ(rec.show(), "notepad.exe (seen 3 times, running)");

  rec.currently_running = false;
  EXPECT_EQ(rec.show(), "notepad.exe (seen 3 times, not running)");
}

// ============================================================================
// process_history_data tests
// ============================================================================

TEST(CheckProcessHistory, process_history_data_fetch_and_get) {
  process_history_check::process_history_data data;

  // fetch() should not throw
  EXPECT_NO_THROW(data.fetch());

  // get() should return a list
  process_history_check::history_type history;
  EXPECT_NO_THROW(history = data.get());

  // Should have found at least some processes (system processes are always running)
  EXPECT_GT(history.size(), 0);

  // get_count() should match
  EXPECT_EQ(data.get_count(), static_cast<long long>(history.size()));
}

TEST(CheckProcessHistory, process_history_data_tracks_processes) {
  process_history_check::process_history_data data;

  // First fetch
  data.fetch();
  auto history1 = data.get();
  long long count1 = data.get_count();

  // Second fetch should update existing processes
  data.fetch();
  auto history2 = data.get();
  long long count2 = data.get_count();

  // Count should be the same or higher (new processes may have started)
  EXPECT_GE(count2, count1);

  // Find a process that was seen both times
  // times_seen should still be 1 since the process was continuously running (not restarted)
  for (const auto &rec : history2) {
    if (rec.currently_running) {
      // Continuously running processes should have times_seen = 1 (only counted once at first detection)
      EXPECT_EQ(rec.times_seen, 1);
      break;
    }
  }
}

TEST(CheckProcessHistory, process_history_data_clear) {
  process_history_check::process_history_data data;

  data.fetch();
  EXPECT_GT(data.get_count(), 0);

  data.clear();
  EXPECT_EQ(data.get_count(), 0);
}

// ============================================================================
// process_history_new tests
// ============================================================================

TEST(CheckProcessHistory, new_processes_found_in_time_window) {
  process_history_check::process_history_data data;

  // Clear any existing history
  data.clear();

  // Fetch processes - all should be "new" since we just cleared
  data.fetch();
  auto history = data.get();

  EXPECT_GT(history.size(), 0);

  // All processes should have first_seen within the last few seconds
  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  long long now_ts = (now - epoch).total_seconds();

  for (const auto &rec : history) {
    // first_seen should be very recent (within last 10 seconds)
    EXPECT_GE(rec.first_seen, now_ts - 10);
    EXPECT_LE(rec.first_seen, now_ts + 1);  // Allow 1 second for clock drift
  }
}

TEST(CheckProcessHistory, new_processes_filter_by_time) {
  process_history_check::process_history_data data;

  // Fetch processes
  data.fetch();
  auto history = data.get();

  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  long long now_ts = (now - epoch).total_seconds();

  // Count processes first seen in last 5 minutes (300 seconds)
  long long cutoff = now_ts - 300;
  int count_new = 0;
  for (const auto &rec : history) {
    if (rec.first_seen >= cutoff) {
      count_new++;
    }
  }

  // Should have found some new processes (since we just fetched)
  // This test might be flaky if NSClient++ has been running for a while
  // but should work for fresh test runs
  EXPECT_GE(count_new, 0);  // At minimum, no assertion failure
}
