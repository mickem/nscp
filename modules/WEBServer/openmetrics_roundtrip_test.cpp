// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The agent's own exposition, read back through the parser CheckOpenMetrics
// scrapes exporters with.
//
// The renderer and the parser are the two halves of one format, written to
// different specifications' corners: the renderer strips `_total` and `_info`
// off a family name before putting it back on the sample, names the sample in
// the Prometheus text dialect's metadata lines and the family in OpenMetrics',
// and escapes label values and help text. Every one of those is a place where
// the parser has to undo exactly what the renderer did, so the snapshot below
// carries one of each, and the tests assert that families, types, help, units,
// labels and values all survive the trip - in both dialects.

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <metrics/openmetrics_parser.hpp>
#include <string>
#include <vector>

#include "openmetrics_renderer.hpp"

namespace om = metrics::openmetrics;

namespace {

PB::Metrics::Metric *add(PB::Metrics::MetricsBundle *b, const std::string &key, const std::string &help = "", const std::string &unit = "") {
  PB::Metrics::Metric *m = b->add_value();
  m->set_key(key);
  if (!help.empty()) m->set_desc(help);
  if (!unit.empty()) m->set_unit(unit);
  return m;
}

void label(PB::Metrics::Metric *m, const std::string &name, const std::string &value) {
  PB::Common::KeyValue *dim = m->add_dims();
  dim->set_key(name);
  dim->set_value(value);
}

// One of everything the renderer has a rule for.
PB::Metrics::MetricsMessage snapshot() {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *system = message.add_payload()->add_bundles();
  system->set_key("system");

  // A gauge with help and a unit the name gains as a suffix.
  PB::Metrics::MetricsBundle *mem = system->add_children();
  mem->set_key("mem");
  add(mem, "physical.used", "Physical memory in use", "bytes")->mutable_gauge_value()->set_value(16554000000.0);

  // A counter, and one whose key already ends in `_total`.
  add(system, "jobs.run", "Jobs run since start")->mutable_counter_value()->set_value(42);
  add(system, "requests_total", "Requests served")->mutable_counter_value()->set_value(1027);

  // An untyped value.
  add(system, "legacy")->mutable_untyped_value()->set_value(7);

  // One family, two series, labels instead of the instance in the name.
  PB::Metrics::MetricsBundle *cpu = system->add_children();
  cpu->set_key("cpu");
  cpu->set_desc("Idle time per core");
  for (const char *core : {"0", "1"}) {
    PB::Metrics::Metric *m = add(cpu, std::string("core ") + core + ".idle", "", "percent");
    m->set_alias("idle");
    label(m, "core", core);
    m->mutable_gauge_value()->set_value(core[0] == '0' ? 97.5 : 12.25);
  }

  // A label value and help text that need every escape.
  PB::Metrics::MetricsBundle *disk = system->add_children();
  disk->set_key("disk");
  PB::Metrics::Metric *volume = add(disk, "free", "Free space\non a \\ volume", "bytes");
  label(volume, "path", "\\Device\\HarddiskVolume1 \"system\"\nreserved");
  volume->mutable_gauge_value()->set_value(1.5);

  // The non-finite values have their own spelling.
  PB::Metrics::MetricsBundle *odd = system->add_children();
  odd->set_key("odd");
  add(odd, "nan")->mutable_gauge_value()->set_value(std::numeric_limits<double>::quiet_NaN());
  add(odd, "inf")->mutable_gauge_value()->set_value(std::numeric_limits<double>::infinity());
  add(odd, "neg_inf")->mutable_gauge_value()->set_value(-std::numeric_limits<double>::infinity());

  // Strings become the labels of the bundle's info family.
  add(system, "hostname", "What this host is")->mutable_string_value()->set_value("web-01");
  add(system, "os")->mutable_string_value()->set_value("linux");

  // A summary and a histogram, carried natively.
  PB::Metrics::Summary *summary = add(system, "rpc.duration", "RPC latency", "seconds")->mutable_summary_value();
  summary->set_sample_count(400);
  summary->set_sample_sum(120);
  PB::Metrics::Quantile *q = summary->add_quantile();
  q->set_quantile(0.5);
  q->set_value(0.2);
  q = summary->add_quantile();
  q->set_quantile(0.99);
  q->set_value(1.4);

  PB::Metrics::Histogram *histogram = add(system, "latency", "Request latency", "seconds")->mutable_histogram_value();
  histogram->set_sample_count(8);
  histogram->set_sample_sum(4.25);
  const double bounds[] = {0.1, 1, std::numeric_limits<double>::infinity()};
  const unsigned counts[] = {3, 7, 8};
  for (int i = 0; i < 3; ++i) {
    PB::Metrics::Bucket *b = histogram->add_bucket();
    b->set_upper_bound(bounds[i]);
    b->set_cumulative_count(counts[i]);
  }
  return message;
}

om::result parse(const openmetrics::dialect dialect) {
  std::vector<std::string> problems;
  const std::string body = openmetrics::render(snapshot(), dialect, &problems);
  EXPECT_TRUE(problems.empty()) << problems.front();
  om::result parsed = om::parse(body);
  EXPECT_TRUE(parsed.ok()) << parsed.error << " on line " << parsed.error_line << " of\n" << body;
  return parsed;
}

const om::family *find(const om::result &parsed, const std::string &name) {
  for (const om::family &f : parsed.families) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

const om::family &family(const om::result &parsed, const std::string &name) {
  const om::family *f = find(parsed, name);
  if (f == nullptr) {
    ADD_FAILURE() << "no family '" << name << "'";
    static const om::family none;
    return none;
  }
  return *f;
}

om::label_list labels(std::initializer_list<std::pair<std::string, std::string> > items) { return om::label_list(items); }

}  // namespace

TEST(OpenmetricsRoundTrip, OpenMetricsBodyEndsWithEof) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  EXPECT_TRUE(parsed.saw_eof);
  EXPECT_EQ(parsed.families.size(), 12u);
}

TEST(OpenmetricsRoundTrip, GaugeKeepsHelpUnitAndValue) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system_mem_physical_used_bytes");
  EXPECT_EQ(f.type, om::family_type::gauge);
  EXPECT_EQ(f.help, "Physical memory in use");
  EXPECT_EQ(f.unit, "bytes");
  ASSERT_EQ(f.samples.size(), 1u);
  // The renderer writes the shortest exact form, so nothing is lost.
  EXPECT_EQ(f.samples[0].value, 16554000000.0);
}

TEST(OpenmetricsRoundTrip, CountersFoldBackOntoTheirFamily) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &jobs = family(parsed, "system_jobs_run");
  EXPECT_EQ(jobs.type, om::family_type::counter);
  ASSERT_EQ(jobs.samples.size(), 1u);
  EXPECT_EQ(jobs.samples[0].name, "system_jobs_run_total");
  EXPECT_EQ(jobs.samples[0].value, 42);

  // The renderer took `_total` off the family name; the parser must not see
  // `system_requests_total` as the family.
  const om::family &requests = family(parsed, "system_requests");
  EXPECT_EQ(requests.type, om::family_type::counter);
  EXPECT_EQ(requests.help, "Requests served");
  ASSERT_EQ(requests.samples.size(), 1u);
  EXPECT_EQ(requests.samples[0].name, "system_requests_total");
  EXPECT_EQ(find(parsed, "system_requests_total"), nullptr);
}

TEST(OpenmetricsRoundTrip, UntypedIsUnknown) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  EXPECT_EQ(family(parsed, "system_legacy").type, om::family_type::unknown);
}

TEST(OpenmetricsRoundTrip, LabelledSeriesShareOneFamily) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system_cpu_idle_percent");
  EXPECT_EQ(f.help, "Idle time per core");
  EXPECT_EQ(f.unit, "percent");
  ASSERT_EQ(f.samples.size(), 2u);
  EXPECT_EQ(f.samples[0].labels, labels({{"core", "0"}}));
  EXPECT_EQ(f.samples[0].value, 97.5);
  EXPECT_EQ(f.samples[1].labels, labels({{"core", "1"}}));
  EXPECT_EQ(f.samples[1].value, 12.25);
}

TEST(OpenmetricsRoundTrip, EscapedLabelValuesAndHelpComeBackVerbatim) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system_disk_free_bytes");
  EXPECT_EQ(f.help, "Free space\non a \\ volume");
  ASSERT_EQ(f.samples.size(), 1u);
  EXPECT_EQ(f.samples[0].labels, labels({{"path", "\\Device\\HarddiskVolume1 \"system\"\nreserved"}}));
}

TEST(OpenmetricsRoundTrip, NonFiniteValuesSurvive) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  EXPECT_TRUE(std::isnan(family(parsed, "system_odd_nan").samples.at(0).value));
  EXPECT_EQ(family(parsed, "system_odd_inf").samples.at(0).value, std::numeric_limits<double>::infinity());
  EXPECT_EQ(family(parsed, "system_odd_neg_inf").samples.at(0).value, -std::numeric_limits<double>::infinity());
}

TEST(OpenmetricsRoundTrip, StringsComeBackAsAnInfoFamily) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system");
  EXPECT_EQ(f.type, om::family_type::info);
  EXPECT_EQ(f.help, "What this host is");
  ASSERT_EQ(f.samples.size(), 1u);
  EXPECT_EQ(f.samples[0].name, "system_info");
  EXPECT_EQ(f.samples[0].labels, labels({{"hostname", "web-01"}, {"os", "linux"}}));
  EXPECT_EQ(f.samples[0].value, 1);
}

TEST(OpenmetricsRoundTrip, SummaryKeepsQuantilesSumAndCount) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system_rpc_duration_seconds");
  EXPECT_EQ(f.type, om::family_type::summary);
  EXPECT_EQ(f.unit, "seconds");
  ASSERT_EQ(f.samples.size(), 4u);
  EXPECT_EQ(om::find_label(f.samples[0], "quantile").value(), "0.5");
  EXPECT_EQ(f.samples[0].value, 0.2);
  EXPECT_EQ(om::find_label(f.samples[1], "quantile").value(), "0.99");
  EXPECT_EQ(f.samples[1].value, 1.4);
  EXPECT_EQ(f.samples[2].name, "system_rpc_duration_seconds_sum");
  EXPECT_EQ(f.samples[2].value, 120);
  EXPECT_EQ(f.samples[3].name, "system_rpc_duration_seconds_count");
  EXPECT_EQ(f.samples[3].value, 400);
}

TEST(OpenmetricsRoundTrip, HistogramKeepsBucketsSumAndCount) {
  const om::result parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::family &f = family(parsed, "system_latency_seconds");
  EXPECT_EQ(f.type, om::family_type::histogram);
  EXPECT_EQ(f.help, "Request latency");
  ASSERT_EQ(f.samples.size(), 5u);
  EXPECT_EQ(f.samples[0].name, "system_latency_seconds_bucket");
  EXPECT_EQ(om::find_label(f.samples[0], "le").value(), "0.1");
  EXPECT_EQ(f.samples[0].value, 3);
  EXPECT_EQ(om::find_label(f.samples[2], "le").value(), "+Inf");
  EXPECT_EQ(f.samples[2].value, 8);
  EXPECT_EQ(f.samples[3].value, 4.25);
  EXPECT_EQ(f.samples[4].value, 8);
}

TEST(OpenmetricsRoundTrip, PrometheusTextDialectLandsOnTheSameFamilies) {
  // The older dialect names the sample in every metadata line and has no
  // `info` type, so the info family arrives as the gauge every exporter
  // served before the type existed. Everything else is the same family.
  const om::result om_parsed = parse(openmetrics::dialect::openmetrics_1_0);
  const om::result text_parsed = parse(openmetrics::dialect::prometheus_text_0_0_4);
  EXPECT_TRUE(text_parsed.saw_eof);
  ASSERT_EQ(text_parsed.families.size(), om_parsed.families.size());
  for (std::size_t i = 0; i < om_parsed.families.size(); ++i) {
    const om::family &a = om_parsed.families[i];
    const om::family &b = text_parsed.families[i];
    if (a.type == om::family_type::info) {
      EXPECT_EQ(b.name, a.name + "_info");
      EXPECT_EQ(b.type, om::family_type::gauge);
    } else {
      EXPECT_EQ(b.name, a.name);
      EXPECT_EQ(b.type, a.type) << a.name;
    }
    EXPECT_EQ(b.help, a.help) << a.name;
    EXPECT_EQ(b.unit, a.unit) << a.name;
    ASSERT_EQ(b.samples.size(), a.samples.size()) << a.name;
    for (std::size_t s = 0; s < a.samples.size(); ++s) {
      EXPECT_EQ(b.samples[s].name, a.samples[s].name);
      EXPECT_EQ(b.samples[s].labels, a.samples[s].labels);
      if (std::isnan(a.samples[s].value)) {
        EXPECT_TRUE(std::isnan(b.samples[s].value));
      } else {
        EXPECT_EQ(b.samples[s].value, a.samples[s].value) << a.samples[s].name;
      }
    }
  }
}

TEST(OpenmetricsRoundTrip, EveryTruncationOfOurOwnBodyIsRefusedOrEndsOnALine) {
  // The body the agent actually serves, cut where a size cap or a dropped
  // connection would cut it: a prefix ending mid-line is never read, and one
  // ending on a line feed reads as the families it got through - never a
  // family with more samples than the full body had.
  for (const openmetrics::dialect dialect : {openmetrics::dialect::openmetrics_1_0, openmetrics::dialect::prometheus_text_0_0_4}) {
    const std::string body = openmetrics::render(snapshot(), dialect);
    const om::result full = om::parse(body);
    ASSERT_TRUE(full.ok()) << full.error;
    for (std::size_t length = 0; length < body.size(); ++length) {
      const om::result parsed = om::parse(body.substr(0, length));
      // `# EOF` without its line feed is the one unterminated line accepted.
      const bool bare_eof = length == body.size() - 1;
      const bool on_a_line = length == 0 || body[length - 1] == '\n' || bare_eof;
      EXPECT_EQ(parsed.ok(), on_a_line) << "cut at " << length << ": " << parsed.error;
      EXPECT_EQ(parsed.saw_eof, bare_eof) << "cut at " << length;
      EXPECT_LE(parsed.sample_count, full.sample_count);
    }
  }
}
