// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The latched metrics snapshot the REST and OpenMetrics endpoints read.
//
// The daemon's metrics thread writes it once a second and any number of HTTP
// workers read it; the three representations (the JSON blob, the flat list and
// the OpenMetrics lines) are independent latches, and confusing them serves a
// scrape the wrong body. Nothing covered that separation, nor the empty state
// a scrape gets before the first write.

#include "metrics_handler.hpp"

#include <gtest/gtest.h>

#include <list>
#include <string>

TEST(MetricsHandler, ReadsAreEmptyBeforeTheFirstWrite) {
  // A scrape that arrives before the metrics thread has run must get an empty
  // body, not uninitialised memory or a fault.
  metrics_handler handler;
  EXPECT_EQ(handler.get(), "");
  EXPECT_EQ(handler.get_list(), "");
  EXPECT_TRUE(handler.get_openmetrics().empty());
}

TEST(MetricsHandler, TheJsonBlobRoundTrips) {
  metrics_handler handler;
  handler.set("{\"cpu\":42}");
  EXPECT_EQ(handler.get(), "{\"cpu\":42}");
}

TEST(MetricsHandler, TheFlatListRoundTrips) {
  metrics_handler handler;
  handler.set_list("cpu\nmem\n");
  EXPECT_EQ(handler.get_list(), "cpu\nmem\n");
}

TEST(MetricsHandler, TheOpenmetricsLinesRoundTrip) {
  metrics_handler handler;
  std::list<std::string> lines = {"# TYPE nscp_cpu gauge", "nscp_cpu 42"};
  handler.set_openmetrics(lines);

  const std::list<std::string> read_back = handler.get_openmetrics();
  ASSERT_EQ(read_back.size(), 2u);
  EXPECT_EQ(read_back.front(), "# TYPE nscp_cpu gauge");
  EXPECT_EQ(read_back.back(), "nscp_cpu 42");
}

TEST(MetricsHandler, TheThreeRepresentationsAreIndependent) {
  // /metrics, /api/v2/metrics and the OpenMetrics scrape each latch their own
  // rendering; writing one must not disturb the others.
  metrics_handler handler;
  handler.set("{\"cpu\":42}");
  handler.set_list("cpu");
  std::list<std::string> lines = {"nscp_cpu 42"};
  handler.set_openmetrics(lines);

  handler.set("{\"cpu\":43}");

  EXPECT_EQ(handler.get(), "{\"cpu\":43}");
  EXPECT_EQ(handler.get_list(), "cpu");
  ASSERT_EQ(handler.get_openmetrics().size(), 1u);
  EXPECT_EQ(handler.get_openmetrics().front(), "nscp_cpu 42");
}

TEST(MetricsHandler, EachWriteReplacesTheWholeSnapshot) {
  // It is a snapshot, not an append log: a shrinking metric set must not leave
  // last second's readings visible.
  metrics_handler handler;
  std::list<std::string> first = {"a 1", "b 2", "c 3"};
  handler.set_openmetrics(first);
  std::list<std::string> second = {"a 9"};
  handler.set_openmetrics(second);

  const std::list<std::string> read_back = handler.get_openmetrics();
  ASSERT_EQ(read_back.size(), 1u);
  EXPECT_EQ(read_back.front(), "a 9");
}

TEST(MetricsHandler, AnEmptyWriteClearsTheSnapshot) {
  metrics_handler handler;
  handler.set("{\"cpu\":42}");
  handler.set("");
  EXPECT_EQ(handler.get(), "");

  std::list<std::string> lines = {"a 1"};
  handler.set_openmetrics(lines);
  std::list<std::string> none;
  handler.set_openmetrics(none);
  EXPECT_TRUE(handler.get_openmetrics().empty());
}

TEST(MetricsHandler, ReadsHandBackACopyRatherThanAReference) {
  // The caller renders the list into a response outside the lock; if it were
  // handed the live container the next metrics tick would mutate it mid-write.
  metrics_handler handler;
  std::list<std::string> lines = {"a 1"};
  handler.set_openmetrics(lines);

  std::list<std::string> taken = handler.get_openmetrics();
  taken.push_back("b 2");

  EXPECT_EQ(handler.get_openmetrics().size(), 1u);
}

TEST(MetricsHandler, TheSourceListIsNotStolenFromTheCaller) {
  // set_openmetrics() takes a non-const reference; it must copy rather than
  // move, or the caller's own list is emptied behind its back.
  metrics_handler handler;
  std::list<std::string> lines = {"a 1", "b 2"};
  handler.set_openmetrics(lines);

  EXPECT_EQ(lines.size(), 2u);
}
