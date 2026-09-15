// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The latched metrics snapshot the REST and OpenMetrics endpoints read.
//
// The daemon's metrics thread writes it once a second and any number of HTTP
// workers read it; the three representations (the JSON blob, the flat list and
// the OpenMetrics exposition) are independent latches, and confusing them
// serves a scrape the wrong body. Nothing covered that separation, nor the
// empty state a scrape gets before the first write.

#include "metrics_handler.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(MetricsHandler, ReadsAreEmptyBeforeTheFirstWrite) {
  // A scrape that arrives before the metrics thread has run must get an empty
  // body, not uninitialised memory or a fault.
  metrics_handler handler;
  EXPECT_EQ(handler.get(), "");
  EXPECT_EQ(handler.get_list(), "");
  EXPECT_EQ(handler.get_openmetrics(), "");
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

TEST(MetricsHandler, TheOpenmetricsExpositionRoundTrips) {
  // One body, not a list of lines: `# TYPE` governs the samples that follow it
  // and the document has to end with `# EOF`, so the renderer owns the whole
  // thing and the latch must hand it back unchanged - the trailing newline
  // included, since a body that loses it is not a valid exposition.
  metrics_handler handler;
  handler.set_openmetrics("# TYPE nscp_cpu gauge\nnscp_cpu 42\n# EOF\n");

  EXPECT_EQ(handler.get_openmetrics(), "# TYPE nscp_cpu gauge\nnscp_cpu 42\n# EOF\n");
}

TEST(MetricsHandler, TheThreeRepresentationsAreIndependent) {
  // /metrics, /api/v2/metrics and the OpenMetrics scrape each latch their own
  // rendering; writing one must not disturb the others.
  metrics_handler handler;
  handler.set("{\"cpu\":42}");
  handler.set_list("cpu");
  handler.set_openmetrics("nscp_cpu 42\n# EOF\n");

  handler.set("{\"cpu\":43}");

  EXPECT_EQ(handler.get(), "{\"cpu\":43}");
  EXPECT_EQ(handler.get_list(), "cpu");
  EXPECT_EQ(handler.get_openmetrics(), "nscp_cpu 42\n# EOF\n");
}

TEST(MetricsHandler, EachWriteReplacesTheWholeSnapshot) {
  // It is a snapshot, not an append log: a shrinking metric set must not leave
  // last second's readings visible.
  metrics_handler handler;
  handler.set_openmetrics("a 1\nb 2\nc 3\n# EOF\n");
  handler.set_openmetrics("a 9\n# EOF\n");

  EXPECT_EQ(handler.get_openmetrics(), "a 9\n# EOF\n");
}

TEST(MetricsHandler, AnEmptyWriteClearsTheSnapshot) {
  metrics_handler handler;
  handler.set("{\"cpu\":42}");
  handler.set("");
  EXPECT_EQ(handler.get(), "");

  handler.set_openmetrics("a 1\n# EOF\n");
  handler.set_openmetrics("");
  EXPECT_EQ(handler.get_openmetrics(), "");
}

TEST(MetricsHandler, ReadsHandBackACopyRatherThanAReference) {
  // The caller writes the body into a response outside the lock; if it were
  // handed the live string the next metrics tick would mutate it mid-write.
  metrics_handler handler;
  handler.set_openmetrics("a 1\n# EOF\n");

  std::string taken = handler.get_openmetrics();
  taken += "b 2\n";

  EXPECT_EQ(handler.get_openmetrics(), "a 1\n# EOF\n");
}
