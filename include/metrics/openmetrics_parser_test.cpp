// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The exposition parser CheckOpenMetrics scrapes exporters with.
//
// The body comes off the network from a process the agent does not control,
// so beyond the grammar these pin the two promises the scraper is built on:
// the parser stops at the first line it cannot read and says which, and no
// input - a truncated body, one enormous line, a hundred thousand series, a
// million metadata lines - costs more than a single linear pass over it or
// keeps more than the caller's limits allow.
//
// Every test names the format it parses: the two formats disagree about family
// names, `# EOF`, timestamps and which types exist, and a case that holds in
// one is often wrong in the other.
//
// The round trip against the agent's own renderer lives beside the renderer,
// in modules/WEBServer/openmetrics_roundtrip_test.cpp.

#include <gtest/gtest.h>

#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <map>
#include <metrics/openmetrics_parser.hpp>
#include <ostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace om = metrics::openmetrics;

namespace {

const om::format openmetrics = om::format::openmetrics_1_0;
const om::format text = om::format::prometheus_text_0_0_4;

// The single family a body was expected to produce. A failed count is reported
// and an empty family returned, so the test carries on to report what else is
// wrong instead of reading past the end of the vector.
const om::family &only_family(const om::result &parsed) {
  static const om::family none;
  if (parsed.families.size() != 1) {
    ADD_FAILURE() << "expected exactly one family, got " << parsed.families.size();
    return none;
  }
  return parsed.families.front();
}

const om::family &family_named(const om::result &parsed, const std::string &name) {
  static const om::family none;
  for (const om::family &f : parsed.families) {
    if (f.name == name) return f;
  }
  ADD_FAILURE() << "no family '" << name << "'";
  return none;
}

// No limits at all, for the tests about what the parser does when it has to
// read everything.
om::limits unlimited() {
  om::limits none;
  none.max_series = 0;
  none.max_families = 0;
  none.max_labels_per_sample = 0;
  none.max_labels = 0;
  none.max_line_bytes = 0;
  return none;
}

const om::family &family_typed(const om::result &parsed, const std::string &name, const om::family_type type) {
  static const om::family none;
  for (const om::family &f : parsed.families) {
    if (f.name == name && f.type == type) return f;
  }
  ADD_FAILURE() << "no " << om::type_name(type) << " family '" << name << "'";
  return none;
}

om::label_list labels(std::initializer_list<std::pair<std::string, std::string> > items) { return om::label_list(items); }

// What every result promises whatever the input; defined with the hostile-input
// tests below.
void expect_consistent(const om::result &parsed, const std::string &body, om::format f);

// A failed parse stops at the line it reports: it holds no sample from that
// line or after it - no more than the lines before the error give. (It may
// hold fewer families: one whose own metadata line failed is taken out.)
void expect_stops_at_error(const om::result &parsed, const std::string &body, const om::format f) {
  ASSERT_FALSE(parsed.ok()) << body;
  std::size_t start = 0;
  for (std::size_t line = 1; line < parsed.error_line && start != std::string::npos; ++line) {
    const std::size_t end = body.find('\n', start);
    start = end == std::string::npos ? std::string::npos : end + 1;
  }
  const om::result before = om::parse(start == std::string::npos ? body : body.substr(0, start), f, om::limits());
  EXPECT_EQ(parsed.sample_count, before.sample_count) << body << " -> " << parsed.error;
  EXPECT_LE(parsed.families.size(), before.families.size()) << body << " -> " << parsed.error;
}

}  // namespace

// --- choosing the format ------------------------------------------------------

TEST(OpenmetricsParser, FormatFollowsTheContentType) {
  EXPECT_EQ(om::format_for_content_type("application/openmetrics-text; version=1.0.0; charset=utf-8"), openmetrics);
  EXPECT_EQ(om::format_for_content_type("Application/OpenMetrics-Text"), openmetrics);
  EXPECT_EQ(om::format_for_content_type("text/plain; version=0.0.4; charset=utf-8"), text);
  EXPECT_EQ(om::format_for_content_type(""), text);
  EXPECT_EQ(om::format_for_content_type("application/json"), text);
}

// --- families and metadata ----------------------------------------------------

TEST(OpenmetricsParser, ReadsAGaugeWithItsMetadata) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# HELP node_memory_free_bytes Free memory.\n"
        "# TYPE node_memory_free_bytes gauge\n"
        "# UNIT node_memory_free_bytes bytes\n"
        "node_memory_free_bytes 1.6554e+10\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    EXPECT_FALSE(parsed.saw_eof);
    EXPECT_EQ(parsed.sample_count, 1u);
    const om::family &g = only_family(parsed);
    EXPECT_EQ(g.name, "node_memory_free_bytes");
    EXPECT_EQ(g.type, om::family_type::gauge);
    EXPECT_EQ(g.help, "Free memory.");
    EXPECT_EQ(g.unit, "bytes");
    ASSERT_EQ(g.samples.size(), 1u);
    EXPECT_EQ(g.samples.at(0).name, "node_memory_free_bytes");
    EXPECT_DOUBLE_EQ(g.samples.at(0).value, 16554000000.0);
    EXPECT_FALSE(g.samples.at(0).timestamp.has_value());
  }
}

TEST(OpenmetricsParser, OpenMetricsCounterIsNamedWithoutItsSuffix) {
  // OpenMetrics names the family and puts `_total` on the sample.
  const om::result parsed = om::parse(
      "# TYPE http_requests counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_created{code=\"200\"} 1.7e9\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests");
  EXPECT_EQ(f.type, om::family_type::counter);
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples.at(0).name, "http_requests_total");
  EXPECT_EQ(f.samples.at(1).name, "http_requests_created");
}

TEST(OpenmetricsParser, PrometheusTextCounterIsNamedAsDeclared) {
  // The older format names the sample in every metadata line, so that is the
  // family name too. Nothing is renamed.
  const om::result parsed = om::parse(
      "# HELP http_requests_total Requests served.\n"
      "# TYPE http_requests_total counter\n"
      "http_requests_total{code=\"200\"} 1027\n"
      "http_requests_total{code=\"500\"} 3\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.name, "http_requests_total");
  EXPECT_EQ(f.type, om::family_type::counter);
  EXPECT_EQ(f.help, "Requests served.");
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples.at(1).labels, labels({{"code", "500"}}));
}

TEST(OpenmetricsParser, PrometheusTextGaugeAndTotalCounterAreTwoFamilies) {
  // What client_golang serves by default on every Go exporter. The two used to
  // collide and stop the parse, losing every family sorted after them.
  const std::string gauge_first =
      "# HELP go_memstats_alloc_bytes Bytes allocated and still in use.\n"
      "# TYPE go_memstats_alloc_bytes gauge\n"
      "go_memstats_alloc_bytes 1.2e+06\n"
      "# HELP go_memstats_alloc_bytes_total Total bytes allocated.\n"
      "# TYPE go_memstats_alloc_bytes_total counter\n"
      "go_memstats_alloc_bytes_total 9.8e+07\n"
      "# TYPE node_load1 gauge\n"
      "node_load1 0.5\n";
  const std::string counter_first =
      "# TYPE go_memstats_alloc_bytes_total counter\n"
      "go_memstats_alloc_bytes_total 9.8e+07\n"
      "# TYPE go_memstats_alloc_bytes gauge\n"
      "go_memstats_alloc_bytes 1.2e+06\n"
      "# TYPE node_load1 gauge\n"
      "node_load1 0.5\n";
  for (const std::string &body : {gauge_first, counter_first}) {
    const om::result parsed = om::parse(body, text);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 3u);
    EXPECT_EQ(family_named(parsed, "go_memstats_alloc_bytes").type, om::family_type::gauge);
    EXPECT_EQ(family_named(parsed, "go_memstats_alloc_bytes_total").type, om::family_type::counter);
    EXPECT_DOUBLE_EQ(family_named(parsed, "node_load1").samples.at(0).value, 0.5);
  }
}

// What client_golang serves when OpenMetrics is negotiated: it writes a counter
// `X_total` as the family `X`, next to the gauge `X` every Go exporter has.
const char *const client_golang_openmetrics =
    "# HELP go_memstats_alloc_bytes Number of bytes allocated in heap and currently in use.\n"
    "# TYPE go_memstats_alloc_bytes gauge\n"
    "go_memstats_alloc_bytes 1.913168e+06\n"
    "# HELP go_memstats_alloc_bytes Total number of bytes allocated in heap until now, even if released already.\n"
    "# TYPE go_memstats_alloc_bytes counter\n"
    "go_memstats_alloc_bytes_total 1.913168e+06\n"
    "go_memstats_alloc_bytes_created 1.7e+09\n"
    "# HELP go_threads Number of OS threads created.\n"
    "# TYPE go_threads gauge\n"
    "go_threads 7\n"
    "# HELP process_cpu_seconds Total user and system CPU time spent in seconds.\n"
    "# TYPE process_cpu_seconds counter\n"
    "# UNIT process_cpu_seconds seconds\n"
    "process_cpu_seconds_total 0.04\n"
    "# EOF\n";

TEST(OpenmetricsParser, ClientGolangOpenMetricsGaugeAndCounterOfOneNameAreBothKept) {
  const om::result parsed = om::parse(client_golang_openmetrics, openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error << " on line " << parsed.error_line;
  expect_consistent(parsed, client_golang_openmetrics, openmetrics);
  EXPECT_TRUE(parsed.saw_eof);
  ASSERT_EQ(parsed.families.size(), 4u);
  // Both keep the name they were declared under; the type tells them apart.
  const om::family &gauge = family_typed(parsed, "go_memstats_alloc_bytes", om::family_type::gauge);
  EXPECT_EQ(gauge.help, "Number of bytes allocated in heap and currently in use.");
  ASSERT_EQ(gauge.samples.size(), 1u);
  const om::family &counter = family_typed(parsed, "go_memstats_alloc_bytes", om::family_type::counter);
  EXPECT_EQ(counter.help, "Total number of bytes allocated in heap until now, even if released already.");
  ASSERT_EQ(counter.samples.size(), 2u);
  EXPECT_EQ(counter.samples.at(0).name, "go_memstats_alloc_bytes_total");
  EXPECT_EQ(counter.samples.at(1).name, "go_memstats_alloc_bytes_created");
  // Everything after the pair is still read, and a counter without a twin is
  // named the same way.
  EXPECT_DOUBLE_EQ(family_named(parsed, "go_threads").samples.at(0).value, 7);
  EXPECT_EQ(family_typed(parsed, "process_cpu_seconds", om::family_type::counter).unit, "seconds");
}

TEST(OpenmetricsParser, ClientGolangBodyCutAtAnyLineReadsAsTruncated) {
  // A body cut by a size cap or a dropped connection - including right after
  // the second `# HELP` of a pair, before its `# TYPE` - must look like
  // truncation (read, but no `# EOF`), never like a broken exporter.
  const std::string body = client_golang_openmetrics;
  for (std::size_t end = body.find('\n'); end != std::string::npos; end = body.find('\n', end + 1)) {
    const std::string prefix = body.substr(0, end + 1);
    const om::result parsed = om::parse(prefix, openmetrics);
    EXPECT_TRUE(parsed.ok()) << "cut after byte " << end << ": " << parsed.error;
    EXPECT_EQ(parsed.saw_eof, end + 1 == body.size()) << "cut after byte " << end;
  }
}

TEST(OpenmetricsParser, OpenMetricsCounterBesideAHistogramOrSummaryOfItsName) {
  // Both own the sample name `rpc_created`, which only one of them may carry
  // (the refusal is in the next test); here the counter has it.
  for (const char *other : {"histogram", "summary"}) {
    const std::string first = std::string(other) == "histogram" ? "rpc_bucket{le=\"+Inf\"} 4\n" : "rpc{quantile=\"0.5\"} 0.2\n";
    const std::string body = std::string("# TYPE rpc ") + other + "\n" + first +
                             "rpc_sum 1.5\n"
                             "rpc_count 4\n"
                             "# TYPE rpc counter\n"
                             "rpc_total 3\n"
                             "rpc_created 1.6e9\n"
                             "# TYPE after gauge\n"
                             "after 1\n"
                             "# EOF\n";
    const om::result parsed = om::parse(body, openmetrics);
    ASSERT_TRUE(parsed.ok()) << other << ": " << parsed.error;
    expect_consistent(parsed, body, openmetrics);
    ASSERT_EQ(parsed.families.size(), 3u) << other;
    EXPECT_EQ(parsed.families.at(0).samples.size(), 3u) << other;
    const om::family &counter = family_typed(parsed, "rpc", om::family_type::counter);
    ASSERT_EQ(counter.samples.size(), 2u) << other;
    EXPECT_EQ(counter.samples.at(1).name, "rpc_created") << other;
  }
}

TEST(OpenmetricsParser, OpenMetricsSameNamePairInEitherOrder) {
  const om::result parsed = om::parse(
      "# TYPE x counter\n"
      "x_total 5\n"
      "# TYPE x gauge\n"
      "x 1\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families.at(0).name, "x");
  EXPECT_EQ(parsed.families.at(0).type, om::family_type::counter);
  EXPECT_EQ(parsed.families.at(1).name, "x");
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::gauge);
}

TEST(OpenmetricsParser, SameNamePairThatCannotStandSideBySideIsRefused) {
  struct refused {
    const char *body;
    std::size_t line;
    const char *fragment;
    // What is left: the families read before the failing line, and no family
    // a refused repeated name began.
    std::size_t families;
  };
  const char *const declared = "already declared or sampled";
  const refused cases[] = {
      // Only a counter may share its name, and only with one non-counter.
      {"# TYPE x gauge\nx 1\n# TYPE x gauge\nx 2\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x histogram\nx_bucket{le=\"+Inf\"} 1\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x info\nx_info 1\n", 3, declared, 1},
      {"# TYPE x counter\nx_total 1\n# TYPE x counter\nx_total 2\n", 3, declared, 1},
      {"# TYPE x histogram\nx_count 1\n# TYPE x summary\nx_count 2\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\nx_total 1\n# TYPE x info\n", 5, declared, 2},
      // The earlier family has to have said what it is: a family named only by
      // its `# HELP` line, or only by its samples, is a declaration that is
      // being repeated.
      {"# HELP x first\n# TYPE y gauge\ny 1\n# TYPE x counter\nx_total 1\n", 4, declared, 2},
      {"x 2\n# TYPE x counter\nx_total 1\n", 2, declared, 1},
      // The later block has to declare its type and then carry a sample of
      // its own. A `# HELP`, or a `# TYPE` followed by anything else, is a late
      // line for the earlier family, and reported where it is.
      {"# TYPE x gauge\nx 1\n# HELP x again\nx 2\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# HELP x again\n# UNIT x bytes\n# TYPE y gauge\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\n# TYPE y gauge\ny 1\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\n# EOF\n", 3, declared, 1},
      // `# EOF` with text after it ends nothing, but it is not one of the
      // block's lines either: the same fault as a plain `# EOF`.
      {"# TYPE x gauge\nx 1\n# TYPE x counter\n# EOF junk\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\nx 2\n", 3, declared, 1},
      // Inside the block every line is held to the usual rules, and reported
      // where it is: the repeated name was fine as far as it went.
      {"# TYPE x gauge\nx 1\n# TYPE x counter\n# TYPE x counter\nx_total 1\n", 4, "second '# TYPE' line for 'x'", 1},
      {"# TYPE x gauge\nx 2\n# UNIT x s\n# UNIT x s\n", 4, "second '# UNIT' line for 'x'", 1},
      // A line that is not the block's own is reported as the repeated name,
      // the first line at fault, before whatever else is wrong with it - its
      // name, its value, a limit, its length.
      {"# TYPE x gauge\nx 2\n# HELP x again\n# HELP x.y z\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\ny abc\n", 3, declared, 1},
      {"# TYPE x gauge\nx 1\n# TYPE x counter\n# TYPE y sometimes\n", 3, declared, 1},
      // The type decides whether the block can pair at all, before anything
      // that type would claim: the same body with and without a `# HELP`.
      {"# TYPE x gauge\nx_info 6\n# TYPE x info\n", 3, declared, 2},
      {"# TYPE x gauge\nx_info 6\n# HELP x h1\n# TYPE x info\n", 3, declared, 2},
      {"# TYPE x histogram\nx_total 3\n# TYPE x counter\n", 3, "'x_total' came before the '# TYPE' line of 'x'", 2},
      // So is the end of the body: one cut mid-line is refused like any other.
      {"# TYPE x gauge\nx 1\n# HELP x again\nfoo 1", 4, "the body ends in the middle of a line", 1},
      // A block that had declared a type the earlier family pairs with keeps
      // that much, as any family keeps what it read before a failing line.
      {"# TYPE x gauge\nx 1\n# TYPE x counter\nx_to", 4, "the body ends in the middle of a line", 2},
      // The counter's sample name is already a family.
      {"# TYPE x_total gauge\nx_total 1\n# TYPE x gauge\nx 1\n# TYPE x counter\nx_total 2\n", 5, "'x_total' came before the '# TYPE' line of 'x'", 2},
      // A sample of the earlier family after the later one began.
      {"# TYPE x gauge\nx 1\n# TYPE x counter\nx_total 1\nx 2\n", 5, "apart from the rest of the family 'x'", 2},
  };
  for (const refused &c : cases) {
    const om::result parsed = om::parse(c.body, openmetrics);
    EXPECT_FALSE(parsed.ok()) << c.body;
    EXPECT_EQ(parsed.error_line, c.line) << c.body << " -> " << parsed.error;
    EXPECT_NE(parsed.error.find(c.fragment), std::string::npos) << c.body << " -> " << parsed.error;
    EXPECT_EQ(parsed.families.size(), c.families) << c.body;
    expect_consistent(parsed, c.body, openmetrics);
    expect_stops_at_error(parsed, c.body, openmetrics);
  }
  // The Prometheus text format names every family after its sample, so it
  // never writes a pair.
  const om::result text_pair = om::parse("# TYPE x gauge\nx 1\n# TYPE x counter\nx 2\n", text);
  EXPECT_FALSE(text_pair.ok());
  EXPECT_EQ(text_pair.error_line, 3u);
}

TEST(OpenmetricsParser, FinalLineFeedAfterEofChangesNothing) {
  // Plenty of exporters leave the line feed after `# EOF` off, and the reader
  // accepts that, so every body must read the same with and without it - the
  // repeated-name rule included, which once only looked at whole lines.
  const char *const cases[] = {
      "# TYPE x gauge\nx 1\n# TYPE x counter\n# EOF\n",
      "# TYPE x gauge\nx 1\n# HELP x again\n# EOF\n",
      "# TYPE x gauge\nx 1\n# TYPE x counter\r\n# EOF\r\n",
      "# TYPE x gauge\nx 1\n# HELP x again\r\n# EOF\r\n",
      "# TYPE x gauge\nx 1\n# TYPE x counter\nx_total 1\n# EOF\n",
      "# TYPE x gauge\nx 1\n  # EOF  \n",
  };
  const auto same = [](const std::string &whole) {
    const std::string cut = whole.substr(0, whole.size() - 1);
    const om::result a = om::parse(whole, openmetrics);
    const om::result b = om::parse(cut, openmetrics);
    EXPECT_EQ(a.ok(), b.ok()) << whole;
    EXPECT_EQ(a.error, b.error) << whole;
    EXPECT_EQ(a.error_line, b.error_line) << whole;
    EXPECT_EQ(a.saw_eof, b.saw_eof) << whole;
    ASSERT_EQ(a.families.size(), b.families.size()) << whole;
    for (std::size_t i = 0; i < a.families.size(); ++i) {
      const om::family &x = a.families.at(i);
      const om::family &y = b.families.at(i);
      EXPECT_EQ(x.name, y.name) << whole;
      EXPECT_EQ(x.type, y.type) << whole;
      EXPECT_EQ(x.help, y.help) << whole;
      EXPECT_EQ(x.unit, y.unit) << whole;
      EXPECT_EQ(x.samples.size(), y.samples.size()) << whole;
    }
    expect_consistent(b, cut, openmetrics);
  };
  for (const char *body : cases) same(body);
  // The block's first two cases, by their result.
  const om::result refused = om::parse("# TYPE x gauge\nx 1\n# TYPE x counter\n# EOF", openmetrics);
  EXPECT_FALSE(refused.ok());
  EXPECT_EQ(refused.error_line, 3u);
  EXPECT_EQ(refused.families.size(), 1u);

  // And any body built from the same pieces the fuzzer uses, ended by `# EOF`.
  const char *const fragments[] = {"# TYPE h histogram\n", "# TYPE h counter\n", "# TYPE h gauge\n",       "# HELP h x\n", "# UNIT h s\n", "h 1\n",
                                   "h_total 1\n",          "h_created 1\n",      "h_bucket{le=\"1\"} 1\n", "h_count 1\n",  "g 1\n",        "# EOF junk\n"};
  std::mt19937 random(1631);
  std::uniform_int_distribution<std::size_t> pick(0, sizeof(fragments) / sizeof(fragments[0]) - 1);
  std::uniform_int_distribution<std::size_t> length(0, 8);
  for (int round = 0; round < 20000; ++round) {
    std::string body;
    const std::size_t pieces = length(random);
    for (std::size_t i = 0; i < pieces; ++i) body += fragments[pick(random)];
    same(body + (random() % 2 == 0 ? "# EOF\n" : "# EOF\r\n"));
    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << "round " << round << " body: " << body;
      return;
    }
  }
}

TEST(OpenmetricsParser, BlankAndCommentLinesMayStandInsideAPairBlock) {
  // Neither belongs to another family, so neither decides the block.
  const std::string body =
      "# TYPE x gauge\nx 1\n"
      "# HELP x Total.\n"
      "\n"
      "# a comment between the metadata lines\n"
      "# TYPE x counter\n"
      "   \n"
      "#another comment\n"
      "x_total 1\n"
      "# EOF\n";
  const om::result parsed = om::parse(body, openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error << " on line " << parsed.error_line;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::counter);
  EXPECT_EQ(parsed.families.at(1).help, "Total.");
  EXPECT_EQ(parsed.families.at(1).samples.size(), 1u);
}

TEST(OpenmetricsParser, OverLongMetadataLineFailsItsFamilyLikeAnyOther) {
  // Every failing metadata line of a family with no samples yet takes that
  // family out - an over-long one too.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  for (const om::format f : {openmetrics, text}) {
    const om::result too_long = om::parse("# TYPE y counter\n# HELP y " + std::string(40, 'h') + "\n", f, bounds);
    EXPECT_EQ(too_long.error_line, 2u) << too_long.error;
    EXPECT_NE(too_long.error.find("longer than 20 bytes"), std::string::npos) << too_long.error;
    EXPECT_TRUE(too_long.families.empty());
    const om::result twice = om::parse("# TYPE y counter\n# HELP y a\n# HELP y b\n", f, bounds);
    EXPECT_EQ(twice.error_line, 3u) << twice.error;
    EXPECT_TRUE(twice.families.empty());
  }
}

TEST(OpenmetricsParser, OverLongMetadataLineFailsTheBlockOfARepeatedName) {
  // The typed block of a repeated name is a family being read like any other:
  // an over-long line of its own metadata takes it out, and leaves the earlier
  // family as it was.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  const om::result parsed = om::parse("# TYPE x gauge\nx 1\n# TYPE x counter\n# HELP x " + std::string(40, 'h') + "\n", openmetrics, bounds);
  EXPECT_EQ(parsed.error_line, 4u) << parsed.error;
  EXPECT_NE(parsed.error.find("longer than 20 bytes"), std::string::npos) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families.at(0).type, om::family_type::gauge);
  EXPECT_EQ(parsed.families.at(0).samples.size(), 1u);
}

TEST(OpenmetricsParser, OverLongLineThatIsNotTheFamilysOwnMetadataKeepsIt) {
  // Each part of "a metadata line of the family being read" on its own: a
  // sample of the family, metadata of another family, metadata of a name that
  // only starts with the family's, and a comment. None of them takes the
  // family out, in either format.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  const std::string long_text(40, 'v');
  for (const om::format f : {openmetrics, text}) {
    for (const std::string &tail : {"y{a=\"" + long_text + "\"} 1\n", "y " + long_text + "\n", "# HELP z " + long_text + "\n", "# HELP y.z " + long_text + "\n",
                                    "# HELP yy " + long_text + "\n", "# " + long_text + "\n"}) {
      const om::result parsed = om::parse("# TYPE y gauge\n" + tail, f, bounds);
      EXPECT_EQ(parsed.error_line, 2u) << tail << " -> " << parsed.error;
      EXPECT_NE(parsed.error.find("longer than 20 bytes"), std::string::npos) << tail << " -> " << parsed.error;
      ASSERT_EQ(parsed.families.size(), 1u) << tail;
      EXPECT_EQ(parsed.families.at(0).name, "y") << tail;
      EXPECT_EQ(parsed.families.at(0).type, om::family_type::gauge) << tail;
    }
  }
}

TEST(OpenmetricsParser, OverLongCommentInsideAPairBlockKeepsTheBlock) {
  // A comment belongs to no family, so an over-long one stops the parse with
  // the typed block kept as declared, as the body ending there would.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  const om::result parsed = om::parse("# TYPE x gauge\nx 1\n# TYPE x counter\n# " + std::string(40, 'c') + "\n", openmetrics, bounds);
  EXPECT_EQ(parsed.error_line, 4u) << parsed.error;
  EXPECT_NE(parsed.error.find("longer than 20 bytes"), std::string::npos) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::counter);
  EXPECT_TRUE(parsed.families.at(1).samples.empty());
}

TEST(OpenmetricsParser, NothingAfterEofTakesBackWhatTheDocumentDeclared) {
  // The document is complete at `# EOF`. Whatever a proxy appends - metadata
  // for the last family included, over-long or not - is an error, and the
  // families stay as the document declared them.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  for (const std::string &tail :
       {"# HELP y " + std::string(40, 'h') + "\n", std::string("# HELP y a\n"), std::string("# TYPE y gauge\n"), std::string("# HELP y a\n# HELP y b\n"),
        std::string("# UNIT y seconds and more\n"), "y_total " + std::string(40, '1') + "\n"}) {
    const om::result parsed = om::parse("# TYPE y counter\n# EOF\n" + tail, openmetrics, bounds);
    EXPECT_FALSE(parsed.ok()) << tail;
    EXPECT_EQ(parsed.error_line, 3u) << tail << " -> " << parsed.error;
    EXPECT_TRUE(parsed.saw_eof) << tail;
    ASSERT_EQ(parsed.families.size(), 1u) << tail << " -> " << parsed.error;
    EXPECT_EQ(parsed.families.at(0).name, "y") << tail;
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::counter) << tail;
    EXPECT_TRUE(parsed.families.at(0).help.empty()) << tail;
  }
}

TEST(OpenmetricsParser, LineLimitCountsTheLineAsServed) {
  // Carriage return and surrounding blanks included: the limit is on what
  // the exporter sent, not on what is left after trimming it.
  om::limits bounds;
  bounds.max_line_bytes = 10;
  const om::result fits = om::parse("  a 1    \r\n", text, bounds);
  EXPECT_TRUE(fits.ok()) << fits.error;
  const om::result padded = om::parse("    a 1     \r\n", text, bounds);
  EXPECT_FALSE(padded.ok());
  EXPECT_NE(padded.error.find("longer than 10 bytes"), std::string::npos) << padded.error;
  // Over by one byte only because of the carriage return: ten bytes of line,
  // eleven as served.
  const om::result crlf = om::parse("a 12345678\r\n", text, bounds);
  EXPECT_FALSE(crlf.ok());
  EXPECT_NE(crlf.error.find("longer than 10 bytes"), std::string::npos) << crlf.error;
  const om::result lf = om::parse("a 12345678\n", text, bounds);
  EXPECT_TRUE(lf.ok()) << lf.error;
  // A metadata line the same: at the limit before its carriage return.
  bounds.max_line_bytes = 19;
  for (const om::format f : {openmetrics, text}) {
    const om::result help_crlf = om::parse("# HELP x aaaaaaaaaa\r\n", f, bounds);
    EXPECT_NE(help_crlf.error.find("longer than 19 bytes"), std::string::npos) << help_crlf.error;
    const om::result help_lf = om::parse("# HELP x aaaaaaaaaa\n", f, bounds);
    EXPECT_EQ(help_lf.error.find("longer"), std::string::npos) << help_lf.error;
    ASSERT_EQ(help_lf.families.size(), 1u);
    EXPECT_EQ(help_lf.families.at(0).help, "aaaaaaaaaa");
  }
}

TEST(OpenmetricsParser, BodyEndingInsideTheSecondBlockOfAPairReadsAsTruncated) {
  // Cut on a line boundary before the block has declared its type, it cannot
  // be told apart from a late line for the first family, and is not kept. Cut
  // after its type, it is read as declared - an empty family, as any family
  // cut before its first sample is. The missing `# EOF` says it is cut.
  struct cut {
    const char *tail;
    std::size_t families;
  };
  const cut cuts[] = {
      {"# HELP x again\n", 1}, {"# HELP x again\n# UNIT x bytes\n", 1}, {"# HELP x again\n# TYPE x counter\n", 2}, {"# TYPE x counter\n# UNIT x bytes\n", 2}};
  for (const cut &c : cuts) {
    const std::string body = std::string("# TYPE x gauge\nx 1\n") + c.tail;
    const om::result parsed = om::parse(body, openmetrics);
    EXPECT_TRUE(parsed.ok()) << c.tail << " -> " << parsed.error;
    EXPECT_FALSE(parsed.saw_eof) << c.tail;
    ASSERT_EQ(parsed.families.size(), c.families) << c.tail;
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::gauge) << c.tail;
    expect_consistent(parsed, body, openmetrics);
  }
}

TEST(OpenmetricsParser, PairFamilyWithValidMetadataIsKeptWhenALaterLineFails) {
  // As any family is: what was read before the failing line stays.
  const om::result paired = om::parse("# TYPE x gauge\nx 1\n# HELP x c\n# TYPE x counter\nx_total abc\n", openmetrics);
  const om::result alone = om::parse("# HELP y c\n# TYPE y counter\ny_total abc\n", openmetrics);
  for (const om::result *parsed : {&paired, &alone}) {
    EXPECT_FALSE(parsed->ok());
    EXPECT_NE(parsed->error.find("invalid value 'abc'"), std::string::npos) << parsed->error;
    ASSERT_FALSE(parsed->families.empty());
    EXPECT_EQ(parsed->families.back().type, om::family_type::counter);
    EXPECT_EQ(parsed->families.back().help, "c");
  }
  EXPECT_EQ(paired.families.size(), 2u);
}

TEST(OpenmetricsParser, BothFamiliesOfAPairMayCarryCreated) {
  // client_golang with created timestamps on writes `x_created` for the
  // counter and for the histogram of the same name. Each keeps its own.
  for (const char *other : {"histogram", "summary"}) {
    const std::string first = std::string(other) == "histogram" ? "x_bucket{le=\"+Inf\"} 4\n" : "x{quantile=\"0.5\"} 0.2\n";
    const std::string body = std::string("# TYPE x ") + other + "\n" + first +
                             "x_sum 1\n"
                             "x_count 4\n"
                             "x_created 1.6e9\n"
                             "# TYPE x counter\n"
                             "x_total 3\n"
                             "x_created 1.7e9\n"
                             "# TYPE after gauge\n"
                             "after 1\n"
                             "# EOF\n";
    const om::result parsed = om::parse(body, openmetrics);
    ASSERT_TRUE(parsed.ok()) << other << ": " << parsed.error;
    expect_consistent(parsed, body, openmetrics);
    ASSERT_EQ(parsed.families.size(), 3u) << other;
    EXPECT_DOUBLE_EQ(parsed.families.at(0).samples.back().value, 1.6e9) << other;
    EXPECT_DOUBLE_EQ(family_typed(parsed, "x", om::family_type::counter).samples.back().value, 1.7e9) << other;
  }
}

TEST(OpenmetricsParser, LimitHitByAnotherFamilyInsideAPairBlockIsTheRepeatedName) {
  // With or without a limit the line after the block belongs to another
  // family, so the block was never a pair: the same line is reported, and the
  // block is not kept.
  // Line 4 is the only one over 20 bytes.
  const std::string body = "# TYPE x gauge\nx 1\n# TYPE x counter\ny{a=\"1\",b=\"2\",c=\"3\"} 1\n";
  om::limits series;
  series.max_series = 1;
  om::limits labels_per_sample;
  labels_per_sample.max_labels_per_sample = 1;
  om::limits line_bytes;
  line_bytes.max_line_bytes = 20;
  for (const om::limits &bounds : {om::limits(), series, labels_per_sample, line_bytes}) {
    const om::result parsed = om::parse(body, openmetrics, bounds);
    EXPECT_FALSE(parsed.ok());
    EXPECT_EQ(parsed.error_line, 3u) << parsed.error;
    EXPECT_NE(parsed.error.find("already declared or sampled"), std::string::npos) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 1u) << parsed.error;
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::gauge);
  }
}

TEST(OpenmetricsParser, OversizedLineInsideAPairBlockIsJudgedInTheDocumentedOrder) {
  // A line the body ends in the middle of is never read, not even for its
  // length; a whole line is judged by the block rule before its length, and
  // the block's own oversized sample is refused for its length.
  om::limits bounds;
  bounds.max_line_bytes = 64;
  const om::result cut = om::parse("# TYPE x gauge\nx 1\n# HELP x again\n" + std::string(10000, 'y'), openmetrics, bounds);
  EXPECT_EQ(cut.error_line, 4u);
  EXPECT_NE(cut.error.find("middle of a line"), std::string::npos) << cut.error;

  const om::result stranger = om::parse("# TYPE x gauge\nx 1\n# HELP x again\n" + std::string(10000, 'y') + "\n", openmetrics, bounds);
  EXPECT_EQ(stranger.error_line, 3u);
  EXPECT_NE(stranger.error.find("already declared or sampled"), std::string::npos) << stranger.error;

  const om::result own = om::parse("# TYPE x gauge\nx 1\n# TYPE x counter\nx_total{a=\"" + std::string(10000, 'y') + "\"} 1\n", openmetrics, bounds);
  EXPECT_EQ(own.error_line, 4u);
  EXPECT_NE(own.error.find("longer than 64 bytes"), std::string::npos) << own.error;
}

// What the official Prometheus Python client (prometheus_client 0.26.0)
// serves for one of every metric type it has - counters with `_created`,
// a gauge with a unit, a labelled histogram, a summary, an info family, an
// enum (a stateset), the three non-finite values, and label values and help
// text that need every escape - in both formats. The expected families are
// what the client's own reference parsers read back from the same bodies, so
// this is the one test here whose expectations no part of this parser wrote -
// with one deliberate difference. The client's text parser renames a counter
// after the OpenMetrics family (`http_requests_total` read back as
// `http_requests`); this parser keeps every Prometheus text family under the
// name its samples carry, so the text expectations say
// `http_requests_total`, and the same for every counter. Regenerate both
// bodies together from the client if either is changed, and keep that
// difference when copying the client's families into the text expectations.
bool same_value(const double a, const double b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

void expect_same_sample(const om::sample &got, const om::sample &want, const std::string &where) {
  EXPECT_EQ(got.name, want.name) << where;
  EXPECT_EQ(got.labels, want.labels) << where << " " << got.name;
  EXPECT_TRUE(same_value(got.value, want.value)) << where << " " << got.name << ": " << got.value << " != " << want.value;
  EXPECT_EQ(got.timestamp, want.timestamp) << where << " " << got.name;
}

// `got` is what reading `want`'s body up to a line boundary gives: the same
// families in the same order, the last one perhaps not yet complete - fewer
// samples, or metadata still to come before its first sample.
void expect_prefix_of(const om::result &got, const om::result &want, const std::string &where) {
  ASSERT_LE(got.families.size(), want.families.size()) << where;
  for (std::size_t i = 0; i < got.families.size(); ++i) {
    const om::family &g = got.families.at(i);
    const om::family &w = want.families.at(i);
    const bool last = i + 1 == got.families.size();
    EXPECT_EQ(g.name, w.name) << where;
    if (!last || !g.samples.empty()) {
      EXPECT_EQ(g.type, w.type) << where << " " << g.name;
      EXPECT_EQ(g.help, w.help) << where << " " << g.name;
      EXPECT_EQ(g.unit, w.unit) << where << " " << g.name;
    }
    if (last) {
      ASSERT_LE(g.samples.size(), w.samples.size()) << where << " " << g.name;
    } else {
      ASSERT_EQ(g.samples.size(), w.samples.size()) << where << " " << g.name;
    }
    for (std::size_t j = 0; j < g.samples.size(); ++j) expect_same_sample(g.samples.at(j), w.samples.at(j), where);
  }
}

void expect_same_families(const om::result &got, const om::result &want, const std::string &where) {
  ASSERT_EQ(got.families.size(), want.families.size()) << where;
  expect_prefix_of(got, want, where);
  for (std::size_t i = 0; i < got.families.size(); ++i) {
    EXPECT_EQ(got.families.at(i).type, want.families.at(i).type) << where;
    EXPECT_EQ(got.families.at(i).help, want.families.at(i).help) << where;
    EXPECT_EQ(got.families.at(i).unit, want.families.at(i).unit) << where;
    EXPECT_EQ(got.families.at(i).samples.size(), want.families.at(i).samples.size()) << where;
  }
}

struct expected_sample {
  const char *name;
  std::vector<std::pair<std::string, std::string> > labels;
  double value;
};

struct expected_family {
  const char *name;
  om::family_type type;
  const char *help;
  const char *unit;
  std::vector<expected_sample> samples;
};

const char *const python_client_openmetrics =
    "# HELP http_requests Requests served, by \\\"code\\\"\\nand path \\\\ root.\n"
    "# TYPE http_requests counter\n"
    "http_requests_total{code=\"200\",path=\"/a \\\"quoted\\\" \\\\path\\nnext\"} 1027.0\n"
    "http_requests_created{code=\"200\",path=\"/a \\\"quoted\\\" \\\\path\\nnext\"} 1.791028073640996e+09\n"
    "http_requests_total{code=\"500\",path=\"/\"} 3.0\n"
    "http_requests_created{code=\"500\",path=\"/\"} 1.791028073641013e+09\n"
    "# HELP queue_depth_bytes Bytes waiting.\n"
    "# TYPE queue_depth_bytes gauge\n"
    "# UNIT queue_depth_bytes bytes\n"
    "queue_depth_bytes 1.5e+09\n"
    "# HELP go_memstats_alloc_bytes Bytes in use.\n"
    "# TYPE go_memstats_alloc_bytes gauge\n"
    "go_memstats_alloc_bytes 1.913168e+06\n"
    "# HELP rpc_latency_seconds RPC latency.\n"
    "# TYPE rpc_latency_seconds histogram\n"
    "rpc_latency_seconds_bucket{le=\"0.1\",method=\"get\"} 1.0\n"
    "rpc_latency_seconds_bucket{le=\"0.5\",method=\"get\"} 2.0\n"
    "rpc_latency_seconds_bucket{le=\"1.0\",method=\"get\"} 3.0\n"
    "rpc_latency_seconds_bucket{le=\"+Inf\",method=\"get\"} 4.0\n"
    "rpc_latency_seconds_count{method=\"get\"} 4.0\n"
    "rpc_latency_seconds_sum{method=\"get\"} 3.95\n"
    "rpc_latency_seconds_created{method=\"get\"} 1.7910280736411815e+09\n"
    "# HELP job_duration_seconds Job duration.\n"
    "# TYPE job_duration_seconds summary\n"
    "job_duration_seconds_count 2.0\n"
    "job_duration_seconds_sum 6.5\n"
    "job_duration_seconds_created 1.7910280736412358e+09\n"
    "# HELP build Build information.\n"
    "# TYPE build info\n"
    "build_info{revision=\"abc123\",version=\"0.12.5\"} 1.0\n"
    "# HELP service_state Service state.\n"
    "# TYPE service_state stateset\n"
    "service_state{service_state=\"starting\"} 0.0\n"
    "service_state{service_state=\"running\"} 1.0\n"
    "service_state{service_state=\"stopped\"} 0.0\n"
    "# HELP weird_values Non-finite values.\n"
    "# TYPE weird_values gauge\n"
    "weird_values{kind=\"nan\"} NaN\n"
    "weird_values{kind=\"pinf\"} +Inf\n"
    "weird_values{kind=\"ninf\"} -Inf\n"
    "# EOF\n";

const char *const python_client_text =
    "# HELP http_requests_total Requests served, by \"code\"\\nand path \\\\ root.\n"
    "# TYPE http_requests_total counter\n"
    "http_requests_total{code=\"200\",path=\"/a \\\"quoted\\\" \\\\path\\nnext\"} 1027.0\n"
    "http_requests_total{code=\"500\",path=\"/\"} 3.0\n"
    "# HELP http_requests_created Requests served, by \"code\"\\nand path \\\\ root.\n"
    "# TYPE http_requests_created gauge\n"
    "http_requests_created{code=\"200\",path=\"/a \\\"quoted\\\" \\\\path\\nnext\"} 1.791028073640996e+09\n"
    "http_requests_created{code=\"500\",path=\"/\"} 1.791028073641013e+09\n"
    "# HELP queue_depth_bytes Bytes waiting.\n"
    "# TYPE queue_depth_bytes gauge\n"
    "queue_depth_bytes 1.5e+09\n"
    "# HELP go_memstats_alloc_bytes Bytes in use.\n"
    "# TYPE go_memstats_alloc_bytes gauge\n"
    "go_memstats_alloc_bytes 1.913168e+06\n"
    "# HELP rpc_latency_seconds RPC latency.\n"
    "# TYPE rpc_latency_seconds histogram\n"
    "rpc_latency_seconds_bucket{le=\"0.1\",method=\"get\"} 1.0\n"
    "rpc_latency_seconds_bucket{le=\"0.5\",method=\"get\"} 2.0\n"
    "rpc_latency_seconds_bucket{le=\"1.0\",method=\"get\"} 3.0\n"
    "rpc_latency_seconds_bucket{le=\"+Inf\",method=\"get\"} 4.0\n"
    "rpc_latency_seconds_count{method=\"get\"} 4.0\n"
    "rpc_latency_seconds_sum{method=\"get\"} 3.95\n"
    "# HELP rpc_latency_seconds_created RPC latency.\n"
    "# TYPE rpc_latency_seconds_created gauge\n"
    "rpc_latency_seconds_created{method=\"get\"} 1.7910280736411815e+09\n"
    "# HELP job_duration_seconds Job duration.\n"
    "# TYPE job_duration_seconds summary\n"
    "job_duration_seconds_count 2.0\n"
    "job_duration_seconds_sum 6.5\n"
    "# HELP job_duration_seconds_created Job duration.\n"
    "# TYPE job_duration_seconds_created gauge\n"
    "job_duration_seconds_created 1.7910280736412358e+09\n"
    "# HELP build_info Build information.\n"
    "# TYPE build_info gauge\n"
    "build_info{revision=\"abc123\",version=\"0.12.5\"} 1.0\n"
    "# HELP service_state Service state.\n"
    "# TYPE service_state gauge\n"
    "service_state{service_state=\"starting\"} 0.0\n"
    "service_state{service_state=\"running\"} 1.0\n"
    "service_state{service_state=\"stopped\"} 0.0\n"
    "# HELP weird_values Non-finite values.\n"
    "# TYPE weird_values gauge\n"
    "weird_values{kind=\"nan\"} NaN\n"
    "weird_values{kind=\"pinf\"} +Inf\n"
    "weird_values{kind=\"ninf\"} -Inf\n";

const expected_family python_client_openmetrics_families[] = {
    {"http_requests",
     om::family_type::counter,
     "Requests served, by \"code\"\nand path \\ root.",
     "",
     {{"http_requests_total", {{"code", "200"}, {"path", "/a \"quoted\" \\path\nnext"}}, 1027.0},
      {"http_requests_created", {{"code", "200"}, {"path", "/a \"quoted\" \\path\nnext"}}, 1791028073.640996},
      {"http_requests_total", {{"code", "500"}, {"path", "/"}}, 3.0},
      {"http_requests_created", {{"code", "500"}, {"path", "/"}}, 1791028073.641013}}},
    {"queue_depth_bytes", om::family_type::gauge, "Bytes waiting.", "bytes", {{"queue_depth_bytes", {}, 1500000000.0}}},
    {"go_memstats_alloc_bytes", om::family_type::gauge, "Bytes in use.", "", {{"go_memstats_alloc_bytes", {}, 1913168.0}}},
    {"rpc_latency_seconds",
     om::family_type::histogram,
     "RPC latency.",
     "",
     {{"rpc_latency_seconds_bucket", {{"le", "0.1"}, {"method", "get"}}, 1.0},
      {"rpc_latency_seconds_bucket", {{"le", "0.5"}, {"method", "get"}}, 2.0},
      {"rpc_latency_seconds_bucket", {{"le", "1.0"}, {"method", "get"}}, 3.0},
      {"rpc_latency_seconds_bucket", {{"le", "+Inf"}, {"method", "get"}}, 4.0},
      {"rpc_latency_seconds_count", {{"method", "get"}}, 4.0},
      {"rpc_latency_seconds_sum", {{"method", "get"}}, 3.95},
      {"rpc_latency_seconds_created", {{"method", "get"}}, 1791028073.6411815}}},
    {"job_duration_seconds",
     om::family_type::summary,
     "Job duration.",
     "",
     {{"job_duration_seconds_count", {}, 2.0}, {"job_duration_seconds_sum", {}, 6.5}, {"job_duration_seconds_created", {}, 1791028073.6412358}}},
    {"build", om::family_type::info, "Build information.", "", {{"build_info", {{"revision", "abc123"}, {"version", "0.12.5"}}, 1.0}}},
    {"service_state",
     om::family_type::stateset,
     "Service state.",
     "",
     {{"service_state", {{"service_state", "starting"}}, 0.0},
      {"service_state", {{"service_state", "running"}}, 1.0},
      {"service_state", {{"service_state", "stopped"}}, 0.0}}},
    {"weird_values",
     om::family_type::gauge,
     "Non-finite values.",
     "",
     {{"weird_values", {{"kind", "nan"}}, std::numeric_limits<double>::quiet_NaN()},
      {"weird_values", {{"kind", "pinf"}}, std::numeric_limits<double>::infinity()},
      {"weird_values", {{"kind", "ninf"}}, -std::numeric_limits<double>::infinity()}}}};

const expected_family python_client_text_families[] = {
    {"http_requests_total",
     om::family_type::counter,
     "Requests served, by \"code\"\nand path \\ root.",
     "",
     {{"http_requests_total", {{"code", "200"}, {"path", "/a \"quoted\" \\path\nnext"}}, 1027.0},
      {"http_requests_total", {{"code", "500"}, {"path", "/"}}, 3.0}}},
    {"http_requests_created",
     om::family_type::gauge,
     "Requests served, by \"code\"\nand path \\ root.",
     "",
     {{"http_requests_created", {{"code", "200"}, {"path", "/a \"quoted\" \\path\nnext"}}, 1791028073.640996},
      {"http_requests_created", {{"code", "500"}, {"path", "/"}}, 1791028073.641013}}},
    {"queue_depth_bytes", om::family_type::gauge, "Bytes waiting.", "", {{"queue_depth_bytes", {}, 1500000000.0}}},
    {"go_memstats_alloc_bytes", om::family_type::gauge, "Bytes in use.", "", {{"go_memstats_alloc_bytes", {}, 1913168.0}}},
    {"rpc_latency_seconds",
     om::family_type::histogram,
     "RPC latency.",
     "",
     {{"rpc_latency_seconds_bucket", {{"le", "0.1"}, {"method", "get"}}, 1.0},
      {"rpc_latency_seconds_bucket", {{"le", "0.5"}, {"method", "get"}}, 2.0},
      {"rpc_latency_seconds_bucket", {{"le", "1.0"}, {"method", "get"}}, 3.0},
      {"rpc_latency_seconds_bucket", {{"le", "+Inf"}, {"method", "get"}}, 4.0},
      {"rpc_latency_seconds_count", {{"method", "get"}}, 4.0},
      {"rpc_latency_seconds_sum", {{"method", "get"}}, 3.95}}},
    {"rpc_latency_seconds_created", om::family_type::gauge, "RPC latency.", "", {{"rpc_latency_seconds_created", {{"method", "get"}}, 1791028073.6411815}}},
    {"job_duration_seconds", om::family_type::summary, "Job duration.", "", {{"job_duration_seconds_count", {}, 2.0}, {"job_duration_seconds_sum", {}, 6.5}}},
    {"job_duration_seconds_created", om::family_type::gauge, "Job duration.", "", {{"job_duration_seconds_created", {}, 1791028073.6412358}}},
    {"build_info", om::family_type::gauge, "Build information.", "", {{"build_info", {{"revision", "abc123"}, {"version", "0.12.5"}}, 1.0}}},
    {"service_state",
     om::family_type::gauge,
     "Service state.",
     "",
     {{"service_state", {{"service_state", "starting"}}, 0.0},
      {"service_state", {{"service_state", "running"}}, 1.0},
      {"service_state", {{"service_state", "stopped"}}, 0.0}}},
    {"weird_values",
     om::family_type::gauge,
     "Non-finite values.",
     "",
     {{"weird_values", {{"kind", "nan"}}, std::numeric_limits<double>::quiet_NaN()},
      {"weird_values", {{"kind", "pinf"}}, std::numeric_limits<double>::infinity()},
      {"weird_values", {{"kind", "ninf"}}, -std::numeric_limits<double>::infinity()}}}};

void expect_families(const om::result &parsed, const expected_family *expected, const std::size_t count) {
  ASSERT_EQ(parsed.families.size(), count);
  for (std::size_t i = 0; i < count; ++i) {
    const om::family &f = parsed.families.at(i);
    const expected_family &e = expected[i];
    EXPECT_EQ(f.name, e.name);
    EXPECT_EQ(f.type, e.type) << e.name;
    EXPECT_EQ(f.help, e.help) << e.name;
    EXPECT_EQ(f.unit, e.unit) << e.name;
    ASSERT_EQ(f.samples.size(), e.samples.size()) << e.name;
    for (std::size_t j = 0; j < e.samples.size(); ++j) {
      const om::sample &s = f.samples.at(j);
      EXPECT_EQ(s.name, e.samples.at(j).name) << e.name;
      EXPECT_EQ(s.labels, om::label_list(e.samples.at(j).labels.begin(), e.samples.at(j).labels.end())) << s.name;
      if (std::isnan(e.samples.at(j).value)) {
        EXPECT_TRUE(std::isnan(s.value)) << s.name;
      } else {
        EXPECT_EQ(s.value, e.samples.at(j).value) << s.name;
      }
      EXPECT_FALSE(s.timestamp.has_value()) << s.name;
    }
  }
}

TEST(OpenmetricsParser, PythonClientOpenMetricsReadsAsTheReferenceParserReadsIt) {
  const om::result parsed = om::parse(python_client_openmetrics, openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error << " on line " << parsed.error_line;
  EXPECT_TRUE(parsed.saw_eof);
  expect_consistent(parsed, python_client_openmetrics, openmetrics);
  expect_families(parsed, python_client_openmetrics_families, sizeof(python_client_openmetrics_families) / sizeof(python_client_openmetrics_families[0]));
}

TEST(OpenmetricsParser, PythonClientTextReadsAsTheReferenceParserReadsIt) {
  const om::result parsed = om::parse(python_client_text, text);
  ASSERT_TRUE(parsed.ok()) << parsed.error << " on line " << parsed.error_line;
  EXPECT_FALSE(parsed.saw_eof);
  expect_consistent(parsed, python_client_text, text);
  expect_families(parsed, python_client_text_families, sizeof(python_client_text_families) / sizeof(python_client_text_families[0]));
}

TEST(OpenmetricsParser, PythonClientBodiesSurviveEveryCut) {
  // The same bodies cut at every byte: a cut on a line feed reads as the
  // lines before it - each family as the whole body has it, up to the cut -
  // and any other cut is refused, keeping exactly what the lines before it
  // read as.
  for (const std::pair<const char *, om::format> &entry : {std::make_pair(python_client_openmetrics, openmetrics), std::make_pair(python_client_text, text)}) {
    const std::string body = entry.first;
    const om::result whole = om::parse(body, entry.second);
    ASSERT_TRUE(whole.ok()) << whole.error;
    for (std::size_t length = 0; length < body.size(); ++length) {
      const std::string prefix = body.substr(0, length);
      const om::result parsed = om::parse(prefix, entry.second);
      expect_consistent(parsed, prefix, entry.second);
      const bool on_a_line = prefix.empty() || prefix.back() == '\n' || (entry.second == openmetrics && length == body.size() - 1);
      EXPECT_EQ(parsed.ok(), on_a_line) << "cut at " << length << ": " << parsed.error;
      if (on_a_line) {
        expect_prefix_of(parsed, whole, "cut at " + std::to_string(length));
      } else {
        const om::result lines = om::parse(body.substr(0, prefix.rfind('\n') + 1), entry.second);
        expect_same_families(parsed, lines, "cut at " + std::to_string(length));
      }
      if (::testing::Test::HasFailure()) return;
    }
  }
}

TEST(OpenmetricsParser, NamesMayStartWithEveryLetterTheGrammarAllows) {
  // The ends of each range: `a` and `z`, `A` and `Z`, and the two
  // non-letters a metric name may start with.
  for (const char *name : {"a", "z", "A", "Z", "_x", ":x", "aZ09_:"}) {
    const om::result parsed = om::parse(std::string(name) + " 1\n", text);
    ASSERT_TRUE(parsed.ok()) << name << ": " << parsed.error;
    EXPECT_EQ(parsed.families.at(0).name, name);
  }
  const om::result labelled = om::parse("m{a=\"1\",z=\"2\",A=\"3\",Z=\"4\",_=\"5\",a_Z9=\"6\"} 1\n", text);
  ASSERT_TRUE(labelled.ok()) << labelled.error;
  EXPECT_EQ(labelled.families.at(0).samples.at(0).labels.size(), 6u);
  // And the characters either side of each range are not letters.
  for (const char *name : {"`x", "{x", "@x", "[x", "9x"}) {
    EXPECT_FALSE(om::parse(std::string(name) + " 1\n", text).ok()) << name;
  }
}

TEST(OpenmetricsParser, DigitsEndWhereTheGrammarSaysTheyDo) {
  // `0` and `9` are digits; the characters either side of them, `/` and `:`,
  // are not - `:` is a name character, but not a label-name one.
  for (const char *good : {"a0 1\n", "a9 1\n", "a{b0=\"1\",b9=\"2\"} 1\n", "a:b 1\n"}) {
    const om::result parsed = om::parse(good, text);
    EXPECT_TRUE(parsed.ok()) << good << " -> " << parsed.error;
  }
  for (const char *bad : {"a/b 1\n", "a{b/c=\"1\"} 1\n", "a{b:c=\"1\"} 1\n", "a 1/\n", "a 1:\n", "a 1 1/\n", "a 1 1:\n"}) {
    const om::result parsed = om::parse(bad, text);
    EXPECT_FALSE(parsed.ok()) << bad;
    EXPECT_EQ(parsed.error_line, 1u) << bad;
    EXPECT_TRUE(parsed.families.empty()) << bad;
  }
}

TEST(OpenmetricsParser, OnlyACommentLineIsTheTerminator) {
  // `# EOF` is a comment line; a line that merely ends in the word is not.
  for (const char *line : {"XEOF", "a EOF", " EOF", "EOF"}) {
    const om::result parsed = om::parse(std::string("a 1\n") + line + "\n", openmetrics);
    EXPECT_FALSE(parsed.ok()) << line;
    EXPECT_FALSE(parsed.saw_eof) << line;
    EXPECT_EQ(parsed.error_line, 2u) << line << " -> " << parsed.error;
  }
  // Nor, without its line feed, may it end the body: it is a cut line.
  for (const char *last : {"XEOF", "a EOF"}) {
    const om::result parsed = om::parse(std::string("a 1\n") + last, openmetrics);
    EXPECT_NE(parsed.error.find("middle of a line"), std::string::npos) << last << " -> " << parsed.error;
  }
  // A blank last line without its line feed may follow `# EOF`.
  const om::result blank = om::parse("a 1\n# EOF\n   ", openmetrics);
  EXPECT_TRUE(blank.ok()) << blank.error;
  EXPECT_TRUE(blank.saw_eof);
}

TEST(OpenmetricsParser, HelpEndingInABackslashKeepsIt) {
  // A backslash with nothing after it escapes nothing, so it is kept as
  // served - with blanks or a carriage return after it in the body or not.
  for (const char *line : {"# HELP x end\\\n", "# HELP x end\\  \n", "# HELP x end\\\r\n"}) {
    const om::result parsed = om::parse(std::string(line) + "x 1\n", text);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    EXPECT_EQ(parsed.families.at(0).help, "end\\") << line;
  }
}

TEST(OpenmetricsParser, DecimalsEitherSideOfTheConversionBuffer) {
  // Values are copied into a 64-byte buffer for conversion, longer ones onto
  // the heap: 62 to 66 characters cover the boundary. The terminator is
  // written through `std::array::at`, so an off-by-one in the bound throws
  // here in every build rather than overflowing the stack where only a
  // sanitizer would see it.
  for (const std::size_t length : {62u, 63u, 64u, 65u, 66u}) {
    const std::string value = "0." + std::string(length - 2, '5');
    const om::result parsed = om::parse("v " + value + "\n", text);
    ASSERT_TRUE(parsed.ok()) << length << ": " << parsed.error;
    EXPECT_NEAR(parsed.families.at(0).samples.at(0).value, 0.5555555555555556, 1e-15) << length;
  }
}

TEST(OpenmetricsParser, EveryTypeHasItsOpenMetricsName) {
  const std::pair<om::family_type, const char *> names[] = {{om::family_type::unknown, "unknown"},
                                                            {om::family_type::counter, "counter"},
                                                            {om::family_type::gauge, "gauge"},
                                                            {om::family_type::histogram, "histogram"},
                                                            {om::family_type::gaugehistogram, "gaugehistogram"},
                                                            {om::family_type::summary, "summary"},
                                                            {om::family_type::info, "info"},
                                                            {om::family_type::stateset, "stateset"}};
  for (const std::pair<om::family_type, const char *> &n : names) EXPECT_STREQ(om::type_name(n.first), n.second);
}

TEST(OpenmetricsParser, EverySpellingOfTheNonFiniteValuesIsRead) {
  // As Go's ParseFloat reads them: any case, the long form of infinity, and
  // a sign on infinity but not on NaN - which Go refuses, as OpenMetrics does.
  struct spelling {
    const char *text;
    int sign;  // 0 for NaN
  };
  for (const om::format f : {openmetrics, text}) {
    for (const char *signed_nan : {"+NaN", "-NaN", "+nan", "-nan", "+NAN"}) {
      const om::result parsed = om::parse(std::string("v ") + signed_nan + "\n", f);
      EXPECT_FALSE(parsed.ok()) << signed_nan;
      EXPECT_NE(parsed.error.find("invalid value"), std::string::npos) << signed_nan << " -> " << parsed.error;
    }
  }
  const spelling spellings[] = {{"NaN", 0},      {"nan", 0},      {"Inf", 1},       {"+inf", 1},       {"-INF", -1}, {"NAN", 0},
                                {"INFINITY", 1}, {"Infinity", 1}, {"+infinity", 1}, {"-Infinity", -1}, {"-inf", -1}, {"nAn", 0}};
  for (const spelling &sp : spellings) {
    const om::result parsed = om::parse(std::string("v ") + sp.text + "\n", text);
    ASSERT_TRUE(parsed.ok()) << sp.text << ": " << parsed.error;
    const double v = parsed.families.at(0).samples.at(0).value;
    if (sp.sign == 0) {
      EXPECT_TRUE(std::isnan(v)) << sp.text;
    } else {
      EXPECT_TRUE(std::isinf(v)) << sp.text;
      EXPECT_EQ(v > 0, sp.sign > 0) << sp.text;
    }
  }
}

TEST(OpenmetricsParser, PrometheusTextTimestampSignsAndOpenMetricsExponents) {
  const om::result signs = om::parse("a 1 +5\na 2 -5\n", text);
  ASSERT_TRUE(signs.ok()) << signs.error;
  EXPECT_DOUBLE_EQ(signs.families.at(0).samples.at(0).timestamp.value(), 5);
  EXPECT_DOUBLE_EQ(signs.families.at(0).samples.at(1).timestamp.value(), -5);
  for (const char *bad : {"a 1 +\n", "a 1 -\n", "a 1 1.5\n", "a 1 1e3\n"}) {
    const om::result parsed = om::parse(bad, text);
    EXPECT_NE(parsed.error.find("invalid timestamp"), std::string::npos) << bad << " -> " << parsed.error;
  }
  const om::result exponent = om::parse("a 1 1.7e9\n# EOF\n", openmetrics);
  ASSERT_TRUE(exponent.ok()) << exponent.error;
  EXPECT_DOUBLE_EQ(exponent.families.at(0).samples.at(0).timestamp.value(), 1.7e9);
}

TEST(OpenmetricsParser, OverLongLineOfAnotherFamilyLeavesTheFamilyBeingRead) {
  // Only a metadata line of the family being read fails that family; an
  // over-long sample, or metadata naming another family, stops the parse with
  // the family as it was.
  om::limits bounds;
  bounds.max_line_bytes = 20;
  for (const std::string &tail :
       {"y{a=\"" + std::string(30, 'v') + "\"} 1\n", "# HELP other " + std::string(30, 'h') + "\n", "# HELP y.z " + std::string(30, 'h') + "\n"}) {
    const om::result parsed = om::parse("# TYPE y counter\n" + tail, text, bounds);
    EXPECT_EQ(parsed.error_line, 2u) << tail;
    EXPECT_NE(parsed.error.find("longer than 20 bytes"), std::string::npos) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 1u) << tail;
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::counter);
  }
}

TEST(OpenmetricsParser, PrometheusTextCounterWithoutTotal) {
  // Older exporters declare a counter and serve it under the bare name.
  const om::result parsed = om::parse("# TYPE jobs counter\njobs 4\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).name, "jobs");
}

TEST(OpenmetricsParser, PrometheusTextSampleBeforeItsTypeIsAnotherMetric) {
  // In the older format `foo_total` and the counter `foo` are unrelated names.
  const om::result parsed = om::parse("foo_total 1\n# TYPE foo counter\nfoo 2\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::counter);
}

TEST(OpenmetricsParser, HistogramSamplesAttachToTheirFamily) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# HELP latency_seconds Request latency.\n"
        "# TYPE latency_seconds histogram\n"
        "latency_seconds_bucket{le=\"0.1\"} 3\n"
        "latency_seconds_bucket{le=\"1\"} 7\n"
        "latency_seconds_bucket{le=\"+Inf\"} 8\n"
        "latency_seconds_sum 4.25\n"
        "latency_seconds_count 8\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    const om::family &h = only_family(parsed);
    EXPECT_EQ(h.name, "latency_seconds");
    EXPECT_EQ(h.type, om::family_type::histogram);
    ASSERT_EQ(h.samples.size(), 5u);
    EXPECT_EQ(om::find_label(h.samples.at(2), "le").value(), "+Inf");
    EXPECT_EQ(h.samples.at(3).name, "latency_seconds_sum");
    EXPECT_DOUBLE_EQ(h.samples.at(4).value, 8);
  }
}

TEST(OpenmetricsParser, SummaryQuantilesSumAndCountAttachToTheirFamily) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse(
        "# TYPE rpc_duration summary\n"
        "rpc_duration{quantile=\"0.5\"} 0.2\n"
        "rpc_duration{quantile=\"0.99\"} 1.4\n"
        "rpc_duration_sum 120\n"
        "rpc_duration_count 400\n",
        f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    const om::family &s = only_family(parsed);
    EXPECT_EQ(s.type, om::family_type::summary);
    ASSERT_EQ(s.samples.size(), 4u);
    EXPECT_EQ(om::find_label(s.samples.at(1), "quantile").value(), "0.99");
  }
}

TEST(OpenmetricsParser, OpenMetricsOnlyTypes) {
  const om::result parsed = om::parse(
      "# TYPE build info\n"
      "build_info{version=\"0.12.5\",revision=\"abc\"} 1\n"
      "# TYPE queue gaugehistogram\n"
      "queue_bucket{le=\"+Inf\"} 3\n"
      "queue_gsum 7\n"
      "queue_gcount 3\n"
      "# TYPE state stateset\n"
      "state{state=\"a\"} 1\n"
      "# TYPE legacy unknown\n"
      "legacy 1\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 4u);
  EXPECT_EQ(parsed.families.at(0).name, "build");
  EXPECT_EQ(parsed.families.at(0).type, om::family_type::info);
  EXPECT_EQ(parsed.families.at(0).samples.at(0).labels, labels({{"version", "0.12.5"}, {"revision", "abc"}}));
  EXPECT_EQ(parsed.families.at(1).type, om::family_type::gaugehistogram);
  EXPECT_EQ(parsed.families.at(1).samples.size(), 3u);
  EXPECT_EQ(parsed.families.at(2).type, om::family_type::stateset);
  EXPECT_EQ(parsed.families.at(3).type, om::family_type::unknown);
}

TEST(OpenmetricsParser, PrometheusTextUntypedIsUnknown) {
  const om::result parsed = om::parse("# TYPE legacy untyped\nlegacy 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).type, om::family_type::unknown);
  EXPECT_STREQ(om::type_name(om::family_type::unknown), "unknown");
}

TEST(OpenmetricsParser, SamplesWithoutMetadataAreFamiliesOfUnknownType) {
  // Nothing says `a_total` is a counter, so it is its own unknown family.
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse("a{x=\"1\"} 1\na{x=\"2\"} 2\na_total 3\n", f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 2u);
    EXPECT_EQ(parsed.families.at(0).name, "a");
    EXPECT_EQ(parsed.families.at(0).type, om::family_type::unknown);
    EXPECT_EQ(parsed.families.at(0).samples.size(), 2u);
    EXPECT_EQ(parsed.families.at(1).name, "a_total");
  }
}

TEST(OpenmetricsParser, OpenMetricsSamplesOfAFamilyMustBeContiguous) {
  // OpenMetrics forbids a family's samples apart from each other; where a
  // same-name pair can follow, the name would not say which family is meant.
  for (const char *body : {"a 1\na_total 3\na 4\n", "# TYPE h histogram\nh_bucket{le=\"+Inf\"} 1\ng 1\nh_sum 2\n"}) {
    const om::result parsed = om::parse(body, openmetrics);
    EXPECT_FALSE(parsed.ok()) << body;
    EXPECT_NE(parsed.error.find("apart from the rest of the family"), std::string::npos) << body << " -> " << parsed.error;
    EXPECT_EQ(parsed.families.size(), 2u) << body;
  }
}

TEST(OpenmetricsParser, PrometheusTextRegroupsAFamilySplitByAnother) {
  // The text format asks for the same grouping, but its reference parser
  // takes a family's samples back wherever they come, and some exporters rely
  // on that. There are no pairs in it, so the name always says which family.
  const std::string body =
      "# TYPE foo gauge\n"
      "foo{a=\"1\"} 1\n"
      "# TYPE bar gauge\n"
      "bar 1\n"
      "foo{a=\"2\"} 2\n"
      "# TYPE h histogram\n"
      "h_bucket{le=\"+Inf\"} 1\n"
      "g 1\n"
      "h_sum 2\n";
  const om::result parsed = om::parse(body, text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  expect_consistent(parsed, body, text);
  ASSERT_EQ(parsed.families.size(), 4u);
  EXPECT_EQ(family_named(parsed, "foo").samples.size(), 2u);
  EXPECT_EQ(family_named(parsed, "h").samples.size(), 2u);
  // Metadata after the family's samples is still a second declaration.
  const om::result late = om::parse("foo 1\nbar 1\n# HELP foo late\n", text);
  EXPECT_FALSE(late.ok());
  EXPECT_EQ(late.error_line, 3u);
}

TEST(OpenmetricsParser, GaugeDoesNotSwallowASampleWithACounterSuffix) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse("# TYPE temp gauge\ntemp 21\ntemp_total 4\n", f);
    ASSERT_TRUE(parsed.ok()) << parsed.error;
    ASSERT_EQ(parsed.families.size(), 2u);
    EXPECT_EQ(parsed.families.at(1).name, "temp_total");
  }
}

TEST(OpenmetricsParser, HelpIsUnescaped) {
  const om::result parsed = om::parse("# HELP g Line one\\nline two, a \\\\ and a \\\" and a stray \\d.\ng 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).help, "Line one\nline two, a \\ and a \" and a stray \\d.");
}

TEST(OpenmetricsParser, KeywordGluedToItsNameIsJustAComment) {
  // `# TYPEfoo` is not a `# TYPE` line, so it declares nothing - the sample
  // after it is an unknown family, not a gauge.
  const om::result parsed = om::parse("# TYPEfoo gauge\nfoo 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).type, om::family_type::unknown);
}

TEST(OpenmetricsParser, OtherCommentsAndBlankLinesAreIgnored) {
  const om::result parsed = om::parse(
      "# Exported by something.\n"
      "\n"
      "#nothing to see\n"
      "   \n"
      "g 1\n"
      "\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "g");
}

TEST(OpenmetricsParser, CrlfLineEndingsAreAccepted) {
  const om::result parsed = om::parse("# TYPE g gauge\r\ng{a=\"b\"} 1\r\n# EOF\r\n", openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_TRUE(parsed.saw_eof);
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(0).value, 1);
}

// --- # EOF ----------------------------------------------------------------------

TEST(OpenmetricsParser, PrometheusTextEofIsJustAComment) {
  // The older format has no terminator; a comment that reads `# EOF` - or
  // starts with it - ends nothing.
  const om::result parsed = om::parse(
      "# EOF of the header section\n"
      "a 1\n"
      "# EOF\n"
      "b 2\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_FALSE(parsed.saw_eof);
  EXPECT_EQ(parsed.families.size(), 2u);
}

TEST(OpenmetricsParser, OpenMetricsEofMayBeFollowedByBlankLinesOnly) {
  const om::result trailing = om::parse("foo 1\n# EOF\n\n  \n", openmetrics);
  EXPECT_TRUE(trailing.ok()) << trailing.error;
  EXPECT_TRUE(trailing.saw_eof);
}

TEST(OpenmetricsParser, UnterminatedEofIsAcceptedInEveryShapeTheReaderAccepts) {
  // Whatever the line reader takes for `# EOF` when a line feed follows, it
  // must also take when the line feed was left off.
  for (const char *body : {"foo 1\n# EOF", "foo 1\n  # EOF  ", "foo 1\n#EOF", "foo 1\n# EOF\r", "foo 1\n# EOF\n   ", "foo 1\n# EOF\n\r"}) {
    const om::result parsed = om::parse(body, openmetrics);
    EXPECT_TRUE(parsed.ok()) << body << " -> " << parsed.error;
    EXPECT_TRUE(parsed.saw_eof) << body;
  }
}

// --- samples ----------------------------------------------------------------

TEST(OpenmetricsParser, LabelValuesAreUnescaped) {
  const om::result parsed = om::parse("disk{path=\"\\\\Device\\\\HarddiskVolume1\",desc=\"a \\\"quoted\\\" name\\nsecond\"} 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).labels, labels({{"path", "\\Device\\HarddiskVolume1"}, {"desc", "a \"quoted\" name\nsecond"}}));
}

TEST(OpenmetricsParser, LabelSetsMayHaveBlanksATrailingCommaOrBeEmpty) {
  const om::result parsed = om::parse(
      "a{ x = \"1\" , y=\"2\", } 1\n"
      "b{} 2\n"
      "c {z=\"#{}\"}\t3\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  ASSERT_EQ(parsed.families.size(), 3u);
  EXPECT_EQ(parsed.families.at(0).samples.at(0).labels, labels({{"x", "1"}, {"y", "2"}}));
  EXPECT_TRUE(parsed.families.at(1).samples.at(0).labels.empty());
  EXPECT_EQ(parsed.families.at(2).samples.at(0).labels, labels({{"z", "#{}"}}));
  EXPECT_DOUBLE_EQ(parsed.families.at(2).samples.at(0).value, 3);
}

TEST(OpenmetricsParser, NonFiniteValuesArePreserved) {
  const om::result parsed = om::parse(
      "v{k=\"a\"} NaN\n"
      "v{k=\"b\"} +Inf\n"
      "v{k=\"c\"} -Inf\n"
      "v{k=\"d\"} Inf\n"
      "v{k=\"e\"} -1.5e-3\n"
      "v{k=\"f\"} .5\n",
      text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 6u);
  EXPECT_TRUE(std::isnan(f.samples.at(0).value));
  EXPECT_TRUE(std::isinf(f.samples.at(1).value) && f.samples.at(1).value > 0);
  EXPECT_TRUE(std::isinf(f.samples.at(2).value) && f.samples.at(2).value < 0);
  EXPECT_TRUE(std::isinf(f.samples.at(3).value) && f.samples.at(3).value > 0);
  EXPECT_DOUBLE_EQ(f.samples.at(4).value, -0.0015);
  EXPECT_DOUBLE_EQ(f.samples.at(5).value, 0.5);
}

TEST(OpenmetricsParser, ValuesBelowTheSmallestNormalAreReadOnEveryPlatform) {
  // A subnormal is what the renderer writes for one, and some C++ streams
  // refuse it while others read it. `strtod` reads it the same way everywhere.
  const om::result parsed = om::parse("x{k=\"a\"} 5e-324\nx{k=\"b\"} 1e-400\nx{k=\"c\"} -2.2250738585072014e-309\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 3u);
  EXPECT_GT(f.samples.at(0).value, 0);
  EXPECT_LT(f.samples.at(0).value, 1e-320);
  EXPECT_EQ(f.samples.at(1).value, 0);
  EXPECT_LT(f.samples.at(2).value, 0);
}

namespace {

// Switches LC_NUMERIC to the first installed locale whose decimal point
// satisfies `wanted`, for the life of the object. Stock Linux images ship no
// locale beyond C and C.UTF-8, so a test using one skips where none is
// installed - except where `NSCP_REQUIRE_TEST_LOCALES` is set, as the
// sanitizer job sets it after generating them, which turns the skip into a
// failure so that one job always runs these tests.
class numeric_locale {
 public:
  template <typename Predicate>
  numeric_locale(std::initializer_list<const char *> names, Predicate wanted) {
    const char *previous = std::setlocale(LC_NUMERIC, nullptr);
    restore_ = previous == nullptr ? "C" : previous;
    for (const char *name : names) {
      if (std::setlocale(LC_NUMERIC, name) != nullptr && wanted(std::string(std::localeconv()->decimal_point))) {
        active_ = name;
        return;
      }
    }
    std::setlocale(LC_NUMERIC, restore_.c_str());
  }
  ~numeric_locale() { std::setlocale(LC_NUMERIC, restore_.c_str()); }
  const char *active() const { return active_; }

 private:
  std::string restore_;
  const char *active_ = nullptr;
};

bool locales_required() {
  const char *required = std::getenv("NSCP_REQUIRE_TEST_LOCALES");
  return required != nullptr && *required != '\0' && std::string(required) != "0";
}

}  // namespace

TEST(OpenmetricsParser, DecimalPointDoesNotFollowTheProcessLocale) {
  // `strtod` reads the C locale's decimal point, and an agent whose locale
  // spells it `,` must still read `1.5` as one and a half.
  const numeric_locale comma({"de_DE.UTF-8", "de_DE.utf8", "de_DE", "fr_FR.UTF-8", "fr_FR", "sv_SE.UTF-8", "ru_RU.UTF-8", "pt_BR.UTF-8", "German_Germany.1252",
                              "de-DE", "German", "fr-FR"},
                             [](const std::string &point) { return point == ","; });
  if (comma.active() == nullptr) {
    if (locales_required()) FAIL() << "NSCP_REQUIRE_TEST_LOCALES is set, but no locale with a comma decimal point is installed";
    GTEST_SKIP() << "no locale with a comma decimal point is installed";
  }
  const om::result parsed = om::parse("x 1.5\ny 2,5\n", text);
  EXPECT_FALSE(parsed.ok()) << comma.active();
  EXPECT_EQ(parsed.error_line, 2u) << parsed.error;
  EXPECT_DOUBLE_EQ(family_named(parsed, "x").samples.at(0).value, 1.5) << comma.active();
}

TEST(OpenmetricsParser, MultiByteDecimalPointDoesNotMatterEither) {
  // A decimal point no single character can stand in for, such as the Arabic
  // decimal separator.
  // Pashto writes it as U+066B. Windows refuses a Unicode-only locale under
  // an ANSI code page, so it is asked for with its UTF-8 code page there.
  const numeric_locale wide(
      {"ps_AF.UTF-8", "ps_AF.utf8", "ps_AF", "ps-AF.UTF-8", "ps-AF.utf8", "ps_AF.65001", "fa_IR.UTF-8", "fa-IR.UTF-8", "ar_EG.UTF-8", "ar-EG.UTF-8"},
      [](const std::string &point) { return point.size() > 1; });
  if (wide.active() == nullptr) {
    if (locales_required()) FAIL() << "NSCP_REQUIRE_TEST_LOCALES is set, but no locale with a multi-byte decimal point is installed";
    GTEST_SKIP() << "no locale with a multi-byte decimal point is installed";
  }
  const om::result parsed = om::parse("x 1.5\ny 5e-324\n", text);
  ASSERT_TRUE(parsed.ok()) << wide.active() << ": " << parsed.error;
  EXPECT_DOUBLE_EQ(family_named(parsed, "x").samples.at(0).value, 1.5);
  EXPECT_GT(family_named(parsed, "y").samples.at(0).value, 0);
}

TEST(OpenmetricsParser, LongDecimalIsRead) {
  // Longer than the stack buffer the conversion uses.
  const std::string digits(200, '1');
  const om::result parsed = om::parse("x 0." + digits + "\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_NEAR(only_family(parsed).samples.at(0).value, 0.1111111111, 1e-9);
}

TEST(OpenmetricsParser, OpenMetricsTimestampIsSecondsAndExemplarSkipped) {
  const om::result parsed = om::parse(
      "# TYPE lat histogram\n"
      "lat_bucket{le=\"1\"} 3 1700000000.5 # {trace_id=\"abc\"} 0.4 1700000000.1\n"
      "lat_bucket{le=\"+Inf\"} 4 # {trace_id=\"def\"} 2\n"
      "lat_count 4 1700000000\n"
      "# EOF\n",
      openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::family &f = only_family(parsed);
  ASSERT_EQ(f.samples.size(), 3u);
  EXPECT_DOUBLE_EQ(f.samples.at(0).timestamp.value(), 1700000000.5);
  EXPECT_FALSE(f.samples.at(1).timestamp.has_value());
  EXPECT_DOUBLE_EQ(f.samples.at(1).value, 4);
  EXPECT_DOUBLE_EQ(f.samples.at(2).timestamp.value(), 1700000000.0);
}

TEST(OpenmetricsParser, PrometheusTextTimestampIsIntegerMilliseconds) {
  const om::result parsed = om::parse("foo 1 1700000000500\nfoo{a=\"b\"} 2 -5\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(0).timestamp.value(), 1700000000500.0);
  EXPECT_DOUBLE_EQ(only_family(parsed).samples.at(1).timestamp.value(), -5.0);
}

TEST(OpenmetricsParser, ColonsAreAllowedInMetricNames) {
  const om::result parsed = om::parse("job:requests:rate5m 12\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).name, "job:requests:rate5m");
}

TEST(OpenmetricsParser, FindLabelReportsAMissingLabel) {
  const om::result parsed = om::parse("a{x=\"1\"} 1\n", text);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  const om::sample &s = only_family(parsed).samples.at(0);
  EXPECT_EQ(om::find_label(s, "x").value(), "1");
  EXPECT_FALSE(om::find_label(s, "y").has_value());
}

// --- errors -------------------------------------------------------------------

// Every way a line can be malformed that the grammar distinguishes, one named
// case each, so a regression reports which input it broke on rather than "one
// of the forty in this table". Each case asserts the line the parser stopped on
// and a fragment of the reason, because both are what reaches the operator,
// and runs in every format it is malformed in.
enum class in { both, openmetrics_only, text_only };

struct malformed {
  const char *name;
  std::string body;
  std::size_t line;
  const char *fragment;
  in formats = in::both;
};

void PrintTo(const malformed &m, std::ostream *os) { *os << m.name; }

class OpenmetricsParserMalformed : public ::testing::TestWithParam<malformed> {};

TEST_P(OpenmetricsParserMalformed, IsReportedOnItsLine) {
  const malformed &m = GetParam();
  for (const om::format f : {openmetrics, text}) {
    if (m.formats == in::openmetrics_only && f != openmetrics) continue;
    if (m.formats == in::text_only && f != text) continue;
    const char *label = f == openmetrics ? "openmetrics" : "text";
    const om::result parsed = om::parse(m.body, f);
    EXPECT_FALSE(parsed.ok()) << label << ": " << m.body;
    EXPECT_EQ(parsed.error_line, m.line) << label << ": " << m.body << " -> " << parsed.error;
    EXPECT_NE(parsed.error.find(m.fragment), std::string::npos) << label << ": " << m.body << " -> " << parsed.error;
    expect_stops_at_error(parsed, m.body, f);
  }
}

std::string case_name(const ::testing::TestParamInfo<malformed> &info) { return info.param.name; }

// Metric names: `[a-zA-Z_:][a-zA-Z0-9_:]*`, then a blank or a label set.
INSTANTIATE_TEST_SUITE_P(
    Names, OpenmetricsParserMalformed,
    ::testing::Values(malformed{"NoName", "ok 1\n{a=\"b\"} 1\n", 2, "expected a metric name"},
                      malformed{"LeadingDigit", "9lives 1\n", 1, "expected a metric name"}, malformed{"LeadingDash", "-foo 1\n", 1, "expected a metric name"},
                      malformed{"Dash", "foo-bar 1\n", 1, "invalid character after 'foo'"}, malformed{"Dot", "foo.bar 1\n", 1, "invalid character after 'foo'"},
                      malformed{"NonAscii", "f\xc3\xb6o 1\n", 1, "invalid character after 'f'"},
                      malformed{"Nul", std::string("foo\0 1\n", 7), 1, "invalid character after 'foo'"},
                      malformed{"QuoteInName", "foo\"bar\" 1\n", 1, "invalid character"}, malformed{"NameOnly", "ok 1\nfoo\n", 2, "missing value for 'foo'"},
                      malformed{"NameAndBlanks", "foo   \t\n", 1, "missing value for 'foo'"},
                      malformed{"NameAndLabelsOnly", "foo{a=\"b\"}\n", 1, "missing value"},
                      malformed{"NoBlankAfterLabels", "foo{a=\"b\"}1\n", 1, "invalid character after 'foo'"},
                      malformed{"DoubleLabelSet", "foo{a=\"b\"}{c=\"d\"} 1\n", 1, "invalid character"},
                      malformed{"ClosingBraceTwice", "foo{a=\"b\"}} 1\n", 1, "invalid character"}),
    case_name);

// Label sets: `{name="value",...}`, with exactly three escapes in the value.
INSTANTIATE_TEST_SUITE_P(
    Labels, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"UnterminatedSet", "foo{a=\"b\" 1\n", 1, "expected ',' or '}'"},
        malformed{"UnterminatedAfterComma", "foo{a=\"b\", 1\n", 1, "invalid label name"}, malformed{"OpenBraceOnly", "foo{\n", 1, "unterminated label set"},
        malformed{"OpenBraceAndBlanks", "foo{   \n", 1, "unterminated label set"},
        malformed{"EndsAfterLabelValue", "foo{a=\"b\"\n", 1, "unterminated label set on 'foo'"},
        malformed{"EndsAfterLabelValueAndBlanks", "foo{a=\"b\"   \n", 1, "unterminated label set on 'foo'"},
        malformed{"MissingEquals", "foo{a \"b\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"NameOnlyLabel", "foo{a} 1\n", 1, "expected '=' after label 'a'"}, malformed{"MissingName", "foo{=\"b\"} 1\n", 1, "invalid label name"},
        malformed{"MissingValue", "foo{a=} 1\n", 1, "quoted value for label 'a'"}, malformed{"UnquotedValue", "foo{a=b} 1\n", 1, "quoted value for label 'a'"},
        malformed{"SingleQuotedValue", "foo{a='b'} 1\n", 1, "quoted value for label 'a'"},
        malformed{"LeadingDigitName", "foo{1a=\"b\"} 1\n", 1, "invalid label name"},
        malformed{"ColonInName", "foo{a:b=\"c\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"DashInName", "foo{a-b=\"c\"} 1\n", 1, "expected '=' after label 'a'"},
        malformed{"MissingSeparator", "foo{a=\"b\" c=\"d\"} 1\n", 1, "expected ',' or '}' after label 'a'"},
        malformed{"SemicolonSeparator", "foo{a=\"b\";c=\"d\"} 1\n", 1, "expected ',' or '}'"}, malformed{"LoneComma", "foo{,} 1\n", 1, "invalid label name"},
        malformed{"DoubleComma", "foo{a=\"b\",,c=\"d\"} 1\n", 1, "invalid label name"},
        malformed{"DuplicateLabel", "foo{a=\"b\",a=\"c\"} 1\n", 1, "label 'a' twice"},
        malformed{"DuplicateLabelSameValue", "foo{a=\"b\",z=\"y\",a=\"b\"} 1\n", 1, "label 'a' twice"},
        malformed{"DuplicateLabelPastTheLinearScan", "foo{a=\"1\",b=\"2\",c=\"3\",d=\"4\",e=\"5\",f=\"6\",g=\"7\",h=\"8\",i=\"9\",j=\"10\",c=\"11\"} 1\n", 1,
                  "label 'c' twice"},
        malformed{"EscapeTab", "foo{a=\"\\t\"} 1\n", 1, "badly escaped value for label 'a'"},
        malformed{"EscapeHex", "foo{a=\"\\x41\"} 1\n", 1, "badly escaped"}, malformed{"EscapeUnicode", "foo{a=\"\\u0041\"} 1\n", 1, "badly escaped"},
        malformed{"EscapeSingleQuote", "foo{a=\"\\'\"} 1\n", 1, "badly escaped"}, malformed{"BackslashAtEndOfLine", "foo{a=\"b\\\n", 1, "badly escaped"},
        malformed{"EscapedClosingQuote", "foo{a=\"b\\\"} 1\n", 1, "badly escaped"},
        malformed{"UnterminatedValue", "foo{a=\"b} 1\n", 1, "unterminated or badly escaped"},
        malformed{"ValueRunsIntoNextLine", "foo{a=\"b\nc\"} 1\n", 1, "unterminated or badly escaped"}),
    case_name);

// Values and timestamps: the decimal float grammar, or a non-finite word for a
// value; a finite decimal (OpenMetrics) or integer (text) for a timestamp.
INSTANTIATE_TEST_SUITE_P(
    Values, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"Word", "foo abc\n", 1, "invalid value 'abc' for 'foo'"}, malformed{"Unit", "foo 12ms\n", 1, "invalid value '12ms'"},
        malformed{"Hex", "foo 0x10\n", 1, "invalid value '0x10'"}, malformed{"HexFloat", "foo 0x1p3\n", 1, "invalid value"},
        malformed{"TwoDots", "foo 1.2.3\n", 1, "invalid value '1.2.3'"}, malformed{"DecimalComma", "foo 1,5\n", 1, "invalid value '1,5'"},
        malformed{"DigitSeparator", "foo 1_000\n", 1, "invalid value '1_000'"}, malformed{"DoubleSign", "foo --1\n", 1, "invalid value '--1'"},
        malformed{"SignOnly", "foo +\n", 1, "invalid value '+'"}, malformed{"DotOnly", "foo .\n", 1, "invalid value '.'"},
        malformed{"ExponentOnly", "foo e5\n", 1, "invalid value 'e5'"}, malformed{"EmptyExponent", "foo 1e\n", 1, "invalid value '1e'"},
        malformed{"SignedEmptyExponent", "foo 1e+\n", 1, "invalid value '1e+'"}, malformed{"FractionalExponent", "foo 1e2.5\n", 1, "invalid value"},
        malformed{"Overflow", "foo 1e400\n", 1, "invalid value '1e400'"}, malformed{"NegativeOverflow", "foo -1e400\n", 1, "invalid value '-1e400'"},
        malformed{"NanSuffix", "foo nanx\n", 1, "invalid value 'nanx'"}, malformed{"Infinite", "foo infinite\n", 1, "invalid value 'infinite'"},
        malformed{"DoubleSignInf", "foo ++Inf\n", 1, "invalid value '++Inf'"}, malformed{"BareCarriageReturn", "foo 1\r\r\n", 1, "invalid value"},
        malformed{"TimestampWord", "foo 1 soon\n", 1, "invalid timestamp 'soon'"},
        malformed{"TimestampWithUnit", "foo 1 1700000000s\n", 1, "invalid timestamp"}, malformed{"TimestampOverflow", "foo 1 1e400\n", 1, "invalid timestamp"},
        malformed{"TimestampNaN", "foo 1 NaN\n", 1, "invalid timestamp 'NaN'"}, malformed{"TimestampInf", "foo 1 +Inf\n", 1, "invalid timestamp '+Inf'"},
        malformed{"TimestampNegativeInf", "foo 1 -inf\n", 1, "invalid timestamp '-inf'"},
        malformed{"TimestampFractionalInText", "foo 1 1700000000.5\n", 1, "invalid timestamp", in::text_only},
        malformed{"TimestampExponentInText", "foo 1 17e11\n", 1, "invalid timestamp", in::text_only},
        malformed{"ExemplarInText", "foo 1 # {a=\"b\"} 1\n", 1, "unexpected text", in::text_only},
        malformed{"ThirdToken", "foo 1 2 3\n", 1, "unexpected text after the value of 'foo'"},
        malformed{"ThirdTokenAfterLabels", "foo{a=\"b\"} 1 2 x\n", 1, "unexpected text"}),
    case_name);

// Metadata lines and how they relate to the families around them.
INSTANTIATE_TEST_SUITE_P(
    Metadata, OpenmetricsParserMalformed,
    ::testing::Values(
        malformed{"UnknownType", "# TYPE foo sometimes\n", 1, "unknown type 'sometimes'"},
        malformed{"TypeCaseMatters", "# TYPE foo Counter\n", 1, "unknown type 'Counter'"}, malformed{"MissingType", "# TYPE foo\n", 1, "unknown type ''"},
        malformed{"UntypedInOpenMetrics", "# TYPE foo untyped\n", 1, "unknown type 'untyped'", in::openmetrics_only},
        malformed{"UnknownInText", "# TYPE foo unknown\n", 1, "unknown type 'unknown'", in::text_only},
        malformed{"InfoInText", "# TYPE foo info\n", 1, "unknown type 'info'", in::text_only},
        malformed{"StatesetInText", "# TYPE foo stateset\n", 1, "unknown type 'stateset'", in::text_only},
        malformed{"GaugehistogramInText", "# TYPE foo gaugehistogram\n", 1, "unknown type 'gaugehistogram'", in::text_only},
        malformed{"TypeTrailingText", "# TYPE foo gauge extra\n", 1, "unexpected text after the type of 'foo'"},
        malformed{"TypeWithoutName", "# TYPE\n", 1, "expected a metric name after '# TYPE'"},
        malformed{"TypeWithoutNameButBlank", "# TYPE \n", 1, "expected a metric name after '# TYPE'"},
        malformed{"HelpWithoutName", "# HELP\n", 1, "expected a metric name after '# HELP'"},
        malformed{"UnitWithoutName", "# UNIT\n", 1, "expected a metric name after '# UNIT'"},
        malformed{"NameLeadingDigit", "# UNIT 9foo bytes\n", 1, "expected a metric name"},
        malformed{"NameWithDot", "# TYPE foo.bar gauge\n", 1, "invalid character in metric name 'foo.bar'"},
        malformed{"UnitTrailingText", "# UNIT foo bytes extra\n", 1, "unexpected text after the unit of 'foo'"},
        malformed{"SecondType", "# TYPE foo gauge\n# TYPE foo gauge\n", 2, "second '# TYPE' line for 'foo'"},
        malformed{"SecondHelp", "# HELP foo a\n# HELP foo b\n", 2, "second '# HELP' line for 'foo'"},
        malformed{"SecondUnit", "# UNIT foo s\n# UNIT foo s\n", 2, "second '# UNIT' line for 'foo'"},
        malformed{"TypeAfterSamples", "foo 1\n# TYPE foo gauge\n", 2, "already declared or sampled"},
        malformed{"HelpAfterSamples", "# TYPE foo gauge\nfoo 1\n# HELP foo late\n", 3, "already declared or sampled", in::text_only},
        malformed{"FamilySplitInTwo", "# TYPE foo gauge\nfoo 1\n# TYPE bar gauge\nbar 1\n# HELP foo late\n", 5, "already declared or sampled", in::text_only},
        // In OpenMetrics a `# HELP` for a typed family may begin the second of
        // a same-name pair (see the pair tests). The lines after it decide, and
        // when they do not make it one the error is the late line itself,
        // worded as in the text format. A body that ends before they decide
        // reads as truncated.
        malformed{"HelpAfterSamplesThenSample", "# TYPE foo gauge\nfoo{a=\"1\"} 1\n# HELP foo late\nfoo{a=\"2\"} 2\n", 3, "already declared or sampled"},
        malformed{"HelpAfterSamplesThenEof", "# TYPE foo gauge\nfoo 1\n# HELP foo late\n# EOF\n", 3, "already declared or sampled", in::openmetrics_only},
        malformed{"FamilySplitInTwoThenNext", "# TYPE foo gauge\nfoo 1\n# TYPE bar gauge\nbar 1\n# HELP foo late\nbaz 1\n", 5, "already declared or sampled"},
        malformed{"BucketBeforeType", "lat_bucket{le=\"1\"} 1\n# TYPE lat histogram\n", 2, "'lat_bucket' came before the '# TYPE' line of 'lat'"},
        malformed{"CountBeforeType", "rpc_count 1\n# TYPE rpc summary\n", 2, "'rpc_count' came before the '# TYPE' line of 'rpc'"},
        malformed{"TotalBeforeType", "foo_total 1\n# TYPE foo counter\n", 2, "'foo_total' came before the '# TYPE' line of 'foo'", in::openmetrics_only},
        malformed{"InfoBeforeType", "build_info 1\n# TYPE build info\n", 2, "came before the '# TYPE' line", in::openmetrics_only},
        malformed{"MetadataForAnOwnedSample", "# TYPE rpc summary\nrpc_count 1\n# TYPE rpc_count gauge\n", 3, "'rpc_count' is a sample of the family 'rpc'"},
        malformed{"MetadataForACounterSample", "# TYPE foo counter\nfoo_total 1\n# HELP foo_total again\n", 3, "'foo_total' is a sample of the family 'foo'",
                  in::openmetrics_only},
        malformed{"BareSampleInHistogram", "# TYPE h histogram\nh_bucket{le=\"1\"} 1\nh 2\nh_count 1\n", 3, "'h' is not a sample of the histogram family 'h'"},
        malformed{"BareSampleInGaugehistogram", "# TYPE q gaugehistogram\nq 1\n", 2, "not a sample of the gaugehistogram family 'q'", in::openmetrics_only},
        malformed{"BareSampleInInfo", "# TYPE b info\nb 1\n", 2, "not a sample of the info family 'b'", in::openmetrics_only},
        malformed{"BareSampleInOpenMetricsCounter", "# TYPE c counter\nc 1\n", 2, "not a sample of the counter family 'c'", in::openmetrics_only}),
    case_name);

// `# EOF` ends an OpenMetrics document; nothing but blank lines may follow it.
INSTANTIATE_TEST_SUITE_P(Eof, OpenmetricsParserMalformed,
                         ::testing::Values(malformed{"SampleAfterEof", "foo 1\n# EOF\nbar 2\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"CommentAfterEof", "foo 1\n# EOF\n# comment\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"SecondEof", "foo 1\n# EOF\n# EOF\n", 3, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"MetadataAfterEof", "# EOF\n# TYPE foo gauge\n", 2, "text after '# EOF'", in::openmetrics_only},
                                           malformed{"EofTrailingText", "foo 1\n# EOF now\n", 2, "unexpected text after '# EOF'", in::openmetrics_only}),
                         case_name);

// A body that stops without its final line feed was cut off.
INSTANTIATE_TEST_SUITE_P(Truncation, OpenmetricsParserMalformed,
                         ::testing::Values(malformed{"MidValue", "# TYPE foo gauge\nfoo 1", 2, "middle of a line"},
                                           malformed{"MidLabelValue", "# TYPE foo gauge\nfoo{a=\"some", 2, "middle of a line"},
                                           malformed{"MidName", "foo 1\nfo", 2, "middle of a line"},
                                           malformed{"MidMetadata", "foo 1\n# HE", 2, "middle of a line"},
                                           malformed{"MidEof", "foo 1\n# EO", 2, "middle of a line"},
                                           malformed{"MidComment", "foo 1\n# just a comment", 2, "middle of a line"},
                                           malformed{"SingleLineNoFeed", "foo 1", 1, "middle of a line"},
                                           // A text body has no terminator: a last `# EOF` is a comment that may
                                           // have been longer.
                                           malformed{"TextCutAtEof", "foo 1\n# EOF", 2, "middle of a line", in::text_only},
                                           malformed{"TextCutAtBlanks", "foo 1\n   ", 2, "middle of a line", in::text_only},
                                           malformed{"OpenMetricsBlanksBeforeEof", "foo 1\n   ", 2, "middle of a line", in::openmetrics_only}),
                         case_name);

TEST(OpenmetricsParser, ErrorKeepsTheFamiliesReadBeforeIt) {
  const om::result parsed = om::parse("# TYPE good gauge\ngood 1\nbad{ 2\nlater 3\n", text);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 3u);
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families.at(0).name, "good");
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, FailedMetadataLineLeavesNoFamilyBehind) {
  // Each line is read in full before it may create a family, and a family
  // whose own metadata line fails before it has any samples is taken out, so
  // the families left after a failure are the ones read before it.
  for (const om::format f : {openmetrics, text}) {
    for (const char *tail : {"# TYPE foo sometimes\n", "# TYPE foo gauge extra\n", "# UNIT foo bytes extra\n", "# TYPE lat histogram\n",
                             "# HELP foo Help.\n# TYPE foo sometimes\n", "# HELP foo Help.\n# UNIT foo s\n# HELP foo again\n"}) {
      const om::result parsed = om::parse(std::string("good 1\nlat_bucket{le=\"1\"} 1\n") + tail, f);
      EXPECT_FALSE(parsed.ok()) << tail;
      ASSERT_EQ(parsed.families.size(), 2u) << tail;
      EXPECT_EQ(parsed.families.at(0).name, "good");
      EXPECT_EQ(parsed.families.at(1).name, "lat_bucket");
    }
  }
  // A family with samples is kept when a later line fails.
  const om::result sampled = om::parse("# TYPE foo gauge\nfoo 1\n# HELP foo late\n", text);
  EXPECT_FALSE(sampled.ok());
  EXPECT_EQ(sampled.families.size(), 1u);
  // So is a whole declaration when the line that fails names something else:
  // `foo.bar` and `fo.o` are not `foo`, whatever they start with.
  for (const om::format f : {openmetrics, text}) {
    for (const char *other : {"# HELP foo.bar x\n", "# TYPE fo.o gauge\n", "# TYPE foo.bar gauge\n"}) {
      const om::result parsed = om::parse(std::string("# HELP foo Help.\n# TYPE foo gauge\n") + other, f);
      EXPECT_FALSE(parsed.ok()) << other;
      ASSERT_EQ(parsed.families.size(), 1u) << other;
      EXPECT_EQ(parsed.families.at(0).name, "foo") << other;
      EXPECT_EQ(parsed.families.at(0).type, om::family_type::gauge) << other;
    }
  }
}

TEST(OpenmetricsParser, FailingSampleIsNotAddedToItsFamily) {
  // The label set is read before the duplicate is found; none of it may leak
  // into the family the sample would have joined.
  const om::result parsed = om::parse("# TYPE foo gauge\nfoo{a=\"1\"} 1\nfoo{a=\"2\",a=\"3\"} 2\n", text);
  EXPECT_FALSE(parsed.ok());
  ASSERT_EQ(parsed.families.size(), 1u);
  EXPECT_EQ(parsed.families.at(0).samples.size(), 1u);
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, ErrorLineCountsBlankCommentAndCrlfLines) {
  const om::result parsed = om::parse("# hello\r\n\r\n\nfoo 1\r\n   \nbar x\r\n", text);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 6u) << parsed.error;
}

TEST(OpenmetricsParser, OnlyTheFirstErrorIsReported) {
  const om::result parsed = om::parse("a 1\nb x\nc{ 3\n", text);
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("invalid value 'x'"), std::string::npos) << parsed.error;
}

TEST(OpenmetricsParser, EmptyBodyIsAnEmptyScrape) {
  for (const om::format f : {openmetrics, text}) {
    const om::result parsed = om::parse("", f);
    EXPECT_TRUE(parsed.ok());
    EXPECT_TRUE(parsed.families.empty());
    EXPECT_FALSE(parsed.saw_eof);
  }
}

// --- bounded input ------------------------------------------------------------

TEST(OpenmetricsParser, TruncatedAtALineBoundaryIsMissingOnlyItsEof) {
  // Nothing in an OpenMetrics body cut on a line boundary can tell it apart
  // from a whole one except the missing terminator, which the caller checks.
  const om::result parsed = om::parse("# TYPE foo gauge\nfoo 1\n", openmetrics);
  EXPECT_TRUE(parsed.ok());
  EXPECT_FALSE(parsed.saw_eof);
}

TEST(OpenmetricsParser, SixtyFourMegabyteLineIsReadInOnePass) {
  const std::size_t size = 64u * 1024u * 1024u;
  std::string body = "big{blob=\"";
  body.append(size, 'x');
  body += "\"} 1\n";
  const om::result parsed = om::parse(body, text, unlimited());
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(only_family(parsed).samples.at(0).labels.at(0).second.size(), size);
}

TEST(OpenmetricsParser, LineOverTheLimitIsRefusedUnread) {
  std::string body = "small 1\nbig{blob=\"";
  body.append(64u * 1024u * 1024u, 'x');
  body += "\"} 1\n";
  om::limits bounds;
  bounds.max_line_bytes = 1024 * 1024;
  const om::result parsed = om::parse(body, text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 2u);
  EXPECT_NE(parsed.error.find("longer than 1048576 bytes"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 1u);
}

TEST(OpenmetricsParser, LineOfAHundredThousandLabelsIsReadLinearly) {
  // A repeated label name is found by sorting the sample's label names and
  // comparing neighbours, so this is n log n rather than five billion
  // comparisons. The repeat at the end proves the check still runs.
  std::string body = "wide{";
  for (int i = 0; i < 100000; ++i) body += "l" + std::to_string(i) + "=\"v\",";
  body += "l0=\"again\"} 1\n";
  const om::result parsed = om::parse(body, text, unlimited());
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("label 'l0' twice"), std::string::npos) << parsed.error;

  const std::string accepted = body.substr(0, body.size() - std::string("l0=\"again\"} 1\n").size()) + "} 1\n";
  const om::result read = om::parse(accepted, text, unlimited());
  ASSERT_TRUE(read.ok()) << read.error;
  EXPECT_EQ(only_family(read).samples.at(0).labels.size(), 100000u);
}

TEST(OpenmetricsParser, WideLineDoesNotSlowTheLinesAfterIt) {
  // Nothing sized by one sample may outlive it: a structure that grew for a
  // 100k-label line and is then cleared per sample makes every later sample
  // pay for the wide one. Compared against the same short lines alone, with a
  // margin wide enough for a loaded CI machine - the regression this guards
  // against was a hundredfold.
  std::string wide = "wide{";
  for (int i = 0; i < 100000; ++i) wide += "l" + std::to_string(i) + "=\"v\",";
  wide += "} 1\n";
  std::string narrow;
  for (int i = 0; i < 200000; ++i) narrow += "a{x=\"" + std::to_string(i % 7) + "\",y=\"1\"} 1\n";
  const auto seconds = [](const std::string &body) {
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const om::result parsed = om::parse(body, text, unlimited());
    EXPECT_TRUE(parsed.ok()) << parsed.error;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  };
  const double alone = seconds(narrow) + seconds(wide);
  const double after = seconds(wide + narrow);
  EXPECT_LT(after, 4 * alone + 0.25) << "wide line alone and short lines alone: " << alone << "s, together: " << after << "s";
}

TEST(OpenmetricsParser, DefaultLimitsStopABodyBuiltToExhaustMemory) {
  // The bodies that cost the most per byte: single-sample families with short
  // names (up to fifty times the body retained, eighty at the peak), and short
  // labels (up to twenty-one, twenty-eight at the peak). The defaults stop each
  // long before it costs more than a few hundred MB.
  std::string families;
  for (int i = 0; i < 60000; ++i) families += "m" + std::to_string(i) + " 1\n";
  const om::result many = om::parse(families, text);
  EXPECT_FALSE(many.ok());
  EXPECT_NE(many.error.find("more than 50000 families"), std::string::npos) << many.error;

  std::string labelled;
  std::string set;
  for (int i = 0; i < 200; ++i) set += "l" + std::to_string(i) + "=\"\",";
  for (int i = 0; i < 13000; ++i) labelled += "m{" + set + "n=\"" + std::to_string(i) + "\"} 1\n";
  const om::result heavy = om::parse(labelled, text);
  EXPECT_FALSE(heavy.ok());
  EXPECT_NE(heavy.error.find("more than 2500000 labels"), std::string::npos) << heavy.error;

  std::string wide = "w{";
  for (int i = 0; i < 300; ++i) wide += "l" + std::to_string(i) + "=\"\",";
  const om::result too_wide = om::parse(wide + "} 1\n", text);
  EXPECT_NE(too_wide.error.find("more than 256 labels on 'w'"), std::string::npos) << too_wide.error;
}

TEST(OpenmetricsParser, InterleavedTextFamiliesAreReadInLinearTime) {
  // Regrouping a sample into a family read earlier must cost what appending
  // to the family being read costs, however many families there are to go
  // back to: two families alternating line by line, and 10,000 families taken
  // round-robin, each against the same lines grouped. A per-switch
  // reallocation made the first quadratic, a per-switch scan of the families
  // would make the second so. Each body is timed warm, best of three, so the
  // order they run in does not favour either.
  const auto seconds = [](const std::string &body, const std::size_t families) {
    double best = 0;
    for (int round = 0; round < 3; ++round) {
      const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
      const om::result parsed = om::parse(body, text);
      const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      EXPECT_TRUE(parsed.ok()) << parsed.error;
      EXPECT_EQ(parsed.families.size(), families);
      if (round == 0 || took < best) best = took;
    }
    return best;
  };
  struct shape {
    std::size_t families;
    std::size_t per_family;
  };
  // Linear, each body parses in milliseconds; quadratic, in seconds - large
  // enough to fail clearly, small enough not to stall CI when it does.
  for (const shape &sh : {shape{2, 20000}, shape{10000, 20}}) {
    std::string interleaved;
    std::string grouped;
    for (std::size_t i = 0; i < sh.per_family; ++i) {
      for (std::size_t f = 0; f < sh.families; ++f) interleaved += "f" + std::to_string(f) + "{i=\"" + std::to_string(i) + "\"} 1\n";
    }
    for (std::size_t f = 0; f < sh.families; ++f) {
      for (std::size_t i = 0; i < sh.per_family; ++i) grouped += "f" + std::to_string(f) + "{i=\"" + std::to_string(i) + "\"} 1\n";
    }
    const double together = seconds(grouped, sh.families);
    const double apart = seconds(interleaved, sh.families);
    EXPECT_LT(apart, 4 * together + 0.25) << sh.families << " families: grouped " << together << "s, interleaved " << apart << "s";
  }
}

TEST(OpenmetricsParser, LabelsCountAgainstTheirLimits) {
  om::limits per_sample;
  per_sample.max_labels_per_sample = 3;
  const om::result wide = om::parse("ok{a=\"1\",b=\"2\",c=\"3\"} 1\nwide{a=\"1\",b=\"2\",c=\"3\",d=\"4\"} 1\n", text, per_sample);
  EXPECT_FALSE(wide.ok());
  EXPECT_EQ(wide.error_line, 2u);
  EXPECT_NE(wide.error.find("more than 3 labels on 'wide'"), std::string::npos) << wide.error;

  // The total is what bounds a body of short label-heavy lines, each within
  // the per-sample limit.
  om::limits total;
  total.max_labels = 5;
  const om::result many = om::parse("a{x=\"1\",y=\"2\"} 1\na{x=\"2\",y=\"2\"} 1\na{x=\"3\",y=\"2\"} 1\n", text, total);
  EXPECT_FALSE(many.ok());
  EXPECT_EQ(many.error_line, 3u);
  EXPECT_NE(many.error.find("more than 5 labels"), std::string::npos) << many.error;
  EXPECT_EQ(many.sample_count, 2u);
}

namespace {

std::string many_series(const std::size_t count) {
  std::string body = "# HELP series_value One of many.\n# TYPE series_value gauge\n";
  for (std::size_t i = 0; i < count; ++i) {
    body += "series_value{instance=\"" + std::to_string(i) + "\",job=\"bench\"} " + std::to_string(i) + "\n";
  }
  body += "# EOF\n";
  return body;
}

}  // namespace

TEST(OpenmetricsParser, HundredThousandSeriesParse) {
  const om::result parsed = om::parse(many_series(100000), openmetrics);
  ASSERT_TRUE(parsed.ok()) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 100000u);
  const om::family &f = only_family(parsed);
  EXPECT_EQ(f.samples.size(), 100000u);
  EXPECT_DOUBLE_EQ(f.samples.at(99999).value, 99999);
}

TEST(OpenmetricsParser, SeriesOverTheLimitStopTheParse) {
  om::limits bounds;
  bounds.max_series = 50000;
  const om::result parsed = om::parse(many_series(100000), openmetrics, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("more than 50000 series"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.sample_count, 50000u);
  // Two metadata lines, then the 50001st sample.
  EXPECT_EQ(parsed.error_line, 50003u);
}

TEST(OpenmetricsParser, MetadataOnlyFamiliesCountAgainstTheFamilyLimit) {
  // A body of nothing but `# TYPE` lines has no samples for `max_series` to
  // count, and would otherwise grow the result without bound.
  std::string body;
  for (int i = 0; i < 100000; ++i) body += "# TYPE m" + std::to_string(i) + " gauge\n";
  om::limits bounds;
  bounds.max_series = 10;
  bounds.max_families = 1000;
  const om::result parsed = om::parse(body, text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_NE(parsed.error.find("more than 1000 families"), std::string::npos) << parsed.error;
  EXPECT_EQ(parsed.error_line, 1001u);
  EXPECT_EQ(parsed.families.size(), 1000u);
}

TEST(OpenmetricsParser, SampledFamiliesCountAgainstTheFamilyLimit) {
  om::limits bounds;
  bounds.max_families = 2;
  const om::result parsed = om::parse("a 1\na 2\nb 1\nc 1\n", text, bounds);
  EXPECT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error_line, 4u);
  EXPECT_EQ(parsed.families.size(), 2u);
  EXPECT_EQ(parsed.sample_count, 3u);
}

// --- hostile input ------------------------------------------------------------
//
// The cases above pin what the parser says about inputs someone thought of.
// These pin what it does with inputs nobody did: whatever the bytes, it must
// return, and what it returns must be internally consistent - which is what
// every caller of `parse()` relies on without checking.

namespace {

// What a result promises regardless of input.
// The sample suffixes a family of this type owns, as the formats define them -
// restated here rather than shared with the parser, so that a parser that
// drifts from the specifications is caught rather than agreed with.
bool owned_suffix(const om::format f, const om::family_type type, const std::string &suffix) {
  if (f == text) {
    if (type == om::family_type::histogram) return suffix == "_bucket" || suffix == "_sum" || suffix == "_count";
    if (type == om::family_type::summary) return suffix.empty() || suffix == "_sum" || suffix == "_count";
    return suffix.empty();
  }
  switch (type) {
    case om::family_type::counter:
      return suffix == "_total" || suffix == "_created";
    case om::family_type::histogram:
      return suffix == "_bucket" || suffix == "_sum" || suffix == "_count" || suffix == "_created";
    case om::family_type::gaugehistogram:
      return suffix == "_bucket" || suffix == "_gsum" || suffix == "_gcount";
    case om::family_type::summary:
      return suffix.empty() || suffix == "_sum" || suffix == "_count" || suffix == "_created";
    case om::family_type::info:
      return suffix == "_info";
    default:
      return suffix.empty();
  }
}

void expect_consistent(const om::result &parsed, const std::string &body, const om::format f) {
  std::size_t lines = 1;
  for (const char c : body) {
    if (c == '\n') ++lines;
  }
  EXPECT_EQ(parsed.ok(), parsed.error.empty());
  if (parsed.ok()) {
    EXPECT_EQ(parsed.error_line, 0u);
  } else {
    EXPECT_GE(parsed.error_line, 1u);
    EXPECT_LE(parsed.error_line, lines);
  }
  // Anything keyed on the family - the scraper's republication, a check's
  // `name` and `type` keywords - relies on one family per name and type, on a
  // name shared only by the pair client_golang writes, and on a sample name
  // meaning one family, `X_created` of that pair aside.
  std::map<std::string, std::vector<const om::family *> > by_name;
  for (const om::family &family : parsed.families) {
    EXPECT_FALSE(family.name.empty());
    by_name[family.name].push_back(&family);
  }
  for (const std::pair<const std::string, std::vector<const om::family *> > &named : by_name) {
    if (named.second.size() == 1) continue;
    EXPECT_EQ(f, openmetrics) << "family '" << named.first << "' twice";
    ASSERT_EQ(named.second.size(), 2u) << "family '" << named.first << "' " << named.second.size() << " times";
    const bool first_counter = named.second.at(0)->type == om::family_type::counter;
    const bool second_counter = named.second.at(1)->type == om::family_type::counter;
    EXPECT_NE(first_counter, second_counter) << "'" << named.first << "' shared by a " << om::type_name(named.second.at(0)->type) << " and a "
                                             << om::type_name(named.second.at(1)->type);
  }
  std::map<std::string, const om::family *> sample_owner;
  std::size_t samples = 0;
  for (const om::family &family : parsed.families) {
    for (const om::sample &s : family.samples) {
      ASSERT_EQ(s.name.compare(0, family.name.size(), family.name), 0) << s.name << " in " << family.name;
      EXPECT_TRUE(owned_suffix(f, family.type, s.name.substr(family.name.size())))
          << "'" << s.name << "' in the " << om::type_name(family.type) << " family '" << family.name << "'";
      const std::map<std::string, const om::family *>::const_iterator seen = sample_owner.find(s.name);
      if (seen == sample_owner.end()) {
        sample_owner[s.name] = &family;
      } else if (seen->second != &family) {
        EXPECT_TRUE(seen->second->name == family.name && s.name == family.name + "_created") << "sample '" << s.name << "' in two families";
      }
      if (s.timestamp.has_value()) {
        EXPECT_TRUE(std::isfinite(s.timestamp.value())) << s.name;
      }
    }
    samples += family.samples.size();
  }
  EXPECT_EQ(samples, parsed.sample_count);
}

const char *const exposition =
    "# HELP http_requests Requests served.\n"
    "# TYPE http_requests counter\n"
    "http_requests_total{code=\"200\",path=\"/a \\\"b\\\"\"} 1027 1700000000\n"
    "http_requests_created{code=\"200\",path=\"/a \\\"b\\\"\"} 1.7e9\n"
    "# TYPE latency_seconds histogram\n"
    "# UNIT latency_seconds seconds\n"
    "latency_seconds_bucket{le=\"0.1\"} 3 # {trace_id=\"x\"} 0.05\n"
    "latency_seconds_bucket{le=\"+Inf\"} 8\n"
    "latency_seconds_sum 4.25\n"
    "latency_seconds_count 8\n"
    "# TYPE build info\n"
    "build_info{version=\"1\\n2\\\\3\"} 1\n"
    "untyped_thing NaN\n"
    "# EOF\n";

// A body that is valid in both formats, for the tests that run both.
const char *const shared_exposition =
    "# HELP h Latency.\n"
    "# TYPE h histogram\n"
    "h_bucket{le=\"1\"} 3\n"
    "h_bucket{le=\"+Inf\"} 4\n"
    "h_sum 2.5\n"
    "h_count 4\n"
    "# TYPE s summary\n"
    "s{quantile=\"0.5\"} 1\n"
    "s_sum 9\n"
    "s_count 3\n"
    "g{a=\"x\"} 1 1700000000\n"
    "g{a=\"y\"} -Inf\n";

}  // namespace

TEST(OpenmetricsParser, EveryTruncationIsRefusedOrEndsOnALine) {
  // Cut a valid exposition at every byte. A prefix that ends on a line feed is
  // a shorter valid exposition; any other prefix was cut mid-line and must be
  // refused - never read as a sample that happens to parse. Including inside
  // the block of a same-name pair, which is read tentatively.
  const std::string pair_body =
      "# TYPE x gauge\nx 1\n# HELP x Total.\n# TYPE x counter\n# UNIT x bytes\nx_total 2\nx_created 1.7e9\n"
      "# TYPE h histogram\nh_bucket{le=\"+Inf\"} 1\nh_sum 1\nh_count 1\nh_created 1\n# TYPE h counter\nh_total 1\nh_created 2\n# EOF\n";
  for (const std::string &body : {std::string(exposition), std::string(client_golang_openmetrics), pair_body}) {
    ASSERT_TRUE(om::parse(body, openmetrics).ok());
    for (std::size_t length = 0; length < body.size(); ++length) {
      const std::string prefix = body.substr(0, length);
      const om::result parsed = om::parse(prefix, openmetrics);
      expect_consistent(parsed, prefix, openmetrics);
      const bool bare_eof = length == body.size() - 1;
      const bool on_a_line = prefix.empty() || prefix[prefix.size() - 1] == '\n' || bare_eof;
      if (on_a_line) {
        EXPECT_TRUE(parsed.ok()) << "cut at " << length << ": " << parsed.error << "\n" << prefix;
      } else {
        EXPECT_FALSE(parsed.ok()) << "cut at " << length << " was read:\n" << prefix;
        EXPECT_NE(parsed.error.find("middle of a line"), std::string::npos) << "cut at " << length << ": " << parsed.error;
      }
      EXPECT_EQ(parsed.saw_eof, bare_eof) << "cut at " << length;
    }
  }
}

TEST(OpenmetricsParser, EverySingleByteCorruptionIsHandled) {
  // Overwrite every byte with each character the grammar gives a meaning to,
  // and a few it does not. Most results are errors; all must be consistent.
  const char replacements[] = {'"', '\\', '{', '}', '=', ',', ' ', '\t', '#', '\n', '\r', '\0', '\x7f', '\xff', 'x', '1', '.', 'e', '-', '+'};
  const std::pair<const char *, om::format> bodies[] = {{exposition, openmetrics}, {shared_exposition, openmetrics}, {shared_exposition, text}};
  for (const std::pair<const char *, om::format> &entry : bodies) {
    const std::string body = entry.first;
    ASSERT_TRUE(om::parse(body, entry.second).ok());
    std::size_t failures = 0;
    for (std::size_t at = 0; at < body.size(); ++at) {
      for (const char replacement : replacements) {
        std::string corrupt = body;
        corrupt[at] = replacement;
        const om::result parsed = om::parse(corrupt, entry.second);
        expect_consistent(parsed, corrupt, entry.second);
        if (!parsed.ok()) ++failures;
      }
    }
    // Not a precise number - only proof that the corruption is being noticed
    // rather than read through.
    EXPECT_GT(failures, body.size());
  }
}

TEST(OpenmetricsParser, RandomBytesAreHandled) {
  // Short random documents drawn mostly from the grammar's own characters, so
  // they get past the first token often enough to reach the deeper states -
  // including the names that clash (`h`, `h_bucket`, `h_total`). Fixed seed: a
  // failure here reproduces.
  const std::string alphabet = "h_{}=,\"\\ \t#\n\r.019eE+-InfNa\xff";
  const char *const fragments[] = {"# TYPE h histogram\n",
                                   "# TYPE h counter\n",
                                   "# TYPE h summary\n",
                                   "# TYPE h gauge\n",
                                   "# TYPE h_total gauge\n",
                                   "# EOF\n",
                                   "h_bucket{le=\"1\"} 1\n",
                                   "h_total 1\n",
                                   "h 1\n",
                                   "h_count 2\n",
                                   "h_created 1\n",
                                   "h_sum 1\n",
                                   "# HELP h x\n",
                                   "# UNIT h s\n",
                                   "h{quantile=\"0.5\"} 1\n"};
  std::mt19937 random(1499);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> pick_fragment(0, sizeof(fragments) / sizeof(fragments[0]) - 1);
  std::uniform_int_distribution<std::size_t> length(0, 12);
  for (int round = 0; round < 20000; ++round) {
    std::string body;
    const std::size_t pieces = length(random);
    for (std::size_t i = 0; i < pieces; ++i) {
      if (random() % 2 == 0) {
        body += fragments[pick_fragment(random)];
      } else {
        for (std::size_t j = 0; j < 8; ++j) body += alphabet[pick(random)];
      }
    }
    for (const om::format f : {openmetrics, text}) {
      const om::result parsed = om::parse(body, f);
      expect_consistent(parsed, body, f);
    }
    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << "round " << round << " body: " << body;
      return;
    }
  }
}

TEST(OpenmetricsParser, AnythingAfterEofNeverChangesTheFamilies) {
  // Random complete documents, each with random lines appended after its
  // `# EOF` - fragments of the grammar, metadata naming the families the
  // document declared, over-long lines. The appended lines are an error, but
  // the families must be exactly those of the document alone.
  const char *const fragments[] = {"# TYPE h histogram\n",
                                   "# TYPE h counter\n",
                                   "# TYPE h gauge\n",
                                   "# TYPE g gauge\n",
                                   "# EOF\n",
                                   "h_bucket{le=\"1\"} 1\n",
                                   "h_total 1\n",
                                   "h 1\n",
                                   "g 2\n",
                                   "h_count 2\n",
                                   "h_sum 1\n",
                                   "# HELP h x\n",
                                   "# HELP g x\n",
                                   "# UNIT h s\n",
                                   "\n",
                                   "# HELP h aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n",
                                   "# HELP g aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n",
                                   "# a comment\n",
                                   "h_total{a=\"bbbbbbbbbbbbbbbbbbbb\"} 1\n"};
  const std::size_t fragment_count = sizeof(fragments) / sizeof(fragments[0]);
  om::limits bounds;
  bounds.max_line_bytes = 24;
  std::mt19937 random(1631);
  std::uniform_int_distribution<std::size_t> pick_fragment(0, fragment_count - 1);
  std::uniform_int_distribution<std::size_t> length(0, 6);
  std::size_t complete = 0;
  for (int round = 0; round < 40000; ++round) {
    std::string document;
    const std::size_t pieces = length(random);
    for (std::size_t i = 0; i < pieces; ++i) document += fragments[pick_fragment(random)];
    document += "# EOF\n";
    const om::result alone = om::parse(document, openmetrics, bounds);
    if (!alone.ok()) continue;
    ++complete;
    std::string tail;
    const std::size_t extra = 1 + length(random);
    for (std::size_t i = 0; i < extra; ++i) tail += fragments[pick_fragment(random)];
    const om::result appended = om::parse(document + tail, openmetrics, bounds);
    EXPECT_TRUE(appended.saw_eof);
    ASSERT_EQ(appended.families.size(), alone.families.size()) << document << "--- appended:\n" << tail << " -> " << appended.error;
    for (std::size_t i = 0; i < alone.families.size(); ++i) {
      const om::family &want = alone.families.at(i);
      const om::family &got = appended.families.at(i);
      EXPECT_EQ(got.name, want.name) << document << "--- appended:\n" << tail;
      EXPECT_EQ(got.type, want.type) << document << "--- appended:\n" << tail;
      EXPECT_EQ(got.help, want.help) << document << "--- appended:\n" << tail;
      EXPECT_EQ(got.unit, want.unit) << document << "--- appended:\n" << tail;
      EXPECT_EQ(got.samples.size(), want.samples.size()) << document << "--- appended:\n" << tail;
    }
    if (::testing::Test::HasFailure()) return;
  }
  // The generator has to produce enough complete documents to mean anything.
  EXPECT_GT(complete, 5000u);
}

TEST(OpenmetricsParser, RandomLinesSplicedIntoAValidBodyAreHandled) {
  // A valid body with a random line after it: the family before the splice
  // must survive whatever the spliced line does.
  const std::string alphabet = "abc_:{}=,\"\\ \t#.0123456789eE+-";
  std::mt19937 random(1623);
  std::uniform_int_distribution<std::size_t> pick(0, alphabet.size() - 1);
  std::uniform_int_distribution<std::size_t> length(1, 40);
  for (int round = 0; round < 5000; ++round) {
    std::string line;
    const std::size_t size = length(random);
    for (std::size_t i = 0; i < size; ++i) line += alphabet[pick(random)];
    const std::string body = std::string("# TYPE kept gauge\nkept 1\n") + line + "\n";
    for (const om::format f : {openmetrics, text}) {
      const om::result parsed = om::parse(body, f);
      expect_consistent(parsed, body, f);
      ASSERT_FALSE(parsed.families.empty()) << line;
      EXPECT_EQ(parsed.families.at(0).name, "kept") << line;
      EXPECT_GE(parsed.families.at(0).samples.size(), 1u) << line;
      if (!parsed.ok()) {
        EXPECT_EQ(parsed.error_line, 3u) << line << " -> " << parsed.error;
      }
    }
  }
}
