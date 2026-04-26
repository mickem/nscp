/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <gtest/gtest.h>

#include <rrd_buffer.hpp>

namespace {

// ============================================================================
// A minimal value_type that satisfies rrd_buffer's requirements:
//   - add(other) : accumulate values
//   - normalize(count) : divide by count to produce an average
// ============================================================================
struct double_value {
  double value = 0.0;

  double_value() = default;
  explicit double_value(double v) : value(v) {}

  void add(const double_value &other) { value += other.value; }
  void normalize(double count) {
    if (count > 0.0) value /= count;
  }
};

}  // namespace

// ============================================================================
// Construction
// ============================================================================

TEST(rrd_buffer, default_constructed_buffers_are_empty) {
  rrd_buffer<double_value> buf;
  // A freshly constructed buffer has no pushed values; all circular_buffer
  // slots are value-initialised to 0.  get_average of 0 seconds returns
  // a zero-initialised value.
  EXPECT_DOUBLE_EQ(buf.get_average(0).value, 0.0);
}

// ============================================================================
// push() and seconds-range average
// ============================================================================

TEST(rrd_buffer, push_single_value_readable_in_seconds_range) {
  rrd_buffer<double_value> buf;
  buf.push(double_value(10.0));
  // Average over the last 1 second (1 pushed value, 59 zero slots)
  // get_average(1) iterates the last 1 element of the seconds buffer
  double avg1 = buf.get_average(1).value;
  EXPECT_DOUBLE_EQ(avg1, 10.0);
}

TEST(rrd_buffer, push_multiple_values_seconds_average) {
  rrd_buffer<double_value> buf;
  buf.push(double_value(10.0));
  buf.push(double_value(20.0));
  // Last 2 elements: 10 and 20; average = (10+20)/2 = 15
  double avg = buf.get_average(2).value;
  EXPECT_DOUBLE_EQ(avg, 15.0);
}

TEST(rrd_buffer, push_many_values_full_seconds_window) {
  rrd_buffer<double_value> buf;
  // Push 60 values of 1.0 each to fill the seconds ring
  for (int i = 0; i < 60; ++i) {
    buf.push(double_value(1.0));
  }
  double avg = buf.get_average(60).value;
  EXPECT_DOUBLE_EQ(avg, 1.0);
}

TEST(rrd_buffer, push_overwrite_seconds_buffer_keeps_last_60) {
  rrd_buffer<double_value> buf;
  // Push 60 zeros then 1 non-zero value
  for (int i = 0; i < 60; ++i) {
    buf.push(double_value(0.0));
  }
  buf.push(double_value(60.0));
  // The last 1 element should be 60.0
  EXPECT_DOUBLE_EQ(buf.get_average(1).value, 60.0);
  // The last 2 elements: 0.0 and 60.0, average = 30.0
  EXPECT_DOUBLE_EQ(buf.get_average(2).value, 30.0);
}

// ============================================================================
// Negative time returns zero-initialised value
// ============================================================================

TEST(rrd_buffer, get_average_negative_time_returns_zero_value) {
  rrd_buffer<double_value> buf;
  buf.push(double_value(100.0));
  double result = buf.get_average(-1).value;
  EXPECT_DOUBLE_EQ(result, 0.0);
}

// ============================================================================
// Minutes-range average (time > 60 triggers the minutes path)
// ============================================================================

TEST(rrd_buffer, minutes_average_after_filling_two_minute_buckets) {
  rrd_buffer<double_value> buf;
  // Each push triggers a minutes entry after 60 seconds.
  // Push 60 values of 2.0 → first minute bucket gets average = 2.0
  for (int i = 0; i < 60; ++i) buf.push(double_value(2.0));
  // Push 60 more values of 4.0 → second minute bucket gets average = 4.0
  for (int i = 0; i < 60; ++i) buf.push(double_value(4.0));
  // time=120 → 120/60 = 2 minutes → average of 2 minute entries = (2+4)/2 = 3
  double avg = buf.get_average(120).value;
  EXPECT_DOUBLE_EQ(avg, 3.0);
}

// ============================================================================
// get_average throws when hours range is out of bounds
// ============================================================================

TEST(rrd_buffer, get_average_throws_when_hours_exceed_buffer_size) {
  rrd_buffer<double_value> buf;
  // time > 60*60 = 3600 seconds selects the hours path.
  // 25 hours worth of seconds would request 25 entries but hours buffer only
  // holds 24; any value > 24*3600 triggers the exception path.
  EXPECT_THROW(buf.get_average(25 * 60 * 60), nsclient::nsclient_exception);
}

// ============================================================================
// calculate_avg on the internal buffers (indirectly via push cycle)
// ============================================================================

TEST(rrd_buffer, constant_push_produces_constant_average) {
  rrd_buffer<double_value> buf;
  for (int i = 0; i < 30; ++i) buf.push(double_value(5.0));
  double avg = buf.get_average(30).value;
  EXPECT_DOUBLE_EQ(avg, 5.0);
}

TEST(rrd_buffer, varying_push_correct_average_over_window) {
  rrd_buffer<double_value> buf;
  // Push 10 ones then 10 threes; last 20 elements: average = (10*1 + 10*3)/20 = 2
  for (int i = 0; i < 10; ++i) buf.push(double_value(1.0));
  for (int i = 0; i < 10; ++i) buf.push(double_value(3.0));
  double avg = buf.get_average(20).value;
  EXPECT_DOUBLE_EQ(avg, 2.0);
}
