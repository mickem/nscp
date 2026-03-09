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

#include <gtest/gtest.h>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/filter.hpp>
#include <parsers/filter/realtime_helper.hpp>

// Provide the NSCAPI singleton so linked code can resolve the symbol.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// ============================================================================
// Minimal stub types to instantiate realtime_filter_helper<>
// ============================================================================

struct stub_filter_type {
  // Minimal interface needed by container::build_filters / process_item.
  // Not called in the timer/accessor tests.
  bool build_syntax(bool, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &) {
    return true;
  }
  bool build_engines(bool, const char *, const std::string &, const std::string &, const std::string &) { return true; }
  bool validate(std::string &) { return true; }
};

struct stub_runtime_data {
  typedef stub_filter_type filter_type;
  typedef int transient_data_type;

  bool booted = false;
  boost::posix_time::ptime last_touch;

  void boot() { booted = true; }
  void touch(boost::posix_time::ptime now) { last_touch = now; }
  bool has_changed(transient_data_type) const { return true; }
  modern_filter::match_result process_item(filter_type &, transient_data_type) { return modern_filter::match_result(true, false); }
};

struct stub_config_object {
  std::string alias_;
  nscapi::settings_filters::filter_object filter;

  stub_config_object(const std::string &alias) : alias_(alias), filter("${list}", "${detail}", "NSCA") {}

  std::string get_alias() const { return alias_; }
};

// Convenient type aliases for the template instantiation under test.
using helper_type = parsers::where::realtime_filter_helper<stub_runtime_data, stub_config_object>;
using container_type = helper_type::container;

// Helper: create a fixed ptime for deterministic tests.
static boost::posix_time::ptime make_time(int hour, int min, int sec) {
  return boost::posix_time::ptime(boost::gregorian::date(2026, 3, 9), boost::posix_time::time_duration(hour, min, sec));
}

// ============================================================================
// container — construction & accessor tests
// ============================================================================

TEST(RealtimeContainer, DefaultConstruction) {
  stub_runtime_data rd;
  container_type c("my_alias", "my_event", rd);

  EXPECT_EQ(c.get_alias(), "my_alias");
  EXPECT_EQ(c.get_event_name(), "my_event");
  EXPECT_EQ(c.command, "my_alias");
  EXPECT_EQ(c.severity, 0);
  EXPECT_FALSE(c.debug);
  EXPECT_FALSE(c.escape_html);
}

TEST(RealtimeContainer, SetAndGetTarget) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  c.set_target("my_target", "tid", "sid");
  EXPECT_EQ(c.get_target(), "my_target");
  EXPECT_EQ(c.get_target_id(), "tid");
  EXPECT_EQ(c.get_source_id(), "sid");
}

TEST(RealtimeContainer, IsEventAndIsEvents) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  c.set_target("event", "", "");
  EXPECT_TRUE(c.is_event());
  EXPECT_FALSE(c.is_events());

  c.set_target("events", "", "");
  EXPECT_FALSE(c.is_event());
  EXPECT_TRUE(c.is_events());

  c.set_target("other", "", "");
  EXPECT_FALSE(c.is_event());
  EXPECT_FALSE(c.is_events());
}

// ============================================================================
// container — is_silent() tests
// ============================================================================

TEST(RealtimeContainer, IsSilentWithoutSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No silent_period set — should never be silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 0)));
}

TEST(RealtimeContainer, IsSilentBeforeAlertTime) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(60);

  // Touch with alert=true at 12:00:00 — next_alert_ = 12:01:00
  c.touch(make_time(12, 0, 0), true);

  // Before 12:01:00 — should be silent.
  EXPECT_TRUE(c.is_silent(make_time(12, 0, 30)));
  // At exactly 12:01:00 — not silent (now < next_alert_ is false).
  EXPECT_FALSE(c.is_silent(make_time(12, 1, 0)));
  // After 12:01:00 — not silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 1, 1)));
}

TEST(RealtimeContainer, IsSilentAfterNonAlertTouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(60);

  // Touch with alert=false at 12:00:00 — next_alert_ = 12:00:00 (now)
  c.touch(make_time(12, 0, 0), false);

  // At 12:00:00 or later — not silent.
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 0)));
  EXPECT_FALSE(c.is_silent(make_time(12, 0, 1)));
}

// ============================================================================
// container — touch() tests
// ============================================================================

TEST(RealtimeContainer, TouchSetsNextOkWithMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);

  c.touch(make_time(10, 0, 0), false);

  // After touch, next_ok_ = 10:00:00 + 120s = 10:02:00
  // Verify via has_timedout:
  EXPECT_FALSE(c.has_timedout(make_time(10, 1, 59)));
  EXPECT_TRUE(c.has_timedout(make_time(10, 2, 0)));
}

TEST(RealtimeContainer, TouchWithAlertSetsSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);

  // next_alert_ = 10:00:30
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 30)));
}

TEST(RealtimeContainer, TouchWithoutAlertResetsSilentToNow) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  // First, trigger an alert to set next_alert_ into the future.
  c.touch(make_time(10, 0, 0), true);
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));

  // Now touch without alert — next_alert_ resets to now (10:00:20).
  c.touch(make_time(10, 0, 20), false);
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 20)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 25)));
}

TEST(RealtimeContainer, TouchDelegatesDataTouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);

  auto t = make_time(14, 30, 0);
  c.touch(t, false);

  EXPECT_EQ(c.data.last_touch, t);
}

TEST(RealtimeContainer, TouchWithoutMaxAgeDoesNotSetTimeout) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // max_age not set

  c.touch(make_time(10, 0, 0), false);

  // has_timedout should always be false without max_age
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 0)));
  EXPECT_FALSE(c.has_timedout(make_time(23, 59, 59)));
}

TEST(RealtimeContainer, TouchWithoutSilentPeriodDoesNotAffectSilence) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // silent_period not set

  c.touch(make_time(10, 0, 0), true);
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 0)));
}

// ============================================================================
// container — has_timedout() tests
// ============================================================================

TEST(RealtimeContainer, HasTimedoutWithoutMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No max_age — never times out.
  EXPECT_FALSE(c.has_timedout(make_time(0, 0, 0)));
  EXPECT_FALSE(c.has_timedout(make_time(23, 59, 59)));
}

TEST(RealtimeContainer, HasTimedoutBeforeExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  // next_ok_ = 12:01:00
  EXPECT_FALSE(c.has_timedout(make_time(12, 0, 30)));
  EXPECT_FALSE(c.has_timedout(make_time(12, 0, 59)));
}

TEST(RealtimeContainer, HasTimedoutAtExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  // Exactly at expiry: next_ok_ <= now
  EXPECT_TRUE(c.has_timedout(make_time(12, 1, 0)));
}

TEST(RealtimeContainer, HasTimedoutAfterExpiry) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);

  EXPECT_TRUE(c.has_timedout(make_time(12, 5, 0)));
}

TEST(RealtimeContainer, HasTimedoutResetsAfterRetouch) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);

  c.touch(make_time(12, 0, 0), false);
  EXPECT_TRUE(c.has_timedout(make_time(12, 1, 0)));

  // Re-touch at 12:01:00 — next_ok_ = 12:02:00
  c.touch(make_time(12, 1, 0), false);
  EXPECT_FALSE(c.has_timedout(make_time(12, 1, 30)));
  EXPECT_TRUE(c.has_timedout(make_time(12, 2, 0)));
}

// ============================================================================
// container — find_minimum_timeout() tests
// ============================================================================

TEST(RealtimeContainer, FindMinimumTimeoutWithoutMaxAge) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  // No max_age

  boost::optional<boost::posix_time::ptime> minNext;
  EXPECT_FALSE(c.find_minimum_timeout(minNext));
  EXPECT_FALSE(minNext.is_initialized());
}

TEST(RealtimeContainer, FindMinimumTimeoutSetsFirstValue) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);
  c.touch(make_time(10, 0, 0), false);

  boost::optional<boost::posix_time::ptime> minNext;
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  ASSERT_TRUE(minNext.is_initialized());
  EXPECT_EQ(*minNext, make_time(10, 1, 0));
}

TEST(RealtimeContainer, FindMinimumTimeoutKeepsSmallerExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to an earlier time — container's next_ok_ (10:02:00)
  // is later, so should not update and return false.
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 0, 30);
  EXPECT_FALSE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(*minNext, make_time(10, 0, 30));
}

TEST(RealtimeContainer, FindMinimumTimeoutUpdatesLargerExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(30);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to a later time — container's next_ok_ (10:00:30)
  // is earlier, so should update.
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 5, 0);
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(*minNext, make_time(10, 0, 30));
}

TEST(RealtimeContainer, FindMinimumTimeoutUpdatesEqualExisting) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(60);
  c.touch(make_time(10, 0, 0), false);

  // Pre-set minNext to equal value — should still update (returns true).
  boost::optional<boost::posix_time::ptime> minNext = make_time(10, 1, 0);
  EXPECT_TRUE(c.find_minimum_timeout(minNext));
  EXPECT_EQ(*minNext, make_time(10, 1, 0));
}

// ============================================================================
// container — combined timer scenarios
// ============================================================================

TEST(RealtimeContainer, SilentPeriodAndMaxAgeIndependent) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.max_age = boost::posix_time::seconds(120);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);

  // At 10:00:15: silent but not timed out
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 15)));
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 15)));

  // At 10:00:45: not silent, not timed out
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 45)));
  EXPECT_FALSE(c.has_timedout(make_time(10, 0, 45)));

  // At 10:02:00: not silent, timed out
  EXPECT_FALSE(c.is_silent(make_time(10, 2, 0)));
  EXPECT_TRUE(c.has_timedout(make_time(10, 2, 0)));
}

TEST(RealtimeContainer, MultipleAlertTouchesExtendSilentPeriod) {
  stub_runtime_data rd;
  container_type c("a", "e", rd);
  c.silent_period = boost::posix_time::seconds(30);

  c.touch(make_time(10, 0, 0), true);
  // next_alert_ = 10:00:30

  // Touch again with alert at 10:00:20 — next_alert_ = 10:00:50
  c.touch(make_time(10, 0, 20), true);
  EXPECT_TRUE(c.is_silent(make_time(10, 0, 40)));
  EXPECT_FALSE(c.is_silent(make_time(10, 0, 50)));
}

// ============================================================================
// realtime_filter_helper — items list management
// ============================================================================

TEST(RealtimeFilterHelper, InitiallyEmpty) {
  helper_type helper(nullptr, 0);
  EXPECT_TRUE(helper.items.empty());
}

