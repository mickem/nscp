// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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

// ============================================================================
// Warm-up: the rings are created full of zero slots, so only the samples
// actually pushed may be reported
// ============================================================================

TEST(rrd_buffer, has_data_is_false_until_a_sample_is_pushed) {
  rrd_buffer<double_value> buf;
  EXPECT_FALSE(buf.has_data());
  EXPECT_EQ(buf.sampled_seconds(), 0u);
  buf.push(double_value(1.0));
  EXPECT_TRUE(buf.has_data());
  EXPECT_EQ(buf.sampled_seconds(), 1u);
}

// A five-minute window asked for after thirty seconds is answered from the
// thirty samples there are - not from five empty minute slots, which read as
// an idle machine however busy it was.
TEST(rrd_buffer, window_longer_than_sampled_is_answered_from_the_samples) {
  rrd_buffer<double_value> buf;
  for (int i = 0; i < 30; ++i) buf.push(double_value(50.0));
  EXPECT_DOUBLE_EQ(buf.get_average(300).value, 50.0);
  EXPECT_DOUBLE_EQ(buf.get_average(60).value, 50.0);
}

// Ninety seconds in, the one whole minute there is answers for five.
TEST(rrd_buffer, window_longer_than_sampled_uses_the_minutes_that_exist) {
  rrd_buffer<double_value> buf;
  for (int i = 0; i < 60; ++i) buf.push(double_value(40.0));
  for (int i = 0; i < 30; ++i) buf.push(double_value(80.0));
  EXPECT_DOUBLE_EQ(buf.get_average(300).value, 40.0);
  // The seconds ring is unaffected: the last 30 are still the newest samples.
  EXPECT_DOUBLE_EQ(buf.get_average(30).value, 80.0);
}

// Once the window has really been sampled it is reported as before.
TEST(rrd_buffer, fully_sampled_window_is_unchanged) {
  rrd_buffer<double_value> buf;
  for (int i = 0; i < 300; ++i) buf.push(double_value(i < 240 ? 0.0 : 100.0));
  // Five minute slots: four at 0 and one at 100.
  EXPECT_DOUBLE_EQ(buf.get_average(300).value, 20.0);
}

TEST(rrd_buffer, sample_count_saturates_at_the_span_of_the_buffers) {
  rrd_buffer<double_value> buf;
  for (int i = 0; i < 24 * 60 * 60 + 10; ++i) buf.push(double_value(1.0));
  EXPECT_EQ(buf.sampled_seconds(), 24u * 60u * 60u);
  // The 24-hour window itself is still beyond what the hours ring can answer.
  EXPECT_THROW(buf.get_average(24 * 60 * 60), nsclient::nsclient_exception);
  EXPECT_DOUBLE_EQ(buf.get_average(23 * 60 * 60).value, 1.0);
}
