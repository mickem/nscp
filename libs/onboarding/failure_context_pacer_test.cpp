// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <onboarding/failure_context_pacer.hpp>

namespace {
typedef onboarding::failure_context_pacer pacer;

// A pacer for a server that takes reports, one an hour.
pacer accepting(const unsigned long interval = 3600) {
  pacer p;
  p.configure(interval);
  p.start();
  p.begin_turn();
  p.server_accepts(0);
  return p;
}
}  // namespace

TEST(failure_context_header, accepts_with_and_without_a_minimum_interval) {
  ASSERT_TRUE(onboarding::parse_failure_context_header("accept"));
  EXPECT_EQ(0ul, onboarding::parse_failure_context_header("accept").value());
  EXPECT_EQ(3600ul, onboarding::parse_failure_context_header("accept; min-interval=3600").value());
  EXPECT_EQ(120ul, onboarding::parse_failure_context_header(" Accept ;Min-Interval = 120 ").value());
  // An unknown parameter is skipped, an unreadable interval ignored.
  EXPECT_EQ(60ul, onboarding::parse_failure_context_header("accept; v=2; min-interval=60").value());
  EXPECT_EQ(0ul, onboarding::parse_failure_context_header("accept; min-interval=soon").value());
}

TEST(failure_context_header, anything_else_is_not_accepting) {
  EXPECT_FALSE(onboarding::parse_failure_context_header(""));
  EXPECT_FALSE(onboarding::parse_failure_context_header("none"));
  EXPECT_FALSE(onboarding::parse_failure_context_header("accepted"));
  EXPECT_FALSE(onboarding::parse_failure_context_header("reject; min-interval=60"));
}

TEST(failure_context_pacer, nothing_goes_until_the_server_asks) {
  pacer p;
  p.start();
  p.begin_turn();
  EXPECT_FALSE(p.due(1, 1000));
  p.server_accepts(0);
  EXPECT_TRUE(p.due(1, 1000));
  // A later poll without the header is back to nothing.
  p.server_silent();
  EXPECT_FALSE(p.due(1, 1000));
}

TEST(failure_context_pacer, nothing_goes_without_a_new_failure) {
  pacer p = accepting();
  EXPECT_FALSE(p.due(0, 1000));
  p.sent(3, 1000);
  EXPECT_FALSE(p.due(3, 1000 + 7200));
  EXPECT_TRUE(p.due(4, 1000 + 7200));
}

TEST(failure_context_pacer, one_report_per_interval_whatever_fails) {
  pacer p = accepting();
  ASSERT_TRUE(p.due(1, 1000));
  p.sent(1, 1000);
  // check_drive_size failing ten minutes after check_cpu: same window.
  EXPECT_FALSE(p.due(2, 1600));
  EXPECT_FALSE(p.due(5, 1000 + 3599));
  EXPECT_TRUE(p.due(5, 1000 + 3600));
  EXPECT_EQ(1000 + 3600, p.next_at());
}

TEST(failure_context_pacer, the_interval_has_a_floor_and_the_server_can_raise_it) {
  pacer p = accepting(1);
  EXPECT_EQ(pacer::min_interval_seconds, p.interval());
  p.configure(3600);
  p.server_accepts(7200);
  EXPECT_EQ(7200ul, p.interval());
  p.sent(1, 1000);
  EXPECT_FALSE(p.due(2, 1000 + 3600));
  EXPECT_TRUE(p.due(2, 1000 + 7200));
  // A server minimum below the operator's interval changes nothing.
  p.server_accepts(60);
  EXPECT_EQ(3600ul, p.interval());
}

TEST(failure_context_pacer, the_last_report_survives_a_restart) {
  pacer p;
  p.configure(3600);
  p.restore(1000, 1200);
  p.start();
  p.begin_turn();
  p.server_accepts(0);
  // A crash-looping agent must not send a report per start.
  EXPECT_FALSE(p.due(1, 1200));
  EXPECT_TRUE(p.due(1, 1000 + 3600));
}

TEST(failure_context_pacer, a_stored_time_from_the_future_is_ignored) {
  pacer p;
  p.configure(3600);
  // A clock that stepped back a day: waiting for that time would silence
  // the host for a day.
  p.restore(1000 + 86400, 1000);
  EXPECT_EQ(0, p.last_sent_at());
  // Within one interval it is taken as it is.
  p.restore(1000 + 600, 1000);
  EXPECT_EQ(1000 + 600, p.last_sent_at());
}

TEST(failure_context_pacer, trouble_skips_this_turn_and_a_rate_limit_the_next) {
  pacer p = accepting();
  p.trouble_this_turn();
  EXPECT_FALSE(p.due(1, 1000));
  p.quiet_next_turn();
  p.begin_turn();
  EXPECT_FALSE(p.due(1, 1000));
  p.begin_turn();
  EXPECT_TRUE(p.due(1, 1000));
}

TEST(failure_context_pacer, a_retry_after_holds_every_report) {
  pacer p = accepting();
  p.hold(1000, 300);
  EXPECT_FALSE(p.due(1, 1299));
  EXPECT_TRUE(p.due(1, 1300));
  // A shorter hold does not cut a longer one short.
  p.hold(1000, 600);
  p.hold(1000, 10);
  EXPECT_FALSE(p.due(1, 1599));
}

TEST(failure_context_pacer, a_rejection_backs_off_doubling_to_an_hour) {
  pacer p = accepting();
  p.rejected(1000);
  EXPECT_FALSE(p.due(1, 1059));
  EXPECT_TRUE(p.due(1, 1060));
  p.rejected(1060);
  EXPECT_EQ(1060 + 120, p.next_at());
  for (int i = 0; i < 10; ++i) p.rejected(2000);
  EXPECT_EQ(2000 + 3600, p.next_at());
  // A report that lands clears it.
  p.sent(1, 6000);
  EXPECT_EQ(6000 + 3600, p.next_at());
}

TEST(failure_context_pacer, a_rejection_never_beats_the_interval) {
  pacer p = accepting();
  p.sent(1, 1000);
  p.rejected(4000);
  EXPECT_FALSE(p.due(2, 4590));
  EXPECT_TRUE(p.due(2, 4600));
}

TEST(failure_context_pacer, a_too_large_report_is_dropped_not_retried) {
  pacer p = accepting();
  p.dropped(4);
  EXPECT_FALSE(p.due(4, 1000));
  // The next failure is a candidate at once: nothing was sent, so no
  // interval started.
  EXPECT_TRUE(p.due(5, 1000));
  EXPECT_EQ(4ull, p.last_index());
  EXPECT_EQ(0, p.last_sent_at());
}
