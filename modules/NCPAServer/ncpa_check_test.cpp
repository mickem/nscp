// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The check half of the NCPA bridge, asserted against the behaviour of the real
// agent (agent/listener/nodes.py). Every expectation here is a byte of output
// that reaches a Nagios line or a perfdata field, so a change that "looks
// harmless" and breaks one of these changes what an operator's alert history
// and graphs contain.

#include "ncpa_check.hpp"

#include <gtest/gtest.h>

namespace {

using ncpa::check_result;
using ncpa::render_options;
using ncpa::request_options;
using ncpa::value;
using ncpa::values_type;

// --------------------------------------------------------------------------
// Nagios range syntax
// --------------------------------------------------------------------------

TEST(ncpa_range, plain_threshold_alerts_above_and_below_zero) {
  EXPECT_FALSE(ncpa::is_within_range("10", 5));
  EXPECT_FALSE(ncpa::is_within_range("10", 10));
  EXPECT_TRUE(ncpa::is_within_range("10", 10.5));
  // NCPA alerts on any negative value for a bare threshold. Odd, deliberate,
  // and relied upon by checks written against the real agent.
  EXPECT_TRUE(ncpa::is_within_range("10", -1));
}

TEST(ncpa_range, minimum_alerts_below) {
  EXPECT_TRUE(ncpa::is_within_range("10:", 9));
  EXPECT_FALSE(ncpa::is_within_range("10:", 10));
  EXPECT_FALSE(ncpa::is_within_range("10:", 1000));
  // Unlike the bare threshold, a minimum does not alert on a negative value
  // for its own sake - it alerts because a negative value is below it.
  EXPECT_TRUE(ncpa::is_within_range("-20:", -30));
  EXPECT_FALSE(ncpa::is_within_range("-20:", -10));
}

TEST(ncpa_range, maximum_alerts_above) {
  EXPECT_FALSE(ncpa::is_within_range(":10", 5));
  EXPECT_TRUE(ncpa::is_within_range(":10", 11));
  EXPECT_TRUE(ncpa::is_within_range(":10", -1));
}

TEST(ncpa_range, negative_infinity_maximum_ignores_negatives) {
  EXPECT_TRUE(ncpa::is_within_range("~:10", 11));
  EXPECT_FALSE(ncpa::is_within_range("~:10", -100));
}

TEST(ncpa_range, outside_and_inside_a_band) {
  EXPECT_FALSE(ncpa::is_within_range("10:20", 15));
  EXPECT_TRUE(ncpa::is_within_range("10:20", 9));
  EXPECT_TRUE(ncpa::is_within_range("10:20", 21));

  EXPECT_TRUE(ncpa::is_within_range("@10:20", 15));
  EXPECT_FALSE(ncpa::is_within_range("@10:20", 9));
  EXPECT_FALSE(ncpa::is_within_range("@10:20", 21));
}

TEST(ncpa_range, empty_range_never_alerts) { EXPECT_FALSE(ncpa::is_within_range("", 12345)); }

TEST(ncpa_range, a_malformed_range_throws) {
  // What turns a typo in -w into UNKNOWN rather than a check that silently
  // never fires.
  EXPECT_THROW(ncpa::is_within_range("not-a-range", 1), std::runtime_error);
  EXPECT_THROW(ncpa::is_within_range("10:20:30", 1), std::runtime_error);
}

TEST(ncpa_range, decimals_and_negative_bounds_parse) {
  EXPECT_TRUE(ncpa::is_within_range("10.5", 10.6));
  EXPECT_FALSE(ncpa::is_within_range("10.5", 10.5));
  EXPECT_FALSE(ncpa::is_within_range("-10:-5", -7));
  EXPECT_TRUE(ncpa::is_within_range("-10:-5", -11));
}

// --------------------------------------------------------------------------
// units scaling
// --------------------------------------------------------------------------

values_type bytes(const double v) { return values_type{value::from_int(static_cast<long long>(v))}; }

TEST(ncpa_units, every_prefix_scales_and_renames_the_unit) {
  struct sample {
    const char *units;
    double factor;
    const char *unit;
  };
  const sample samples[] = {{"k", 1e3, "kB"},
                            {"K", 1e3, "kB"},
                            {"M", 1e6, "MB"},
                            {"G", 1e9, "GB"},
                            {"T", 1e12, "TB"},
                            {"Ki", 1024.0, "KiB"},
                            {"Mi", 1048576.0, "MiB"},
                            {"Gi", 1073741824.0, "GiB"},
                            {"Ti", 1099511627776.0, "TiB"}};
  for (const sample &s : samples) {
    values_type vals = bytes(2 * s.factor);
    std::string unit = "B";
    ncpa::adjust_scale(vals, s.units, unit);
    EXPECT_EQ(unit, s.unit) << s.units;
    ASSERT_EQ(vals.size(), 1u);
    EXPECT_DOUBLE_EQ(vals[0].number, 2.0) << s.units;
    // A scaled value is a float, so it renders as "2.00" rather than "2".
    EXPECT_EQ(vals[0].format(), "2.00") << s.units;
  }
}

TEST(ncpa_units, lower_case_prefixes_are_accepted) {
  values_type vals = bytes(1073741824.0);
  std::string unit = "B";
  ncpa::adjust_scale(vals, "gi", unit);
  EXPECT_EQ(unit, "GiB");
  EXPECT_DOUBLE_EQ(vals[0].number, 1.0);
}

TEST(ncpa_units, a_non_byte_node_is_never_rescaled) {
  // check_ncpa sends `units` on every request, so a percentage has to ignore
  // it or `-u G` would silently divide a CPU load by a billion.
  values_type vals{value::from_double(42.5)};
  std::string unit = "%";
  ncpa::adjust_scale(vals, "G", unit);
  EXPECT_EQ(unit, "%");
  EXPECT_DOUBLE_EQ(vals[0].number, 42.5);
}

TEST(ncpa_units, explicit_bytes_truncates_but_does_not_scale) {
  values_type vals{value::from_double(1023.7)};
  std::string unit = "B";
  ncpa::adjust_scale(vals, "B", unit);
  EXPECT_EQ(unit, "B");
  EXPECT_EQ(vals[0].format(), "1023");
}

TEST(ncpa_units, an_unknown_prefix_is_ignored) {
  values_type vals = bytes(1024);
  std::string unit = "B";
  ncpa::adjust_scale(vals, "furlongs", unit);
  EXPECT_EQ(unit, "B");
  EXPECT_DOUBLE_EQ(vals[0].number, 1024);
}

TEST(ncpa_units, scaling_rounds_to_two_decimals) {
  values_type vals = bytes(1536);
  std::string unit = "B";
  ncpa::adjust_scale(vals, "Ki", unit);
  EXPECT_EQ(vals[0].format(), "1.50");
}

// --------------------------------------------------------------------------
// aggregate
// --------------------------------------------------------------------------

values_type four_cores() { return values_type{value::from_double(1), value::from_double(4), value::from_double(2), value::from_double(9)}; }

TEST(ncpa_aggregate, collapses_to_one_value) {
  values_type vals = four_cores();
  ncpa::aggregate_values(vals, "max");
  ASSERT_EQ(vals.size(), 1u);
  EXPECT_DOUBLE_EQ(vals[0].number, 9);

  vals = four_cores();
  ncpa::aggregate_values(vals, "min");
  ASSERT_EQ(vals.size(), 1u);
  EXPECT_DOUBLE_EQ(vals[0].number, 1);

  vals = four_cores();
  ncpa::aggregate_values(vals, "sum");
  ASSERT_EQ(vals.size(), 1u);
  EXPECT_DOUBLE_EQ(vals[0].number, 16);

  vals = four_cores();
  ncpa::aggregate_values(vals, "avg");
  ASSERT_EQ(vals.size(), 1u);
  EXPECT_DOUBLE_EQ(vals[0].number, 4);
}

TEST(ncpa_aggregate, avg_rounds_to_two_decimals) {
  values_type vals{value::from_double(1), value::from_double(1), value::from_double(2)};
  ncpa::aggregate_values(vals, "avg");
  EXPECT_EQ(vals[0].format(), "1.33");
}

TEST(ncpa_aggregate, an_unknown_mode_leaves_the_list_alone) {
  values_type vals = four_cores();
  // NCPA's default is the literal string "None".
  ncpa::aggregate_values(vals, "None");
  EXPECT_EQ(vals.size(), 4u);
  ncpa::aggregate_values(vals, "");
  EXPECT_EQ(vals.size(), 4u);
}

// --------------------------------------------------------------------------
// stdout and perfdata
// --------------------------------------------------------------------------

TEST(ncpa_render, ok_line_matches_the_agent) {
  request_options opts;
  opts.warning = "80";
  opts.critical = "90";
  render_options ropts;
  ropts.node_name = "percent";

  const check_result result = ncpa::render_check(values_type{value::from_double(3.2)}, "%", opts, ropts);
  EXPECT_EQ(result.returncode, 0);
  EXPECT_EQ(result.stdout_text, "OK: Percent was 3.20 % | 'percent'=3.20%;80;90;");
}

TEST(ncpa_render, thresholds_pick_the_prefix_and_the_return_code) {
  request_options opts;
  opts.warning = "80";
  opts.critical = "90";
  render_options ropts;
  ropts.node_name = "percent";

  check_result result = ncpa::render_check(values_type{value::from_double(85)}, "%", opts, ropts);
  EXPECT_EQ(result.returncode, 1);
  EXPECT_EQ(result.stdout_text, "WARNING: Percent was 85.00 % | 'percent'=85.00%;80;90;");

  result = ncpa::render_check(values_type{value::from_double(95)}, "%", opts, ropts);
  EXPECT_EQ(result.returncode, 2);
  EXPECT_EQ(result.stdout_text, "CRITICAL: Percent was 95.00 % | 'percent'=95.00%;80;90;");
}

TEST(ncpa_render, a_list_gets_one_indexed_perfdata_label_per_value) {
  request_options opts;
  opts.warning = "80";
  opts.critical = "90";
  render_options ropts;
  ropts.node_name = "percent";

  const check_result result = ncpa::render_check(values_type{value::from_double(1), value::from_double(95)}, "%", opts, ropts);
  EXPECT_EQ(result.returncode, 2);
  EXPECT_EQ(result.stdout_text, "CRITICAL: Percent was 1.00 %, 95.00 % | 'percent_0'=1.00%;80;90; 'percent_1'=95.00%;80;90;");
}

TEST(ncpa_render, an_integer_renders_without_decimals) {
  request_options opts;
  render_options ropts;
  ropts.node_name = "total";
  const check_result result = ncpa::render_check(values_type{value::from_int(8589934592LL)}, "B", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Total was 8589934592 B | 'total'=8589934592B;;;");
}

TEST(ncpa_render, a_title_longer_than_three_characters_is_dropped_from_perfdata) {
  // Nagios only knows a handful of UOMs; a word there breaks the parse, so
  // NCPA leaves it out of the perfdata while keeping it in the text.
  request_options opts;
  render_options ropts;
  ropts.node_name = "count";
  const check_result result = ncpa::render_check(values_type{value::from_int(3)}, "users", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Count was 3 users | 'count'=3;;;");
}

TEST(ncpa_render, the_title_is_python_capitalized) {
  // str.capitalize() lower-cases everything after the first character, so a
  // node called used_percent reports as "Used_percent" - not "Used_Percent".
  request_options opts;
  render_options ropts;
  ropts.node_name = "used_percent";
  const check_result result = ncpa::render_check(values_type{value::from_double(12)}, "%", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Used_percent was 12.00 % | 'used_percent'=12.00%;;;");
}

TEST(ncpa_render, an_explicit_title_and_perfdata_label_are_honoured) {
  request_options opts;
  opts.title = "Disk usage";
  opts.perfdata_label = "usage";
  render_options ropts;
  ropts.node_name = "used_percent";
  const check_result result = ncpa::render_check(values_type{value::from_double(12)}, "%", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Disk usage was 12.00 % | 'usage'=12.00%;;;");
}

TEST(ncpa_render, a_secondary_child_renders_name_colon_value_without_thresholds) {
  request_options opts;
  opts.warning = "80";
  opts.critical = "90";
  render_options ropts;
  ropts.node_name = "total";
  ropts.use_prefix = false;
  ropts.use_perfdata = false;
  ropts.secondary_data = true;
  ropts.primary_total = 100;

  const check_result result = ncpa::render_check(values_type{value::from_double(8)}, "GiB", opts, ropts);
  EXPECT_EQ(result.stdout_text, "Total: 8.00 GiB");
  // A three-character unit still fits in the perfdata; only a longer one is
  // dropped.
  EXPECT_EQ(result.perfdata, "'total'=8.00GiB;;;");
}

TEST(ncpa_render, the_primary_child_carries_the_extra_data_placeholder) {
  request_options opts;
  render_options ropts;
  ropts.node_name = "percent";
  ropts.primary = true;
  ropts.use_perfdata = false;
  ropts.custom_output = "Memory usage was";

  const check_result result = ncpa::render_check(values_type{value::from_double(42)}, "%", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Memory usage was 42.00 % {extra_data}");
  EXPECT_EQ(result.perfdata, "'percent'=42.00%;;;");
}

TEST(ncpa_render, uptime_is_reported_as_elapsed_time) {
  request_options opts;
  render_options ropts;
  ropts.node_name = "uptime";
  const check_result result = ncpa::render_check(values_type{value::from_double(90061)}, "s", opts, ropts);
  EXPECT_EQ(result.stdout_text, "OK: Uptime was 1 day 1 hour 1 minute 1 second | 'uptime'=90061.00s;;;");
}

TEST(ncpa_render, interface_status_is_reported_as_a_word) {
  request_options opts;
  render_options ropts;
  ropts.node_name = "status";
  EXPECT_EQ(ncpa::render_check(values_type{value::from_int(0)}, "", opts, ropts).stdout_text, "OK: Status is up | 'status'=0;;;");
  EXPECT_EQ(ncpa::render_check(values_type{value::from_int(2)}, "", opts, ropts).stdout_text, "OK: Status is down | 'status'=2;;;");
  EXPECT_EQ(ncpa::render_check(values_type{value::from_int(3)}, "", opts, ropts).stdout_text, "OK: Status is unknown | 'status'=3;;;");
}

TEST(ncpa_render, a_threshold_on_a_string_node_is_unknown_rather_than_ok) {
  request_options opts;
  opts.warning = "10";
  render_options ropts;
  ropts.node_name = "agent_version";
  const check_result result = ncpa::render_check(values_type{value::from_string("0.19.0")}, "", opts, ropts);
  EXPECT_EQ(result.returncode, 3);
  EXPECT_NE(result.stdout_text.find("could not convert string to float"), std::string::npos);
}

TEST(ncpa_render, a_malformed_threshold_is_unknown) {
  request_options opts;
  opts.warning = "eighty";
  render_options ropts;
  ropts.node_name = "percent";
  const check_result result = ncpa::render_check(values_type{value::from_double(1)}, "%", opts, ropts);
  EXPECT_EQ(result.returncode, 3);
  EXPECT_EQ(result.stdout_text, "Improper warning/critical format.");
}

TEST(ncpa_render, a_pipe_in_the_title_becomes_a_slash) {
  // A mount point is spelled "C:|" in the tree, and an unescaped pipe in the
  // text would be read as the start of perfdata by every Nagios frontend.
  request_options opts;
  opts.title = "C:|";
  render_options ropts;
  ropts.node_name = "used_percent";
  const check_result result = ncpa::render_check(values_type{value::from_double(12)}, "%", opts, ropts);
  EXPECT_EQ(result.stdout_text.compare(0, 11, "OK: C:/ was"), 0) << result.stdout_text;
}

// --------------------------------------------------------------------------
// elapsed_time
// --------------------------------------------------------------------------

TEST(ncpa_elapsed_time, singular_and_plural) {
  EXPECT_EQ(ncpa::elapsed_time(1), "1 second");
  EXPECT_EQ(ncpa::elapsed_time(62), "1 minute 2 seconds");
  EXPECT_EQ(ncpa::elapsed_time(3600), "1 hour");
  EXPECT_EQ(ncpa::elapsed_time(86400 * 3), "3 days");
  // Zero units are skipped entirely rather than reported as "0 hours".
  EXPECT_EQ(ncpa::elapsed_time(86401), "1 day 1 second");
  EXPECT_EQ(ncpa::elapsed_time(0), "");
}

// --------------------------------------------------------------------------
// delta
// --------------------------------------------------------------------------

TEST(ncpa_delta, the_first_sample_reports_zero) {
  ncpa::delta_store store;
  values_type vals{value::from_int(1000)};
  EXPECT_FALSE(store.deltaize_at("k", vals, 100));
  ASSERT_EQ(vals.size(), 1u);
  EXPECT_DOUBLE_EQ(vals[0].number, 0);
}

TEST(ncpa_delta, the_second_sample_is_a_per_second_rate) {
  ncpa::delta_store store;
  values_type first{value::from_int(1000)};
  store.deltaize_at("k", first, 100);

  values_type second{value::from_int(1200)};
  EXPECT_TRUE(store.deltaize_at("k", second, 110));
  EXPECT_DOUBLE_EQ(second[0].number, 20);
}

TEST(ncpa_delta, a_counter_reset_reports_the_absolute_difference) {
  ncpa::delta_store store;
  values_type first{value::from_int(1000)};
  store.deltaize_at("k", first, 100);
  values_type second{value::from_int(100)};
  store.deltaize_at("k", second, 110);
  EXPECT_DOUBLE_EQ(second[0].number, 90);
}

TEST(ncpa_delta, two_polls_in_the_same_second_report_zero_rather_than_infinity) {
  ncpa::delta_store store;
  values_type first{value::from_int(1000)};
  store.deltaize_at("k", first, 100);
  values_type second{value::from_int(1200)};
  store.deltaize_at("k", second, 100);
  EXPECT_DOUBLE_EQ(second[0].number, 0);
}

TEST(ncpa_delta, keys_do_not_share_samples) {
  ncpa::delta_store store;
  values_type a{value::from_int(1000)};
  store.deltaize_at("a", a, 100);
  values_type b{value::from_int(50)};
  // A different key has never been seen, so it reports zero rather than
  // differencing against the other poller's sample.
  EXPECT_FALSE(store.deltaize_at("b", b, 110));
  EXPECT_DOUBLE_EQ(b[0].number, 0);
}

}  // namespace
