// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/cli_helper.hpp>

// A minimal dummy filter type used to instantiate cli_helper<T>.
// Only build_filter() and post_process() depend on T's interface,
// and those methods are not exercised in these tests.
struct DummyFilter {};

// ============================================================================
// data_container tests
// ============================================================================

TEST(DataContainer, DefaultConstruction) {
  modern_filter::data_container dc;
  EXPECT_FALSE(dc.debug);
  EXPECT_FALSE(dc.escape_html);
  EXPECT_TRUE(dc.filter_string.empty());
  EXPECT_TRUE(dc.warn_string.empty());
  EXPECT_TRUE(dc.crit_string.empty());
  EXPECT_TRUE(dc.ok_string.empty());
  EXPECT_TRUE(dc.syntax_empty.empty());
  EXPECT_TRUE(dc.syntax_ok.empty());
  EXPECT_TRUE(dc.syntax_top.empty());
  EXPECT_TRUE(dc.syntax_detail.empty());
  EXPECT_TRUE(dc.syntax_perf.empty());
  EXPECT_TRUE(dc.perf_config.empty());
  EXPECT_TRUE(dc.empty_state.empty());
  EXPECT_TRUE(dc.syntax_unique.empty());
  // Not empty: a filter built without the misc options must still join lists
  // with something.
  EXPECT_EQ(", ", dc.list_separator);
}

// ============================================================================
// perf_writer tests
// ============================================================================

TEST(PerfWriter, WriteFloatValueFull) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();

  parsers::where::performance_data pd;
  pd.alias = "cpu_load";
  pd.unit = "%";
  parsers::where::performance_data::perf_value pv;
  pv.value = 42.5;
  pv.warn = 70.0;
  pv.crit = 90.0;
  pv.minimum = 0.0;
  pv.maximum = 100.0;
  pd.float_value = pv;

  modern_filter::perf_writer writer(*line);
  writer.write(pd);

  ASSERT_EQ(1, line->perf_size());
  const auto &perf = line->perf(0);
  EXPECT_EQ("cpu_load", perf.alias());
  EXPECT_TRUE(perf.has_float_value());
  EXPECT_FALSE(perf.has_string_value());
  EXPECT_DOUBLE_EQ(42.5, perf.float_value().value());
  EXPECT_EQ("%", perf.float_value().unit());
  EXPECT_DOUBLE_EQ(70.0, perf.float_value().warning().value());
  EXPECT_DOUBLE_EQ(90.0, perf.float_value().critical().value());
  EXPECT_DOUBLE_EQ(0.0, perf.float_value().minimum().value());
  EXPECT_DOUBLE_EQ(100.0, perf.float_value().maximum().value());
}

TEST(PerfWriter, WriteFloatValuePartial) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();

  parsers::where::performance_data pd;
  pd.alias = "mem_used";
  // No unit
  parsers::where::performance_data::perf_value pv;
  pv.value = 1024.0;
  pv.warn = 2048.0;
  // crit, minimum, maximum left unset (boost::optional)
  pd.float_value = pv;

  modern_filter::perf_writer writer(*line);
  writer.write(pd);

  ASSERT_EQ(1, line->perf_size());
  const auto &perf = line->perf(0);
  EXPECT_EQ("mem_used", perf.alias());
  EXPECT_TRUE(perf.has_float_value());
  EXPECT_DOUBLE_EQ(1024.0, perf.float_value().value());
  EXPECT_EQ("", perf.float_value().unit());
  EXPECT_TRUE(perf.float_value().has_warning());
  EXPECT_DOUBLE_EQ(2048.0, perf.float_value().warning().value());
  EXPECT_FALSE(perf.float_value().has_critical());
  EXPECT_FALSE(perf.float_value().has_minimum());
  EXPECT_FALSE(perf.float_value().has_maximum());
}

TEST(PerfWriter, WriteStringValue) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();

  parsers::where::performance_data pd;
  pd.alias = "status";
  pd.string_value = std::string("running");

  modern_filter::perf_writer writer(*line);
  writer.write(pd);

  ASSERT_EQ(1, line->perf_size());
  const auto &perf = line->perf(0);
  EXPECT_EQ("status", perf.alias());
  EXPECT_FALSE(perf.has_float_value());
  EXPECT_TRUE(perf.has_string_value());
  EXPECT_EQ("running", perf.string_value().value());
}

TEST(PerfWriter, WriteNoValue) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();

  parsers::where::performance_data pd;
  pd.alias = "empty_metric";
  // Neither float_value nor string_value set

  modern_filter::perf_writer writer(*line);
  writer.write(pd);

  ASSERT_EQ(1, line->perf_size());
  const auto &perf = line->perf(0);
  EXPECT_EQ("empty_metric", perf.alias());
  EXPECT_FALSE(perf.has_float_value());
  EXPECT_FALSE(perf.has_string_value());
}

TEST(PerfWriter, WriteMultipleValues) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();
  modern_filter::perf_writer writer(*line);

  parsers::where::performance_data pd1;
  pd1.alias = "metric1";
  parsers::where::performance_data::perf_value pv1;
  pv1.value = 10.0;
  pd1.float_value = pv1;

  parsers::where::performance_data pd2;
  pd2.alias = "metric2";
  pd2.string_value = std::string("hello");

  writer.write(pd1);
  writer.write(pd2);

  ASSERT_EQ(2, line->perf_size());
  EXPECT_EQ("metric1", line->perf(0).alias());
  EXPECT_EQ("metric2", line->perf(1).alias());
}

TEST(PerfWriter, WriteFloatValueNoUnit) {
  PB::Commands::QueryResponseMessage::Response response;
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();

  parsers::where::performance_data pd;
  pd.alias = "count";
  pd.unit = "";  // explicitly empty
  parsers::where::performance_data::perf_value pv;
  pv.value = 5.0;
  pd.float_value = pv;

  modern_filter::perf_writer writer(*line);
  writer.write(pd);

  ASSERT_EQ(1, line->perf_size());
  // Unit should not be set when empty
  EXPECT_EQ("", line->perf(0).float_value().unit());
}

// ============================================================================
// cli_helper tests — construction and simple accessors
// ============================================================================

class CliHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    request_.set_command("test_command");
    response_ = response_msg_.add_payload();
  }

  PB::Commands::QueryRequestMessage::Request request_;
  PB::Commands::QueryResponseMessage response_msg_;
  PB::Commands::QueryResponseMessage::Response *response_ = nullptr;
  modern_filter::data_container data_;
};

TEST_F(CliHelperTest, Construction) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  // Verify the helper initializes with expected defaults
  EXPECT_FALSE(helper.show_all);
  EXPECT_TRUE(helper.fields.empty());
}

TEST_F(CliHelperTest, EmptyReturnsTrueWhenNoWarnOrCrit) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  EXPECT_TRUE(helper.empty());
}

TEST_F(CliHelperTest, EmptyReturnsFalseWithWarn) {
  data_.warn_string.emplace_back("load > 80");
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  EXPECT_FALSE(helper.empty());
}

TEST_F(CliHelperTest, EmptyReturnsFalseWithCrit) {
  data_.crit_string.emplace_back("load > 95");
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  EXPECT_FALSE(helper.empty());
}

TEST_F(CliHelperTest, EmptyReturnsFalseWithBothWarnAndCrit) {
  data_.warn_string.emplace_back("load > 80");
  data_.crit_string.emplace_back("load > 95");
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  EXPECT_FALSE(helper.empty());
}

// ============================================================================
// cli_helper — append_all_filters
// ============================================================================

TEST_F(CliHelperTest, AppendAllFiltersWhenFilterStringEmpty) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.append_all_filters(" AND", "active = 1");

  ASSERT_EQ(1, data_.filter_string.size());
  EXPECT_EQ("(active = 1) AND active = 1", data_.filter_string[0]);
}

TEST_F(CliHelperTest, AppendAllFiltersWhenFilterStringExists) {
  data_.filter_string.emplace_back("type = 'service'");
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.append_all_filters(" AND", "active = 1");

  ASSERT_EQ(1, data_.filter_string.size());
  EXPECT_EQ("(type = 'service') AND active = 1", data_.filter_string[0]);
}

TEST_F(CliHelperTest, AppendAllFiltersMultipleExistingFilters) {
  data_.filter_string.emplace_back("type = 'a'");
  data_.filter_string.emplace_back("type = 'b'");
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.append_all_filters(" OR", "enabled = 1");

  ASSERT_EQ(2, data_.filter_string.size());
  EXPECT_EQ("(type = 'a') OR enabled = 1", data_.filter_string[0]);
  EXPECT_EQ("(type = 'b') OR enabled = 1", data_.filter_string[1]);
}

// ============================================================================
// cli_helper — set_default_index
// ============================================================================

TEST_F(CliHelperTest, SetDefaultIndexWhenEmpty) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.set_default_index("%(name)");

  EXPECT_EQ("%(name)", data_.syntax_unique);
}

TEST_F(CliHelperTest, SetDefaultIndexDoesNotOverwrite) {
  data_.syntax_unique = "%(id)";
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.set_default_index("%(name)");

  EXPECT_EQ("%(id)", data_.syntax_unique);
}

// ============================================================================
// cli_helper — set_default_perf_config
// ============================================================================

TEST_F(CliHelperTest, SetDefaultPerfConfig) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.set_default_perf_config("cpu(unit:%)");

  EXPECT_EQ("cpu(unit:%)", data_.perf_config);
}

TEST_F(CliHelperTest, SetDefaultPerfConfigOverwrites) {
  data_.perf_config = "old_config";
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.set_default_perf_config("new_config");

  EXPECT_EQ("new_config", data_.perf_config);
}

// ============================================================================
// cli_helper — set_filter_syntax
// ============================================================================

TEST_F(CliHelperTest, SetFilterSyntax) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  nscapi::program_options::field_map fields;
  fields["name"] = "The name of the item";
  fields["status"] = "Current status";

  helper.set_filter_syntax(fields);

  EXPECT_EQ(2, helper.fields.size());
  EXPECT_EQ("The name of the item", helper.fields["name"]);
  EXPECT_EQ("Current status", helper.fields["status"]);
}

// ============================================================================
// cli_helper — parse_options_post (show_all replacements)
// ============================================================================

TEST_F(CliHelperTest, ParseOptionsPostShowAllReplacesDollarProblemList) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Status: ${problem_list}";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("Status: ${detail_list}", data_.syntax_top);
}

TEST_F(CliHelperTest, ParseOptionsPostShowAllReplacesPercentProblemList) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Status: %(problem_list)";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("Status: %(detail_list)", data_.syntax_top);
}

TEST_F(CliHelperTest, ParseOptionsPostShowAllReplacesPercentList) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Status: %(list)";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  // %(list) is kept as %(list) when show_all
  EXPECT_EQ("Status: %(list)", data_.syntax_top);
}

TEST_F(CliHelperTest, ParseOptionsPostShowAllReplacesDollarList) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Status: ${list}";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("Status: %(list)", data_.syntax_top);
}

TEST_F(CliHelperTest, ParseOptionsPostShowAllFallbackToDetailList) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Some status message";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("%(detail_list)", data_.syntax_top);
}

TEST_F(CliHelperTest, ParseOptionsPostShowAllDisabledNoChange) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = false;
  data_.syntax_top = "Status: ${problem_list}";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("Status: ${problem_list}", data_.syntax_top);
}

// ============================================================================
// cli_helper — parse_options_post (syntax_ok clearing)
// ============================================================================

TEST_F(CliHelperTest, ParseOptionsPostClearsSyntaxOkWhenDetailList) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Status: %(detail_list)";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("", data_.syntax_ok);
}

TEST_F(CliHelperTest, ParseOptionsPostClearsSyntaxOkWhenPercentList) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Status: %(list)";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("", data_.syntax_ok);
}

TEST_F(CliHelperTest, ParseOptionsPostClearsSyntaxOkWhenDollarList) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Status: ${list}";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("", data_.syntax_ok);
}

TEST_F(CliHelperTest, ParseOptionsPostClearsSyntaxOkWhenMatchList) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Status: %(match_list)";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("", data_.syntax_ok);
}

TEST_F(CliHelperTest, ParseOptionsPostClearsSyntaxOkWhenLines) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Total lines: 5";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("", data_.syntax_ok);
}

TEST_F(CliHelperTest, ParseOptionsPostKeepsSyntaxOkWhenNoListKeyword) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.syntax_top = "Status: %(status) %(count) items";
  data_.syntax_ok = "Everything is fine";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  EXPECT_EQ("Everything is fine", data_.syntax_ok);
}

// ============================================================================
// cli_helper — parse_options_post (warn/crit alias handling)
// ============================================================================

TEST_F(CliHelperTest, ParseOptionsPostWarnAliasOverridesWarning) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.warn_string.emplace_back("original_warn");

  // Build a variables_map with "warn" set via boost::program_options parsing
  boost::program_options::options_description alias_desc;
  alias_desc.add_options()("warn", boost::program_options::value<std::vector<std::string>>(), "alias");
  boost::program_options::variables_map vm;
  std::vector<std::string> args = {"--warn", "alias_warn_value"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(alias_desc).run(), vm);
  boost::program_options::notify(vm);

  helper.parse_options_post(vm);

  ASSERT_EQ(1, data_.warn_string.size());
  EXPECT_EQ("alias_warn_value", data_.warn_string[0]);
}

TEST_F(CliHelperTest, ParseOptionsPostCritAliasOverridesCritical) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.crit_string.emplace_back("original_crit");

  // Build a variables_map with "crit" set via boost::program_options parsing
  boost::program_options::options_description alias_desc;
  alias_desc.add_options()("crit", boost::program_options::value<std::vector<std::string>>(), "alias");
  boost::program_options::variables_map vm;
  std::vector<std::string> args = {"--crit", "alias_crit_value"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(alias_desc).run(), vm);
  boost::program_options::notify(vm);

  helper.parse_options_post(vm);

  ASSERT_EQ(1, data_.crit_string.size());
  EXPECT_EQ("alias_crit_value", data_.crit_string[0]);
}

TEST_F(CliHelperTest, ParseOptionsPostNoAliasKeepsOriginal) {
  const modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  data_.warn_string.emplace_back("keep_this_warn");
  data_.crit_string.emplace_back("keep_this_crit");

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  ASSERT_EQ(1, data_.warn_string.size());
  EXPECT_EQ("keep_this_warn", data_.warn_string[0]);
  ASSERT_EQ(1, data_.crit_string.size());
  EXPECT_EQ("keep_this_crit", data_.crit_string[0]);
}

// ============================================================================
// cli_helper — add_syntax sets defaults in data
// ============================================================================

TEST_F(CliHelperTest, AddSyntaxRegistersOptions) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_syntax("%(status): %(list)", "%(name) = %(value)", "%(name)", "No items found", "All %(count) items are ok");

  // Parsing empty arguments should apply default values
  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("%(status): %(list)", data_.syntax_top);
  EXPECT_EQ("%(name) = %(value)", data_.syntax_detail);
  EXPECT_EQ("%(name)", data_.syntax_perf);
  EXPECT_EQ("No items found", data_.syntax_empty);
  EXPECT_EQ("All %(count) items are ok", data_.syntax_ok);
}

TEST_F(CliHelperTest, AddSyntaxEmptyDefaults) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_syntax("", "", "", "", "");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("", data_.syntax_top);
  EXPECT_EQ("", data_.syntax_detail);
  EXPECT_EQ("", data_.syntax_perf);
  EXPECT_EQ("", data_.syntax_empty);
  EXPECT_EQ("", data_.syntax_ok);
}

// ============================================================================
// cli_helper — add_index
// ============================================================================

TEST_F(CliHelperTest, AddIndexRegistersOption) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_index("%(name)");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("%(name)", data_.syntax_unique);
}

// ============================================================================
// cli_helper — add_filter_option
// ============================================================================

TEST_F(CliHelperTest, AddFilterOptionWithDefault) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_filter_option("state = 'started'");

  // Parse empty arguments to apply default
  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(1, data_.filter_string.size());
  EXPECT_EQ("state = 'started'", data_.filter_string[0]);
}

TEST_F(CliHelperTest, AddFilterOptionEmpty) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_filter_option("");

  // Parse empty arguments — no default set
  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_TRUE(data_.filter_string.empty());
}

// ============================================================================
// cli_helper — add_warn_option / add_crit_option
// ============================================================================

TEST_F(CliHelperTest, AddWarnOptionWithDefault) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_warn_option("load > 80");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(1, data_.warn_string.size());
  EXPECT_EQ("load > 80", data_.warn_string[0]);
}

TEST_F(CliHelperTest, AddCritOptionWithDefault) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_crit_option("load > 95");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(1, data_.crit_string.size());
  EXPECT_EQ("load > 95", data_.crit_string[0]);
}

TEST_F(CliHelperTest, AddWarnOptionDualDefaults) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_warn_option("load > 80", "mem > 70");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(2, data_.warn_string.size());
  EXPECT_EQ("load > 80", data_.warn_string[0]);
  EXPECT_EQ("mem > 70", data_.warn_string[1]);
}

TEST_F(CliHelperTest, AddCritOptionDualDefaults) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_crit_option("load > 95", "mem > 90");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(2, data_.crit_string.size());
  EXPECT_EQ("load > 95", data_.crit_string[0]);
  EXPECT_EQ("mem > 90", data_.crit_string[1]);
}

// ============================================================================
// cli_helper — add_ok_option
// ============================================================================

TEST_F(CliHelperTest, AddOkOptionWithDefault) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_ok_option("state = 'ok'");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(1, data_.ok_string.size());
  EXPECT_EQ("state = 'ok'", data_.ok_string[0]);
}

TEST_F(CliHelperTest, AddOkOptionEmpty) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_ok_option();

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_TRUE(data_.ok_string.empty());
}

// ============================================================================
// cli_helper — add_misc_options
// ============================================================================

TEST_F(CliHelperTest, AddMiscOptionsDefaultEmptyState) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("ignored", data_.empty_state);
  EXPECT_FALSE(data_.debug);
  EXPECT_FALSE(data_.escape_html);
}

TEST_F(CliHelperTest, AddMiscOptionsCustomEmptyState) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_misc_options("critical");

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("critical", data_.empty_state);
}

// ============================================================================
// cli_helper — add_options (composite)
// ============================================================================

TEST_F(CliHelperTest, AddOptionsCompositeRegistersAll) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  std::map<std::string, std::string> syntax;
  syntax["name"] = "The name";

  helper.add_options("load > 80", "load > 95", "active = 1", syntax, "warning");

  boost::program_options::variables_map vm;
  std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  ASSERT_EQ(1, data_.filter_string.size());
  EXPECT_EQ("active = 1", data_.filter_string[0]);
  ASSERT_EQ(1, data_.warn_string.size());
  EXPECT_EQ("load > 80", data_.warn_string[0]);
  ASSERT_EQ(1, data_.crit_string.size());
  EXPECT_EQ("load > 95", data_.crit_string[0]);
  EXPECT_EQ("warning", data_.empty_state);
  EXPECT_EQ("The name", helper.fields["name"]);
}

// ============================================================================
// cli_helper — parse_options_post combined show_all + syntax_ok clearing
// ============================================================================

TEST_F(CliHelperTest, ParseOptionsPostShowAllWithDetailListClearsSyntaxOk) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.show_all = true;
  data_.syntax_top = "Status: %(problem_list)";
  data_.syntax_ok = "All good";

  const boost::program_options::variables_map vm;
  helper.parse_options_post(vm);

  // show_all replaces problem_list with detail_list
  EXPECT_EQ("Status: %(detail_list)", data_.syntax_top);
  // detail_list triggers syntax_ok clearing
  EXPECT_EQ("", data_.syntax_ok);
}

// ============================================================================
// cli_helper — list-separator (issue #1370)
// ============================================================================

// Enough of a filter for build_filter() to run against: it only needs to
// accept the build calls and expose a summary whose separator we can read.
struct SeparatorFilter {
  struct {
    std::string list_separator = ", ";
  } summary;
  // build_filter sets the number format on the context and tells the renderers
  // whether they have one to honour; both are recorded here so the tests can
  // read them back.
  struct fake_context {
    str::number_format number_format;
    void set_number_format(const str::number_format &fmt) { number_format = fmt; }
  };
  std::shared_ptr<fake_context> context = std::make_shared<fake_context>();
  bool human_numbers = false;
  bool started = false;

  void set_human_number_format(const bool human) { human_numbers = human; }

  bool build_syntax(bool, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &) {
    return true;
  }
  bool build_index(const std::string &, std::string &) { return true; }
  bool build_engines(bool, const std::vector<std::string> &, const std::vector<std::string> &, const std::vector<std::string> &,
                     const std::vector<std::string> &) {
    return true;
  }
  bool validate(std::string &) { return true; }
  void start_match() { started = true; }
};

TEST_F(CliHelperTest, AddMiscOptionsDefaultListSeparator) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ(", ", data_.list_separator);
}

TEST_F(CliHelperTest, AddMiscOptionsListSeparatorFromArguments) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);

  helper.add_misc_options();

  boost::program_options::variables_map vm;
  // The escape survives option parsing untouched; build_filter decodes it.
  const std::vector<std::string> args{"--list-separator", "\\n"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_EQ("\\n", data_.list_separator);
}

// The point of the option: what the operator typed as \n has to reach the
// summary as a real newline, and it has to be there before matching starts.
TEST_F(CliHelperTest, BuildFilterUnescapesListSeparatorIntoTheSummary) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  data_.list_separator = "\\n";

  SeparatorFilter filter;
  ASSERT_TRUE(helper.build_filter(filter));

  EXPECT_EQ("\n", filter.summary.list_separator);
  EXPECT_TRUE(filter.started);
}

TEST_F(CliHelperTest, BuildFilterKeepsAPlainSeparatorVerbatim) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  data_.list_separator = " | ";

  SeparatorFilter filter;
  ASSERT_TRUE(helper.build_filter(filter));

  EXPECT_EQ(" | ", filter.summary.list_separator);
}

// ============================================================================
// cli_helper — number formatting (issue #1428)
// ============================================================================

TEST_F(CliHelperTest, NumberFormatDefaultsToTheHistoricalRendering) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> empty_args;
  boost::program_options::store(boost::program_options::command_line_parser(empty_args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  SeparatorFilter filter;
  ASSERT_TRUE(helper.build_filter(filter));

  EXPECT_TRUE(filter.context->number_format.is_default());
  // Nothing configured, so the templates keep rendering their floats the way
  // they always did.
  EXPECT_FALSE(filter.human_numbers);
}

TEST_F(CliHelperTest, NumberFormatOptionsReachTheContext) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> args{"--decimals", "2", "--byte-unit", "GB", "--decimal-separator", ",", "--thousands-separator", "."};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  SeparatorFilter filter;
  ASSERT_TRUE(helper.build_filter(filter));

  EXPECT_EQ(2, filter.context->number_format.decimals);
  EXPECT_EQ("GB", filter.context->number_format.byte_unit);
  EXPECT_EQ(",", filter.context->number_format.decimal_separator);
  EXPECT_EQ(".", filter.context->number_format.thousands_separator);
  EXPECT_TRUE(filter.human_numbers);
}

// REST passes each option as the single token `key=value`, which is the
// transport the request came in over for most of these checks.
TEST_F(CliHelperTest, NumberFormatOptionsAcceptTheValuedForm) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> args{"--decimals=1", "--byte-unit=MB"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  SeparatorFilter filter;
  ASSERT_TRUE(helper.build_filter(filter));

  EXPECT_EQ(1, filter.context->number_format.decimals);
  EXPECT_EQ("MB", filter.context->number_format.byte_unit);
}

TEST_F(CliHelperTest, BuildFilterRejectsAnUnknownByteUnit) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  data_.byte_unit = "ZB";

  SeparatorFilter filter;
  EXPECT_FALSE(helper.build_filter(filter));
  ASSERT_GT(response_->lines_size(), 0);
  EXPECT_NE(std::string::npos, response_->lines(0).message().find("Invalid byte-unit"));
  // A rejected option must not leave the filter half-built.
  EXPECT_FALSE(filter.started);
}

TEST_F(CliHelperTest, BuildFilterRejectsNegativeDecimals) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  data_.decimals = -2;

  SeparatorFilter filter;
  EXPECT_FALSE(helper.build_filter(filter));
  ASSERT_GT(response_->lines_size(), 0);
  EXPECT_NE(std::string::npos, response_->lines(0).message().find("Invalid decimals"));
}

// An unbounded decimals would make render_fixed allocate a multi-megabyte
// string and crash the check, so a value past max_decimals is rejected up
// front rather than reaching the renderer.
TEST_F(CliHelperTest, BuildFilterRejectsRunawayDecimals) {
  modern_filter::cli_helper<SeparatorFilter> helper(request_, response_, data_);
  data_.decimals = 1000000000;

  SeparatorFilter filter;
  EXPECT_FALSE(helper.build_filter(filter));
  ASSERT_GT(response_->lines_size(), 0);
  EXPECT_NE(std::string::npos, response_->lines(0).message().find("Invalid decimals"));
  EXPECT_FALSE(filter.started);
}

// ============================================================================
// cli_helper — valued booleans (REST passes `x=true` as a single token, which
// bool_switch would reject with "does not take any arguments")
// ============================================================================

TEST_F(CliHelperTest, MiscBooleansAcceptValuedForm) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> args{"--escape-html=true", "--show-all=true", "--debug=false"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_TRUE(data_.escape_html);
  EXPECT_TRUE(helper.show_all);
  EXPECT_FALSE(data_.debug);
}

TEST_F(CliHelperTest, MiscBooleansStillWorkAsBareFlags) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  boost::program_options::variables_map vm;
  const std::vector<std::string> args{"--escape-html", "--show-all", "--debug"};
  boost::program_options::store(boost::program_options::command_line_parser(args).options(helper.get_desc()).run(), vm);
  boost::program_options::notify(vm);

  EXPECT_TRUE(data_.escape_html);
  EXPECT_TRUE(helper.show_all);
  EXPECT_TRUE(data_.debug);
}

// ============================================================================
// cli_helper — warmup-state
// ============================================================================

TEST_F(CliHelperTest, WarmupStateDefaultsToUnknown) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_warmup_option();

  ASSERT_TRUE(helper.parse_options());
  helper.set_warmup_response("No data available yet (collector still initializing)");

  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response_->result());
  ASSERT_EQ(1, response_->lines_size());
  EXPECT_EQ("No data available yet (collector still initializing)", response_->lines(0).message());
}

// REST passes the option as a single key=value token.
TEST_F(CliHelperTest, WarmupStatePicksTheReportedStatus) {
  const std::vector<std::pair<std::string, PB::Common::ResultCode>> cases{{"ok", PB::Common::ResultCode::OK},
                                                                          {"warning", PB::Common::ResultCode::WARNING},
                                                                          {"CRITICAL", PB::Common::ResultCode::CRITICAL},
                                                                          {"unknown", PB::Common::ResultCode::UNKNOWN},
                                                                          // The short spellings every other status option takes.
                                                                          {"warn", PB::Common::ResultCode::WARNING},
                                                                          {"crit", PB::Common::ResultCode::CRITICAL}};
  for (const auto &c : cases) {
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command("test_command");
    request.add_arguments("warmup-state=" + c.first);
    PB::Commands::QueryResponseMessage msg;
    PB::Commands::QueryResponseMessage::Response *response = msg.add_payload();
    modern_filter::data_container data;
    modern_filter::cli_helper<DummyFilter> helper(request, response, data);
    helper.add_warmup_option();

    ASSERT_TRUE(helper.parse_options()) << c.first;
    helper.set_warmup_response("warming up");
    EXPECT_EQ(c.second, response->result()) << c.first;
    EXPECT_EQ("warming up", response->lines(0).message()) << c.first;
  }
}

TEST_F(CliHelperTest, WarmupStateRejectsAnUnknownValue) {
  request_.add_arguments("warmup-state=okay");
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_warmup_option();

  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response_->result());
  ASSERT_EQ(1, response_->lines_size());
  EXPECT_NE(std::string::npos, response_->lines(0).message().find("Invalid warmup-state: okay"));
}

// Only checks that opt in carry the option; elsewhere it stays an unknown
// argument instead of being silently accepted.
TEST_F(CliHelperTest, WarmupStateIsNotRegisteredWithoutOptIn) {
  request_.add_arguments("warmup-state=ok");
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_misc_options();

  EXPECT_FALSE(helper.parse_options());
}

TEST_F(CliHelperTest, WarmupStateRemembersWhetherItWasGiven) {
  modern_filter::cli_helper<DummyFilter> defaulted(request_, response_, data_);
  defaulted.add_warmup_option();
  ASSERT_TRUE(defaulted.parse_options());
  EXPECT_FALSE(defaulted.warmup_state_given());

  request_.add_arguments("warmup-state=unknown");
  modern_filter::data_container data;
  modern_filter::cli_helper<DummyFilter> given(request_, response_, data);
  given.add_warmup_option();
  ASSERT_TRUE(given.parse_options());
  EXPECT_TRUE(given.warmup_state_given());
}

// A collector that has tried and come back empty is never a warm-up.
TEST_F(CliHelperTest, AnswerUnlessSampledKeepsFailuresUnknown) {
  request_.add_arguments("warmup-state=ok");
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_warmup_option();
  ASSERT_TRUE(helper.parse_options());

  sampling::tracker t;
  t.failed("/proc/stat: permission denied");
  EXPECT_TRUE(helper.answer_unless_sampled(t.get(false), "CPU"));
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, response_->result());
  EXPECT_EQ("No CPU data available: the collector failed to sample it: /proc/stat: permission denied", response_->lines(0).message());
}

TEST_F(CliHelperTest, AnswerUnlessSampledUsesWarmupStateBeforeTheFirstAttempt) {
  request_.add_arguments("warmup-state=ok");
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_warmup_option();
  ASSERT_TRUE(helper.parse_options());

  EXPECT_TRUE(helper.answer_unless_sampled(sampling::tracker().get(false), "CPU"));
  EXPECT_EQ(PB::Common::ResultCode::OK, response_->result());
  EXPECT_EQ("No CPU data available yet (collector still initializing)", response_->lines(0).message());
}

TEST_F(CliHelperTest, AnswerUnlessSampledLeavesAReadyCheckAlone) {
  modern_filter::cli_helper<DummyFilter> helper(request_, response_, data_);
  helper.add_warmup_option();
  ASSERT_TRUE(helper.parse_options());

  sampling::tracker t;
  t.succeeded();
  EXPECT_FALSE(helper.answer_unless_sampled(t.get(true), "CPU"));
  EXPECT_EQ(0, response_->lines_size());
}

// ============================================================================
// cli_helper — the -w / -c short thresholds
// ============================================================================

namespace {
struct short_threshold_case {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage msg;
  PB::Commands::QueryResponseMessage::Response *response;
  modern_filter::data_container data;
  explicit short_threshold_case(const std::vector<std::string> &args) : response(msg.add_payload()) {
    request.set_command("test_command");
    for (const std::string &a : args) request.add_arguments(a);
  }
  std::string message() const { return response->lines_size() > 0 ? response->lines(0).message() : ""; }
};
typedef std::vector<std::string> string_list;
}  // namespace

// Every shape the short form arrives in: on its own, mixed into key=value
// arguments (REST, NRPE, the test console), and mixed into dashed ones.
TEST(CliHelperShortThresholds, EveryArgumentShapeIsAccepted) {
  const std::vector<string_list> shapes{{"-w", "80", "-c", "90"},        {"-w80", "-c90"},  {"-w=80", "-c=90"}, {"w=80", "c=90"}, {"show-all=true", "-w", "80", "-c", "90"},
                                    {"-w", "80", "-c", "90", "show-all=true"}, {"--show-all", "-w", "80", "-c", "90"},     {"--w", "80", "--c", "90"}};
  for (const string_list &shape : shapes) {
    short_threshold_case c(shape);
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("load > 70", "load > 95", "", {});
    helper.add_short_threshold("load", modern_filter::shorthand::above);
    ASSERT_TRUE(helper.parse_options()) << boost::join(shape, " ") << ": " << c.message();
    EXPECT_EQ(string_list({"load > 80"}), c.data.warn_string) << boost::join(shape, " ");
    EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string) << boost::join(shape, " ");
  }
}

TEST(CliHelperShortThresholds, WithoutADeclaredKeywordItBoundsCount) {
  short_threshold_case c({"-w", "5", "-c", "1:"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"count > 5"}), c.data.warn_string);
  EXPECT_EQ(string_list({"count < 1"}), c.data.crit_string);
}

// The keyword can be declared after add_options(), where the check's own
// options are.
TEST(CliHelperShortThresholds, OnlyTheGivenSideReplacesItsDefault) {
  short_threshold_case c({"-c", "10"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("free < 20%", "free < 5%", "", {});
  helper.add_short_threshold("free", modern_filter::shorthand::below, modern_filter::shorthand::unit_percent);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"free < 20%"}), c.data.warn_string);
  EXPECT_EQ(string_list({"free < 10%"}), c.data.crit_string);
}

TEST(CliHelperShortThresholds, NextToAnExplicitWarningItIsOneMoreExpression) {
  short_threshold_case c({"warning=state = 'bad'", "-w", "80"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"state = 'bad'", "load > 80"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 95"}), c.data.crit_string);
}

TEST(CliHelperShortThresholds, AnExpressionIsUsedAsItIs) {
  short_threshold_case c({"-w", "user > 80", "-c", "none"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"user > 80"}), c.data.warn_string);
  // "none" is what build_filter() strips, leaving the check without a critical.
  EXPECT_EQ(string_list({"none"}), c.data.crit_string);
}

TEST(CliHelperShortThresholds, DurationsAndLists) {
  short_threshold_case c({"-w", "0.5,20", "-c", "5m"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  helper.add_short_threshold("time", modern_filter::shorthand::above, modern_filter::shorthand::unit_seconds_as_ms);
  helper.add_short_threshold("loss", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"time > 500 or loss > 20%"}), c.data.warn_string);
  EXPECT_EQ(string_list({"time > 300000"}), c.data.crit_string);
}

TEST(CliHelperShortThresholds, ABadThresholdIsReported) {
  short_threshold_case c({"-w", "1,2"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above);
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
  EXPECT_NE(std::string::npos, c.message().find("Too many values in threshold 1,2")) << c.message();
}

// The help names the keyword, so "what does -w mean here" is answered by the
// check itself.
TEST(CliHelperShortThresholds, HelpNamesTheKeyword) {
  short_threshold_case c({"help"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above);
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  // The rendered help is word wrapped, so the wording is read off the option.
  EXPECT_NE(std::string::npos, c.message().find("w=ARG")) << c.message();
  EXPECT_NE(std::string::npos, helper.get_desc().find("w", false).description().find("-w N is the same as warning=\"load > N\""));
  EXPECT_NE(std::string::npos, helper.get_desc().find("c", false).description().find("-c N is the same as critical=\"load > N\""));
}

// Only the count form is shared by every check; a check's own keyword must
// stay on its reference page (see common_option_marker).
TEST(CliHelperShortThresholds, OnlyTheCountFormIsACommonOption) {
  {
    short_threshold_case c({});
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("", "", "", {});
    ASSERT_TRUE(helper.parse_options());
    EXPECT_NE(std::string::npos, helper.get_desc().find("w", false).description().find("Common option for all filter checks."));
  }
  {
    short_threshold_case c({});
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("", "", "", {});
    helper.add_short_threshold("load", modern_filter::shorthand::above);
    ASSERT_TRUE(helper.parse_options());
    EXPECT_EQ(std::string::npos, helper.get_desc().find("w", false).description().find("Common option for all filter checks."));
  }
}

TEST(CliHelperShortThresholds, CanBeDisabled) {
  short_threshold_case c({"-w", "5"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  helper.disable_short_thresholds();
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(nullptr, helper.get_desc().find_nothrow("w", false));
}

// "-c 5" on a check whose critical also says "result != 'ok'" moves the time
// bound; the template is what keeps a failed request critical.
TEST(CliHelperShortThresholds, TheTemplateKeepsTheOtherConditions) {
  short_threshold_case c({"-w", "2", "-c", "5"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", {});
  helper.add_short_threshold("time", modern_filter::shorthand::above, modern_filter::shorthand::unit_seconds_as_ms, "1", "5");
  helper.add_short_critical("${time} or result != 'ok'");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"time > 2000"}), c.data.warn_string);
  EXPECT_EQ(string_list({"time > 5000 or result != 'ok'"}), c.data.crit_string);
}

// The default of the short form is documentation. Until -w / -c is given the
// check's own default runs, also where the two are not the same text.
TEST(CliHelperShortThresholds, TheShortDefaultDoesNotReplaceTheChecksDefault) {
  short_threshold_case c({});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("used > 80%", "used > 90%", "", {});
  helper.add_short_threshold("free", modern_filter::shorthand::below, modern_filter::shorthand::unit_percent, "20", "10");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"used > 80%"}), c.data.warn_string);
  EXPECT_EQ(string_list({"used > 90%"}), c.data.crit_string);
}

// show-default has to stay a line that can be copied, edited and passed back:
// with "w=20" in it, editing only warning= would leave the short form adding
// the old bound back as a second expression.
TEST(CliHelperShortThresholds, ShowDefaultLeavesTheShortFormOut) {
  // The abbreviations are what the option parser would guess: answered there
  // they would list the short form too.
  for (const std::string arg : {"show-default", "--show-default", "--show-def", "show-d"}) {
    short_threshold_case c({arg});
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("used > 80%", "used > 90%", "", {});
    helper.add_short_threshold("free", modern_filter::shorthand::below, modern_filter::shorthand::unit_percent, "20", "10");
    EXPECT_FALSE(helper.parse_options());
    EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
    EXPECT_NE(std::string::npos, c.message().find("\"warning=used > 80%\"")) << c.message();
    EXPECT_NE(std::string::npos, c.message().find("\"critical=used > 90%\"")) << c.message();
    EXPECT_EQ(std::string::npos, c.message().find("\"w=")) << c.message();
    EXPECT_EQ(std::string::npos, c.message().find("\"c=")) << c.message();
  }
}

// The round trip itself: what show-default prints, with only the warning
// edited, must run with the edited warning and nothing else.
TEST(CliHelperShortThresholds, ShowDefaultRoundTripsAfterAnEdit) {
  short_threshold_case c({"warning=load > 95", "critical=load > 98"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "80", "90");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"load > 95"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 98"}), c.data.crit_string);
}

// The option is listed in help; a value that merely contains the word is not
// taken for the command.
TEST(CliHelperShortThresholds, ShowQueryIsListedAndNotMistaken) {
  short_threshold_case c({"filter=name = 'show-query'"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_NE(nullptr, helper.get_desc().find_nothrow("show-query", false));
  EXPECT_FALSE(helper.show_query_requested);
  EXPECT_EQ(string_list({"name = 'show-query'"}), c.data.filter_string);
}

TEST(CliHelperShortThresholds, OneSideLeavesTheOtherDefaultAlone) {
  short_threshold_case c({"-w", "25"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("used > 80%", "used > 90%", "", {});
  helper.add_short_threshold("free", modern_filter::shorthand::below, modern_filter::shorthand::unit_percent, "20", "10");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"free < 25%"}), c.data.warn_string);
  EXPECT_EQ(string_list({"used > 90%"}), c.data.crit_string);
}

// An expression is a threshold of its own, as with warning=.
TEST(CliHelperShortThresholds, AnExpressionReplacesTheWholeDefault) {
  short_threshold_case c({"-c", "code = 500"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", {});
  helper.add_short_threshold("time", modern_filter::shorthand::above, modern_filter::shorthand::unit_seconds_as_ms, "1", "5");
  helper.add_short_critical("${time} or result != 'ok'");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"code = 500"}), c.data.crit_string);
}

// A bound on the number of matches says how many problems are a problem, so
// "any error is critical" must not survive next to it.
TEST(CliHelperShortThresholds, ACountBoundReplacesTheWholeDefault) {
  short_threshold_case c({"-c", "10"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("level = 'warning'", "level = 'error'", "", {});
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"level = 'warning'"}), c.data.warn_string);
  EXPECT_EQ(string_list({"count > 10"}), c.data.crit_string);
}

// A negative bound next to dashed arguments must reach the option as its
// value rather than be read as an option.
TEST(CliHelperShortThresholds, ANegativeBoundNextToDashedArguments) {
  short_threshold_case c({"--show-all", "-c", "-5:"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("temperature > 70", "temperature > 90", "", {});
  helper.add_short_threshold("temperature", modern_filter::shorthand::above, modern_filter::shorthand::unit_plain, "70", "90");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"temperature < -5"}), c.data.crit_string);
  EXPECT_TRUE(helper.show_all);
}

// An unset macro arrives as "w=": that is a broken command, not a request to
// drop the warning.
TEST(CliHelperShortThresholds, AnEmptyValueIsRefused) {
  short_threshold_case c({"w="});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "70", "95");
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
  EXPECT_NE(std::string::npos, c.message().find("Missing value for -w")) << c.message();
}

TEST(CliHelperShortThresholds, AWrongUnitIsRefused) {
  short_threshold_case c({"-w", "80m"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "70", "95");
  EXPECT_FALSE(helper.parse_options());
  EXPECT_NE(std::string::npos, c.message().find("Invalid unit for load: 80m")) << c.message();
}

// "-w 20:10" would be "load < 20 or load > 10": always true, so the check
// would alert for good. It is refused like any other bad threshold.
TEST(CliHelperShortThresholds, AReversedRangeIsRefused) {
  short_threshold_case c({"-w", "20:10"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 70", "load > 95", "", {});
  helper.add_short_threshold("load", modern_filter::shorthand::above, modern_filter::shorthand::unit_percent_number, "70", "95");
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
  EXPECT_NE(std::string::npos, c.message().find("Invalid threshold: 20:10")) << c.message();
}

// A duration on a millisecond keyword: the help says what -w N really is.
TEST(CliHelperShortThresholds, HelpSaysSecondsBecomeMilliseconds) {
  short_threshold_case c({});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("time > 1000", "time > 5000", "", {});
  helper.add_short_threshold("time", modern_filter::shorthand::above, modern_filter::shorthand::unit_seconds_as_ms, "1", "5");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  const std::string text = helper.get_desc().find("w", false).description();
  EXPECT_NE(std::string::npos, text.find("-w N is the same as warning=\"time > N*1000\"")) << text;
}

// With a template the other conditions of the default stay, so the help must
// not say the default is replaced right next to an expansion that shows it is
// not.
TEST(CliHelperShortThresholds, HelpDoesNotSayATemplateReplacesTheDefault) {
  short_threshold_case c({});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", {});
  helper.add_short_threshold("time", modern_filter::shorthand::above, modern_filter::shorthand::unit_seconds_as_ms, "1", "5");
  helper.add_short_critical("${time} or result != 'ok'");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  const std::string warn = helper.get_desc().find("w", false).description();
  const std::string crit = helper.get_desc().find("c", false).description();
  // The warning has no template: the bound is all there is, and it replaces.
  EXPECT_NE(std::string::npos, warn.find("and replaces the default warning.")) << warn;
  EXPECT_EQ(std::string::npos, crit.find("replaces the default")) << crit;
  EXPECT_NE(std::string::npos, crit.find("critical=\"time > N*1000 or result != 'ok'\": the default critical with its bound moved, the other conditions stay."))
      << crit;
}

// A check with an option "w" of its own keeps it - and its default warning,
// which reading that option as a threshold used to clear.
TEST(CliHelperShortThresholds, AnOptionOfTheChecksOwnIsNotAThreshold) {
  short_threshold_case c({"w=5", "-c", "90"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  std::string width;
  helper.add_options("load > 70", "load > 95", "", {});
  helper.get_desc().add_options()("w", boost::program_options::value<std::string>(&width), "Width");
  helper.add_short_threshold("load", modern_filter::shorthand::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ("5", width);
  EXPECT_EQ(string_list({"load > 70"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);
}

TEST(CliHelperShortThresholds, AnEmptyOptionOfTheChecksOwnIsNotAMissingThreshold) {
  short_threshold_case c({"w="});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  std::string width = "unset";
  helper.add_options("load > 70", "load > 95", "", {});
  helper.get_desc().add_options()("w", boost::program_options::value<std::string>(&width), "Width");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ("", width);
  EXPECT_EQ(string_list({"load > 70"}), c.data.warn_string);
}

// show-query is left out of show-default, which lists every option with a
// default: "show-query=false" has no place in a line to pass back.
TEST(CliHelperShortThresholds, ShowDefaultDoesNotListShowQuery) {
  short_threshold_case c({"show-default"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  EXPECT_FALSE(helper.parse_options());
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  EXPECT_EQ(std::string::npos, c.message().find("show-query")) << c.message();
}

// "-w -c 90": the -w has no value. It used to take "-c" for one and fail on
// the stray "90"; now it says what is wrong, in every shape.
TEST(CliHelperShortThresholds, ABareFlagInFrontOfAnotherIsAMissingValue) {
  const std::vector<string_list> shapes{{"-w", "-c", "90"}, {"--show-all", "-w", "-c", "90"}, {"show-all=true", "-c", "90", "-w"}, {"--show-all", "-c"}};
  for (const string_list &shape : shapes) {
    short_threshold_case c(shape);
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("load > 70", "load > 95", "", {});
    helper.add_short_threshold("load", modern_filter::shorthand::above);
    EXPECT_FALSE(helper.parse_options()) << boost::join(shape, " ");
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
    const std::string flag = shape.back() == "-c" ? "-c" : "-w";
    EXPECT_EQ("Missing value for " + flag + " (use none to remove the threshold)", c.message()) << boost::join(shape, " ");
  }
}

TEST(CliHelperShortThresholds, AnAttachedNegativeBound) {
  short_threshold_case c({"--show-all", "-c-5:"});
  modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
  helper.add_options("temperature > 70", "temperature > 90", "", {});
  helper.add_short_threshold("temperature", modern_filter::shorthand::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"temperature < -5"}), c.data.crit_string);
}

// An unset macro: "-w $ARG1$ drive=C:" arrives as "-w drive=C:". The -w must
// not eat the drive and run the check on everything with "drive=C:" for a
// warning.
TEST(CliHelperShortThresholds, ABareFlagInFrontOfAKeyValueArgumentIsAMissingValue) {
  const std::vector<string_list> shapes{{"-w", "show-all=true"}, {"-w", "show-all=true", "-c", "90"}, {"show-all=true", "-c", "90", "-w", "debug=true"}};
  for (const string_list &shape : shapes) {
    short_threshold_case c(shape);
    modern_filter::cli_helper<DummyFilter> helper(c.request, c.response, c.data);
    helper.add_options("load > 70", "load > 95", "", {});
    helper.add_short_threshold("load", modern_filter::shorthand::above);
    EXPECT_FALSE(helper.parse_options()) << boost::join(shape, " ");
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
    EXPECT_EQ("Missing value for -w (use none to remove the threshold)", c.message()) << boost::join(shape, " ");
  }
}

// ============================================================================
// cli_helper — the short thresholds, member by member
// ============================================================================

namespace {
namespace sh = modern_filter::shorthand;
typedef modern_filter::cli_helper<DummyFilter> short_helper;

// Whether a helper with the standard options answers `name` for `args`.
bool requested(const string_list &args, const std::string &name) {
  short_threshold_case c(args);
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  return helper.is_requested(name);
}
}  // namespace

TEST(CliHelperIsRequested, TheBareFlagInEitherShape) {
  EXPECT_TRUE(requested({"show-default"}, "show-default"));
  EXPECT_TRUE(requested({"--show-default"}, "show-default"));
  EXPECT_TRUE(requested({"filter=x = 1", "show-default"}, "show-default"));
  EXPECT_TRUE(requested({"--filter", "x = 1", "--show-default"}, "show-default"));
  EXPECT_FALSE(requested({}, "show-default"));
  EXPECT_FALSE(requested({"filter=x = 1", "show-all"}, "show-default"));
}

TEST(CliHelperIsRequested, TheValuedFormRestPasses) {
  for (const std::string value : {"true", "TRUE", "True", "1", "yes", "YES", "on", "On"}) {
    EXPECT_TRUE(requested({"show-default=" + value}, "show-default")) << value;
    EXPECT_TRUE(requested({"--show-default=" + value}, "show-default")) << value;
  }
  for (const std::string value : {"false", "FALSE", "0", "no", "off", "", "maybe", "true ", "truee"}) {
    EXPECT_FALSE(requested({"show-default=" + value}, "show-default")) << value;
    EXPECT_FALSE(requested({"--show-default=" + value}, "show-default")) << value;
  }
}

TEST(CliHelperIsRequested, TheLastOneGivenDecides) {
  EXPECT_FALSE(requested({"show-default", "show-default=false"}, "show-default"));
  EXPECT_TRUE(requested({"show-default=false", "show-default"}, "show-default"));
  EXPECT_TRUE(requested({"show-default=false", "show-default=true"}, "show-default"));
  EXPECT_FALSE(requested({"show-default=true", "--show-default=0"}, "show-default"));
  // An argument that is not the flag does not change the answer.
  EXPECT_TRUE(requested({"show-default", "show-all=false", "filter=x"}, "show-default"));
}

TEST(CliHelperIsRequested, OnlyTheOptionItself) {
  // The word as a value, inside a value, with something in front or behind.
  EXPECT_FALSE(requested({"filter=show-default"}, "show-default"));
  EXPECT_FALSE(requested({"filter=name = 'show-default'"}, "show-default"));
  EXPECT_FALSE(requested({"show-defaults"}, "show-default"));
  EXPECT_FALSE(requested({"show-default-values=true"}, "show-default"));
  EXPECT_FALSE(requested({"xshow-default"}, "show-default"));
  EXPECT_FALSE(requested({"-show-default"}, "show-default"));
  EXPECT_FALSE(requested({"---show-default"}, "show-default"));
  EXPECT_FALSE(requested({"Show-Default"}, "show-default"));
  // Nothing in front of the "=", or nothing at all.
  EXPECT_FALSE(requested({"=true"}, "show-default"));
  EXPECT_FALSE(requested({"--=true"}, "show-default"));
  EXPECT_FALSE(requested({""}, "show-default"));
  EXPECT_FALSE(requested({"--"}, "show-default"));
  // Another flag is another flag.
  EXPECT_FALSE(requested({"show-all"}, "show-default"));
  EXPECT_FALSE(requested({"help"}, "show-default"));
}

TEST(CliHelperIsRequested, AnAbbreviationThatCanOnlyMeanTheOption) {
  for (const std::string arg : {"show-d", "show-de", "show-defaul", "--show-d", "--show-def", "show-d=true", "--show-def=1"})
    EXPECT_TRUE(requested({arg}, "show-default")) << arg;
  EXPECT_FALSE(requested({"show-d=false"}, "show-default"));
  EXPECT_FALSE(requested({"--show-def=0"}, "show-default"));
  // show-all shares these, so they name neither.
  for (const std::string arg : {"s", "sh", "show", "show-", "--show-", "show-=true"}) EXPECT_FALSE(requested({arg}, "show-default")) << arg;
  // The other way around: what is unique for show-all is not show-default.
  EXPECT_FALSE(requested({"show-a"}, "show-default"));
  EXPECT_TRUE(requested({"show-a"}, "show-all"));
}

TEST(CliHelperIsRequested, IsAbbreviationOf) {
  short_threshold_case c({});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  EXPECT_TRUE(helper.is_abbreviation_of("show-d", "show-default"));
  EXPECT_TRUE(helper.is_abbreviation_of("show-defaul", "show-default"));
  // Not shorter than the name: the name itself, or longer.
  EXPECT_FALSE(helper.is_abbreviation_of("show-default", "show-default"));
  EXPECT_FALSE(helper.is_abbreviation_of("show-defaults", "show-default"));
  // Empty, or not how the name starts.
  EXPECT_FALSE(helper.is_abbreviation_of("", "show-default"));
  EXPECT_FALSE(helper.is_abbreviation_of("how-d", "show-default"));
  EXPECT_FALSE(helper.is_abbreviation_of("show-x", "show-default"));
  // Shared with another option: "show-" with show-all, "w" with warn and
  // warning when asking for an option that is not there at all.
  EXPECT_FALSE(helper.is_abbreviation_of("show-", "show-default"));
  EXPECT_FALSE(helper.is_abbreviation_of("w", "warning"));
  EXPECT_FALSE(helper.is_abbreviation_of("warn", "warning"));
  EXPECT_TRUE(helper.is_abbreviation_of("warni", "warning"));
  // Once show-query is registered "show-q" has a meaning, and "show-d" still
  // has only the one.
  EXPECT_TRUE(helper.is_abbreviation_of("show-q", "show-query"));
  helper.register_short_thresholds();
  helper.register_show_query();
  EXPECT_TRUE(helper.is_abbreviation_of("show-q", "show-query"));
  EXPECT_TRUE(helper.is_abbreviation_of("show-d", "show-default"));
  EXPECT_FALSE(helper.is_abbreviation_of("show-", "show-query"));
}

TEST(CliHelperShortRegistration, RegistersOnceAndOnlyWhenEnabled) {
  {
    short_threshold_case c({});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
    EXPECT_FALSE(helper.short_thresholds_registered);
    helper.register_short_thresholds();
    EXPECT_TRUE(helper.short_thresholds_registered);
    EXPECT_TRUE(helper.short_warn_owned);
    EXPECT_TRUE(helper.short_crit_owned);
    // show-query is registered next to them on the way into parse_options().
    helper.register_show_query();
    const std::size_t options = helper.get_desc().options().size();
    // A second call adds nothing (and would throw on a duplicate option at
    // parse time if it did).
    helper.register_short_thresholds();
    EXPECT_EQ(options, helper.get_desc().options().size());
    EXPECT_EQ(1u, helper.short_thresholds.size());
    ASSERT_TRUE(helper.parse_options()) << c.message();
    EXPECT_EQ(options, helper.get_desc().options().size());
  }
  {
    short_threshold_case c({});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.disable_short_thresholds();
    helper.register_short_thresholds();
    EXPECT_FALSE(helper.short_thresholds_registered);
    EXPECT_FALSE(helper.short_warn_owned);
    EXPECT_FALSE(helper.short_crit_owned);
    EXPECT_EQ(nullptr, helper.get_desc().find_nothrow("w", false));
    EXPECT_EQ(nullptr, helper.get_desc().find_nothrow("c", false));
    // show-query is not one of them: it has a registration of its own.
    EXPECT_EQ(nullptr, helper.get_desc().find_nothrow("show-query", false));
    EXPECT_TRUE(helper.short_thresholds.empty());
  }
}

TEST(CliHelperShortRegistration, TheDefaultKeywordIsCountAndIsDeclaredOnce) {
  short_threshold_case c({});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  EXPECT_FALSE(helper.short_thresholds_declared);
  helper.register_short_thresholds();
  EXPECT_FALSE(helper.short_thresholds_declared);
  ASSERT_EQ(1u, helper.short_thresholds.size());
  EXPECT_EQ("count", helper.short_thresholds[0].variable);
  EXPECT_EQ(sh::above, helper.short_thresholds[0].dir);
  const std::string warn = helper.get_desc().find("w", false).description();
  EXPECT_EQ(0u, warn.find("Short form of warning: -w N is the same as warning=\"count > N\" and replaces the default warning.")) << warn;
  // No unit sentence and no list sentence for a plain single keyword.
  EXPECT_EQ(std::string::npos, warn.find("A number without a unit")) << warn;
  EXPECT_EQ(std::string::npos, warn.find("one value per keyword")) << warn;
  EXPECT_NE(std::string::npos, warn.find("Common option for all filter checks.")) << warn;
  const std::string crit = helper.get_desc().find("c", false).description();
  EXPECT_EQ(0u, crit.find("Short form of critical: -c N is the same as critical=\"count > N\" and replaces the default critical.")) << crit;
}

TEST(CliHelperShortRegistration, TheHelpOfAListAndOfAUnit) {
  short_threshold_case c({});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("time > 60 or loss > 5%", "time > 100 or loss > 10%", "", {});
  helper.add_short_threshold("time", sh::above, sh::unit_milliseconds, "60", "100");
  helper.add_short_threshold("loss", sh::above, sh::unit_percent, "5", "10");
  EXPECT_TRUE(helper.short_thresholds_declared);
  helper.register_short_thresholds();
  ASSERT_EQ(2u, helper.short_thresholds.size());
  const std::string warn = helper.get_desc().find("w", false).description();
  EXPECT_EQ(0u, warn.find("Short form of warning: -w TIME,LOSS is the same as warning=\"time > TIME or loss > LOSS%\" and replaces the default warning."))
      << warn;
  EXPECT_NE(std::string::npos, warn.find("\nTakes one value per keyword, separated by commas; one that is left out keeps its default.")) << warn;
  // The unit is that of the first keyword.
  EXPECT_NE(std::string::npos, warn.find("\nA number without a unit is in milliseconds;")) << warn;
  EXPECT_EQ(std::string::npos, warn.find("Common option for all filter checks.")) << warn;
  // The defaults are those of the short form, as they would be typed.
  boost::any warn_default, crit_default;
  ASSERT_TRUE(helper.get_desc().find("w", false).semantic()->apply_default(warn_default));
  ASSERT_TRUE(helper.get_desc().find("c", false).semantic()->apply_default(crit_default));
  EXPECT_EQ(string_list({"60,5"}), boost::any_cast<string_list>(warn_default));
  EXPECT_EQ(string_list({"100,10"}), boost::any_cast<string_list>(crit_default));
}

TEST(CliHelperShortRegistration, AnOptionTheCheckDeclaredIsLeftAlone) {
  short_threshold_case c({});
  short_helper helper(c.request, c.response, c.data);
  std::string width, colour;
  bool own_listing = false;
  helper.add_options("load > 80", "load > 90", "", {});
  helper.get_desc().add_options()("w", boost::program_options::value<std::string>(&width), "Width")(
      "c", boost::program_options::value<std::string>(&colour), "Colour")(
      "show-query", boost::program_options::value<bool>(&own_listing)->implicit_value(true)->default_value(false), "The check's own");
  const std::size_t options = helper.get_desc().options().size();
  helper.register_short_thresholds();
  helper.register_show_query();
  EXPECT_TRUE(helper.short_thresholds_registered);
  EXPECT_FALSE(helper.short_warn_owned);
  EXPECT_FALSE(helper.short_crit_owned);
  EXPECT_FALSE(helper.show_query_owned);
  EXPECT_EQ(options, helper.get_desc().options().size());
  EXPECT_EQ("Width", helper.get_desc().find("w", false).description());
  EXPECT_EQ("Colour", helper.get_desc().find("c", false).description());
  EXPECT_EQ("The check's own", helper.get_desc().find("show-query", false).description());
}

TEST(CliHelperShortRegistration, OnlyOneOfTheTwoIsTheChecksOwn) {
  // "c" is the check's, "w" is the threshold: -w is applied, c= is the
  // check's value, and an empty c= is no missing threshold.
  short_threshold_case c({"-w", "50", "c=", "w=60"});
  short_helper helper(c.request, c.response, c.data);
  std::string colour = "unset";
  helper.add_options("load > 80", "load > 90", "", {});
  helper.get_desc().add_options()("c", boost::program_options::value<std::string>(&colour), "Colour");
  helper.add_short_threshold("load", sh::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_TRUE(helper.short_warn_owned);
  EXPECT_FALSE(helper.short_crit_owned);
  EXPECT_EQ("", colour);
  EXPECT_EQ(string_list({"load > 50", "load > 60"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);

  // The other way around, and here the empty w= is the check's.
  short_threshold_case d({"w=", "-c", "95"});
  short_helper other(d.request, d.response, d.data);
  std::string width = "unset";
  other.add_options("load > 80", "load > 90", "", {});
  other.get_desc().add_options()("w", boost::program_options::value<std::string>(&width), "Width");
  other.add_short_threshold("load", sh::above);
  ASSERT_TRUE(other.parse_options()) << d.message();
  EXPECT_FALSE(other.short_warn_owned);
  EXPECT_TRUE(other.short_crit_owned);
  EXPECT_EQ("", width);
  EXPECT_EQ(string_list({"load > 80"}), d.data.warn_string);
  EXPECT_EQ(string_list({"load > 95"}), d.data.crit_string);
}

TEST(CliHelperShortRegistration, ShortEffect) {
  EXPECT_EQ(" and replaces the default warning.", short_helper::short_effect("warning", string_list()));
  EXPECT_EQ(" and replaces the default critical.", short_helper::short_effect("critical", string_list()));
  EXPECT_EQ(": the default critical with its bound moved, the other conditions stay.", short_helper::short_effect("critical", string_list(1, "${time} or x = 1")));
  EXPECT_EQ(": the default warning with its bound moved, the other conditions stay.", short_helper::short_effect("warning", string_list(2, "${time}")));
}

TEST(CliHelperShortRegistration, ShortOptionCarriesTheShortDefault) {
  short_threshold_case c({});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("time > 5000", "result != 'ok'", "", {});
  helper.add_short_threshold("time", sh::above, sh::unit_seconds_as_ms, "5", "");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  // -w has a default to show, -c has none ...
  const boost::program_options::option_description &warn = helper.get_desc().find("w", false);
  const boost::program_options::option_description &crit = helper.get_desc().find("c", false);
  boost::any value;
  EXPECT_TRUE(warn.semantic()->apply_default(value));
  EXPECT_EQ(string_list({"5"}), boost::any_cast<string_list>(value));
  boost::any none;
  EXPECT_FALSE(crit.semantic()->apply_default(none));
  // ... and neither of them ran: the check's own defaults did.
  EXPECT_EQ(string_list({"time > 5000"}), c.data.warn_string);
  EXPECT_EQ(string_list({"result != 'ok'"}), c.data.crit_string);
}

TEST(CliHelperShortDefaults, AnswerDefaultQueries) {
  {
    // Nothing asked: nothing answered, but the options are registered.
    short_threshold_case c({"filter=x = 1"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    EXPECT_FALSE(helper.answer_default_queries());
    EXPECT_TRUE(helper.short_thresholds_registered);
    EXPECT_TRUE(helper.show_query_owned);
    EXPECT_EQ(0, c.response->lines_size());
  }
  {
    // show-default: rendered before -w / -c and show-query exist, which are
    // registered all the same.
    short_threshold_case c({"show-default"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
    EXPECT_TRUE(helper.answer_default_queries());
    EXPECT_TRUE(helper.short_thresholds_registered);
    EXPECT_TRUE(helper.show_query_owned);
    EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
    EXPECT_NE(std::string::npos, c.message().find("\"warning=load > 80\"")) << c.message();
    EXPECT_EQ(std::string::npos, c.message().find("\"w=")) << c.message();
    EXPECT_EQ(std::string::npos, c.message().find("show-query")) << c.message();
  }
  {
    // Both asked for: show-default is the one that answers, as it is the one
    // that never runs the check's own code.
    short_threshold_case c({"show-query", "show-default"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
    EXPECT_TRUE(helper.answer_default_queries());
    EXPECT_NE(std::string::npos, c.message().find("\"warning=load > 80\"")) << c.message();
  }
  {
    // show-query is not answered here: build_filter() does.
    short_threshold_case c({"show-query"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    EXPECT_FALSE(helper.answer_default_queries());
    EXPECT_EQ(0, c.response->lines_size());
  }
}

// A helper without -w / -c (CheckHelpers' render_perf) answers show-default
// the same way, and leaves show-query out of it just the same.
TEST(CliHelperShortDefaults, DisabledShowDefaultIsUnchanged) {
  short_threshold_case c({"show-default"});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  helper.disable_short_thresholds();
  EXPECT_FALSE(helper.parse_options());
  EXPECT_FALSE(helper.short_thresholds_registered);
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  EXPECT_NE(std::string::npos, c.message().find("\"warning=load > 80\"")) << c.message();
  EXPECT_NE(std::string::npos, c.message().find("\"critical=load > 90\"")) << c.message();
  EXPECT_EQ(std::string::npos, c.message().find("show-query")) << c.message();
  // It is exactly what the option parser printed for it before.
  modern_filter::data_container other_data;
  short_threshold_case d({});
  short_helper other(d.request, d.response, other_data);
  other.add_options("load > 80", "load > 90", "", {});
  EXPECT_EQ(nscapi::program_options::help_show_default(other.get_desc()), c.message());
}

TEST(CliHelperShortArguments, NormalizedRequest) {
  const string_list typed{"-w", "80", "drive=c:", "-c90"};
  short_threshold_case c(typed);
  short_helper helper(c.request, c.response, c.data);
  const PB::Commands::QueryRequestMessage::Request normalized = helper.normalized_request();
  EXPECT_EQ("test_command", normalized.command());
  EXPECT_EQ(string_list({"w=80", "drive=c:", "c=90"}), string_list(normalized.arguments().begin(), normalized.arguments().end()));
  // The request itself is not touched.
  EXPECT_EQ(typed, string_list(c.request.arguments().begin(), c.request.arguments().end()));

  // Disabled: handed on exactly as typed.
  helper.disable_short_thresholds();
  const PB::Commands::QueryRequestMessage::Request untouched = helper.normalized_request();
  EXPECT_EQ("test_command", untouched.command());
  EXPECT_EQ(typed, string_list(untouched.arguments().begin(), untouched.arguments().end()));

  // No arguments at all.
  short_threshold_case none({});
  short_helper empty(none.request, none.response, none.data);
  EXPECT_EQ(0, empty.normalized_request().arguments_size());
}

TEST(CliHelperShortArguments, CheckShortValues) {
  const auto request_of = [](const string_list &args) {
    PB::Commands::QueryRequestMessage::Request request;
    for (const std::string &a : args) request.add_arguments(a);
    return request;
  };
  {
    // Every value is there.
    short_threshold_case c({});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.register_short_thresholds();
    EXPECT_TRUE(helper.check_short_values(request_of({"w=80", "c=90", "filter="})));
    EXPECT_TRUE(helper.check_short_values(request_of({})));
    EXPECT_EQ(0, c.response->lines_size());
  }
  for (const std::string flag : {"w", "c"}) {
    for (const std::string arg : {flag + "=", "--" + flag + "="}) {
      short_threshold_case c({});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      helper.register_short_thresholds();
      EXPECT_FALSE(helper.check_short_values(request_of({"filter=x", arg}))) << arg;
      EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result()) << arg;
      EXPECT_EQ("Missing value for -" + flag + " (use none to remove the threshold)", c.message()) << arg;
    }
  }
  {
    // Disabled: an empty w= is none of our business.
    short_threshold_case c({});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.disable_short_thresholds();
    EXPECT_TRUE(helper.check_short_values(request_of({"w=", "c="})));
    EXPECT_EQ(0, c.response->lines_size());
  }
  {
    // Not registered, so neither is ours yet.
    short_threshold_case c({});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    EXPECT_TRUE(helper.check_short_values(request_of({"w="})));
    EXPECT_TRUE(helper.check_short_values(request_of({"c="})));
    EXPECT_EQ(0, c.response->lines_size());
  }
}

TEST(CliHelperShortApply, NotGivenOrDefaultedLeavesTheDefaultsAlone) {
  short_threshold_case c({"filter=x = 1"});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "70", "");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  // -w has a default (and so is in the map, defaulted), -c has none (and is
  // not in it): neither is applied.
  EXPECT_EQ(string_list({"load > 80"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);
}

TEST(CliHelperShortApply, NextToEveryWayOfGivingTheLongForm) {
  // warning= and critical=: one more expression each.
  {
    short_threshold_case c({"warning=load > 50", "critical=load > 60", "w=70", "c=75"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above);
    ASSERT_TRUE(helper.parse_options()) << c.message();
    EXPECT_EQ(string_list({"load > 50", "load > 70"}), c.data.warn_string);
    EXPECT_EQ(string_list({"load > 60", "load > 75"}), c.data.crit_string);
  }
  // The aliases warn= and crit= count as given too.
  {
    short_threshold_case c({"warn=load > 50", "crit=load > 60", "w=70", "c=75"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above);
    ASSERT_TRUE(helper.parse_options()) << c.message();
    EXPECT_EQ(string_list({"load > 50", "load > 70"}), c.data.warn_string);
    EXPECT_EQ(string_list({"load > 60", "load > 75"}), c.data.crit_string);
  }
  // A check without a default has nothing to replace.
  {
    short_threshold_case c({"w=70", "c=75"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("", "", "", {});
    helper.add_short_threshold("load", sh::above);
    ASSERT_TRUE(helper.parse_options()) << c.message();
    EXPECT_EQ(string_list({"load > 70"}), c.data.warn_string);
    EXPECT_EQ(string_list({"load > 75"}), c.data.crit_string);
  }
  // A helper that never declared warning / critical at all.
  {
    short_threshold_case c({"w=70", "c=75"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options(std::map<std::string, std::string>(), "ignored");
    helper.add_short_threshold("load", sh::above);
    ASSERT_TRUE(helper.parse_options()) << c.message();
    EXPECT_EQ(string_list({"load > 70"}), c.data.warn_string);
    EXPECT_EQ(string_list({"load > 75"}), c.data.crit_string);
  }
}

TEST(CliHelperShortApply, GivenMoreThanOnce) {
  short_threshold_case c({"w=70", "w=load < 5", "-w", "@1:2", "-c", "none"});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("load > 80", "load > 90", "", {});
  helper.add_short_threshold("load", sh::above);
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ(string_list({"load > 70", "load < 5", "load >= 1 and load <= 2"}), c.data.warn_string);
  // "none" is an expression like any other here; build_filter() drops it.
  EXPECT_EQ(string_list({"none"}), c.data.crit_string);
}

TEST(CliHelperShortApply, ABadValueOnEitherSideIsReportedAndNothingIsApplied) {
  {
    short_threshold_case c({"w=80m", "c=90"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above, sh::unit_percent_number);
    EXPECT_FALSE(helper.parse_options());
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
    EXPECT_EQ("Invalid unit for load: 80m (expected a percentage)", c.message());
    EXPECT_EQ(string_list({"load > 80"}), c.data.warn_string);
    // The critical side is not looked at once the warning failed.
    EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);
  }
  {
    short_threshold_case c({"w=85", "c=20:10"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above, sh::unit_percent_number);
    EXPECT_FALSE(helper.parse_options());
    EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
    EXPECT_EQ("Invalid threshold: 20:10 (a range starts at its low end, as in 10:20)", c.message());
    EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);
  }
  {
    // The second of two values is the bad one: the first is not applied.
    short_threshold_case c({"w=85", "w=1,2"});
    short_helper helper(c.request, c.response, c.data);
    helper.add_options("load > 80", "load > 90", "", {});
    helper.add_short_threshold("load", sh::above);
    EXPECT_FALSE(helper.parse_options());
    EXPECT_EQ("Too many values in threshold 1,2: expected at most 1 (load)", c.message());
    EXPECT_EQ(string_list({"load > 80"}), c.data.warn_string);
  }
  {
    // A list with nothing in it, and a value that is only blanks.
    for (const std::string value : {",", " "}) {
      short_threshold_case c({"--show-all", "--w", value});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      helper.add_short_threshold("load", sh::above);
      EXPECT_FALSE(helper.parse_options()) << value;
      EXPECT_EQ("Missing value for -w (use none to remove the threshold)", c.message()) << value;
    }
  }
}

TEST(CliHelperShortApply, CalledDirectlyWithAnOptionThatIsNotOurs) {
  // apply_short_thresholds() skips a side whose option is the check's own,
  // whatever that option holds.
  short_threshold_case c({"w=not a threshold, 1:2:3", "c=5"});
  short_helper helper(c.request, c.response, c.data);
  std::string width, colour;
  helper.add_options("load > 80", "load > 90", "", {});
  helper.get_desc().add_options()("w", boost::program_options::value<std::string>(&width), "Width")(
      "c", boost::program_options::value<std::string>(&colour), "Colour");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_EQ("not a threshold, 1:2:3", width);
  EXPECT_EQ("5", colour);
  EXPECT_EQ(string_list({"load > 80"}), c.data.warn_string);
  EXPECT_EQ(string_list({"load > 90"}), c.data.crit_string);
}

// The three parse_options() overloads share the short threshold handling;
// each of them is taken through every way out.
namespace {
enum parse_kind { parse_plain, parse_positional, parse_extra };
bool run_parse(short_helper &helper, const parse_kind kind, string_list &extra) {
  if (kind == parse_positional) {
    boost::program_options::positional_options_description p;
    p.add("filter", -1);
    return helper.parse_options(p);
  }
  if (kind == parse_extra) return helper.parse_options(extra);
  return helper.parse_options();
}
}  // namespace

TEST(CliHelperShortParse, EveryOverloadTakesEveryWayOut) {
  for (const parse_kind kind : {parse_plain, parse_positional, parse_extra}) {
    string_list extra;
    {
      // Applied.
      short_threshold_case c({"show-all=true", "-w", "85", "-c95"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
      ASSERT_TRUE(run_parse(helper, kind, extra)) << kind << ": " << c.message();
      EXPECT_EQ(string_list({"load > 85"}), c.data.warn_string) << kind;
      EXPECT_EQ(string_list({"load > 95"}), c.data.crit_string) << kind;
    }
    {
      // show-default is answered; show-query is recorded for build_filter().
      short_threshold_case c({"show-default"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      helper.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
      EXPECT_FALSE(run_parse(helper, kind, extra)) << kind;
      EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result()) << kind;
      EXPECT_NE(std::string::npos, c.message().find("\"warning=load > 80\"")) << kind << ": " << c.message();
      EXPECT_EQ(std::string::npos, c.message().find("\"w=")) << kind << ": " << c.message();

      short_threshold_case d({"show-query=true", "-w", "85"});
      short_helper other(d.request, d.response, d.data);
      other.add_options("load > 80", "load > 90", "", {});
      other.add_short_threshold("load", sh::above, sh::unit_percent_number, "80", "90");
      EXPECT_TRUE(run_parse(other, kind, extra)) << kind << ": " << d.message();
      EXPECT_TRUE(other.show_query_requested) << kind;
      EXPECT_EQ(0, d.response->lines_size()) << kind;
      EXPECT_EQ(string_list({"load > 85"}), d.data.warn_string) << kind;
    }
    {
      // A flag without a value.
      short_threshold_case c({"show-all=true", "-c"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      EXPECT_FALSE(run_parse(helper, kind, extra)) << kind;
      EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result()) << kind;
      EXPECT_EQ("Missing value for -c (use none to remove the threshold)", c.message()) << kind;
    }
    {
      // The option parser refuses the line: a value for a number that is none.
      short_threshold_case c({"decimals=many", "-w", "85"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      EXPECT_FALSE(run_parse(helper, kind, extra)) << kind;
      EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result()) << kind;
      // Nothing was stored, the short threshold included.
      EXPECT_TRUE(c.data.warn_string.empty()) << kind;
    }
    {
      // A threshold that does not fit the check.
      short_threshold_case c({"-w", "1,2"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      EXPECT_FALSE(run_parse(helper, kind, extra)) << kind;
      EXPECT_EQ("Too many values in threshold 1,2: expected at most 1 (count)", c.message()) << kind;
    }
    {
      // warmup-state is validated after the thresholds are applied.
      short_threshold_case c({"warmup-state=sideways", "-w", "85"});
      short_helper helper(c.request, c.response, c.data);
      helper.add_options("load > 80", "load > 90", "", {});
      helper.add_warmup_option();
      helper.add_short_threshold("load", sh::above);
      EXPECT_FALSE(run_parse(helper, kind, extra)) << kind;
      EXPECT_EQ("Invalid warmup-state: sideways (expected one of ok, warning, critical, unknown)", c.message()) << kind;
      EXPECT_EQ(string_list({"load > 85"}), c.data.warn_string) << kind;
    }
  }
}

TEST(CliHelperShortParse, TheExtraArgumentsAreStillCollected) {
  // check_pdh: what is not an option is a counter, and -w / -c are not left
  // among them.
  short_threshold_case c({"\\Processor(_total)\\% Processor Time", "-w", "80", "-c", "90"});
  short_helper helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  helper.add_short_threshold("value", sh::above);
  string_list extra;
  ASSERT_TRUE(helper.parse_options(extra)) << c.message();
  EXPECT_EQ(string_list({"\\Processor(_total)\\% Processor Time"}), extra);
  EXPECT_EQ(string_list({"value > 80"}), c.data.warn_string);
  EXPECT_EQ(string_list({"value > 90"}), c.data.crit_string);
}

// ============================================================================
// cli_helper — show-query
// ============================================================================

namespace {
// Enough of a filter for build_filter() to run: records whether matching
// started, and refuses an expression naming a keyword it does not have, the
// way the real engine fails to build one.
struct QueryFilter {
  struct {
    std::string list_separator = ", ";
  } summary;
  struct fake_context {
    void set_number_format(const str::number_format &) {}
  };
  std::shared_ptr<fake_context> context = std::make_shared<fake_context>();
  bool started = false;
  void set_human_number_format(bool) {}
  bool build_syntax(bool, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &, const std::string &) {
    return true;
  }
  bool build_index(const std::string &, std::string &) { return true; }
  static bool known(const std::vector<std::string> &expressions) {
    for (const std::string &e : expressions)
      if (e.find("no_such_keyword") != std::string::npos) return false;
    return true;
  }
  bool build_engines(bool, const std::vector<std::string> &filter, const std::vector<std::string> &ok, const std::vector<std::string> &warn,
                     const std::vector<std::string> &crit) {
    return known(filter) && known(ok) && known(warn) && known(crit);
  }
  bool validate(std::string &) { return true; }
  void start_match() { started = true; }
};
typedef modern_filter::cli_helper<QueryFilter> query_helper;

// A check in the shape of check_tcp: a time bound with a template on the
// critical side, a syntax, and options of its own.
struct query_case : short_threshold_case {
  std::string host;
  int port = 0;
  bool verbose = false;
  std::vector<std::string> times;
  query_helper helper;
  explicit query_case(const string_list &args) : short_threshold_case(args), helper(request, response, data) {
    helper.add_options("time > 1000", "time > 5000 or result != 'ok'", "", {});
    helper.add_short_threshold("time", sh::above, sh::unit_seconds_as_ms, "1", "5");
    helper.add_short_critical("${time} or result != 'ok'");
    helper.add_syntax("${status}: ${list}", "${host}: ${time}ms", "${host}", "", "");
    // clang-format off
    helper.get_desc().add_options()
      ("host", boost::program_options::value<std::string>(&host), "Host")
      ("port", boost::program_options::value<int>(&port)->default_value(443), "Port")
      ("verbose", boost::program_options::value<bool>(&verbose)->implicit_value(true)->default_value(false), "Verbose")
      ("time", boost::program_options::value<std::vector<std::string> >(&times), "Times")
      ;
    // clang-format on
  }
  // Runs the check up to where show-query answers; true when it would have
  // gone on to match.
  bool run() {
    if (!helper.parse_options()) return false;
    QueryFilter filter;
    const bool built = helper.build_filter(filter);
    EXPECT_EQ(built, filter.started);
    return built;
  }
  bool has(const std::string &token) const { return message().find("\"" + token + "\" ") != std::string::npos; }
};
}  // namespace

TEST(CliHelperShowQuery, TheDefaults) {
  query_case c({"show-query"});
  EXPECT_FALSE(c.run());
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  EXPECT_EQ(
      "\"warning=time > 1000\" \"critical=time > 5000 or result != 'ok'\" \"debug=false\" \"show-all=false\" \"empty-state=ignored\" "
      "\"escape-html=false\" \"list-separator=, \" \"decimals=-1\" \"top-syntax=${status}: ${list}\" \"detail-syntax=${host}: ${time}ms\" "
      "\"perf-syntax=${host}\" \"port=443\" \"verbose=false\" ",
      c.message());
}

TEST(CliHelperShowQuery, ShortThresholdsAreResolved) {
  query_case c({"-w", "2", "show-query", "-c", "10"});
  EXPECT_FALSE(c.run());
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  EXPECT_TRUE(c.has("warning=time > 2000")) << c.message();
  EXPECT_TRUE(c.has("critical=time > 10000 or result != 'ok'")) << c.message();
  EXPECT_EQ(std::string::npos, c.message().find("\"w=")) << c.message();
  EXPECT_EQ(std::string::npos, c.message().find("\"c=")) << c.message();
  EXPECT_EQ(std::string::npos, c.message().find("show-query")) << c.message();
}

TEST(CliHelperShowQuery, AnExplicitWarningNextToAShortOne) {
  query_case c({"warning=result = 'timeout'", "-w", "2", "show-query"});
  EXPECT_FALSE(c.run());
  // Two expressions, two tokens: the explicit one and the -w.
  EXPECT_NE(std::string::npos, c.message().find("\"warning=result = 'timeout'\" \"warning=time > 2000\" ")) << c.message();
  EXPECT_TRUE(c.has("critical=time > 5000 or result != 'ok'")) << c.message();
}

TEST(CliHelperShowQuery, EveryExpressionIsATokenOfItsOwn) {
  query_case c({"filter=host = 'a'", "filter=host = 'b'", "critical=time > 1", "crit=time > 2", "show-query"});
  EXPECT_FALSE(c.run());
  EXPECT_NE(std::string::npos, c.message().find("\"filter=host = 'a'\" \"filter=host = 'b'\" ")) << c.message();
  // crit= is folded into critical (it replaces it, as it does on a run), and
  // is not echoed as an option of its own.
  EXPECT_TRUE(c.has("critical=time > 2")) << c.message();
  EXPECT_FALSE(c.has("critical=time > 1")) << c.message();
  EXPECT_EQ(std::string::npos, c.message().find("\"crit=")) << c.message();
  // Never joined into one value (the list separator is the first ", " that
  // belongs in the line).
  const std::string head = c.message().substr(0, c.message().find("\"list-separator="));
  EXPECT_EQ(std::string::npos, head.find(", ")) << head;
}

// What a check changes between parse_options() and build_filter() is what
// runs, so that is what is shown (check_cpu clears its default filter for
// cores=true, check_pdh moves its top-syntax, check_eventlog sets a unique
// index).
TEST(CliHelperShowQuery, ARewriteByTheCheckIsShown) {
  query_case c({"show-query", "host=db01"});
  ASSERT_TRUE(c.helper.parse_options()) << c.message();
  c.data.syntax_top = "${status}: ${problem_list}";
  c.helper.append_all_filters("and", "host != 'localhost'");
  c.helper.set_default_index("${host}");
  c.data.warn_string.clear();
  QueryFilter filter;
  EXPECT_FALSE(c.helper.build_filter(filter));
  EXPECT_FALSE(filter.started);
  EXPECT_TRUE(c.has("top-syntax=${status}: ${problem_list}")) << c.message();
  EXPECT_TRUE(c.has("filter=(host != 'localhost')and host != 'localhost'")) << c.message();
  EXPECT_TRUE(c.has("unique-index=${host}")) << c.message();
  // Dropped by the check, while the option has a default: "none" says so, as
  // leaving it out would bring the default back on a paste.
  EXPECT_TRUE(c.has("warning=none")) << c.message();
  EXPECT_TRUE(c.has("host=db01")) << c.message();
}

// A filter setting the check fills in without declaring its option is shown
// as well, after the declared ones.
TEST(CliHelperShowQuery, ASettingWithoutAnOptionIsShownToo) {
  query_case c({"show-query"});
  ASSERT_TRUE(c.helper.parse_options());
  c.data.perf_config = "time(unit:s)";
  QueryFilter filter;
  EXPECT_FALSE(c.helper.build_filter(filter));
  EXPECT_TRUE(c.has("perf-config=time(unit:s)")) << c.message();
}

TEST(CliHelperShowQuery, TheValuedFormRestPasses) {
  for (const std::string arg : {"show-query=true", "--show-query=true", "show-query=1", "--show-query"}) {
    query_case c({arg});
    EXPECT_FALSE(c.run()) << arg;
    EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result()) << arg;
    EXPECT_TRUE(c.has("warning=time > 1000")) << arg << ": " << c.message();
  }
  // Switched off it is an ordinary run.
  for (const std::string arg : {"show-query=false", "--show-query=false", "show-query=0"}) {
    query_case c({arg});
    EXPECT_TRUE(c.run()) << arg << ": " << c.message();
    EXPECT_FALSE(c.helper.show_query_requested) << arg;
    EXPECT_EQ(0, c.response->lines_size()) << arg;
  }
}

// The expressions are still built: an invalid one fails as it would on a run.
TEST(CliHelperShowQuery, AnInvalidExpressionStillFails) {
  query_case c({"warning=no_such_keyword > 1", "show-query"});
  EXPECT_FALSE(c.run());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, c.response->result());
  EXPECT_EQ("Failed to build engines", c.message());
  // And so does an option the check refuses before it gets that far.
  query_case d({"byte-unit=lightyears", "show-query"});
  EXPECT_FALSE(d.run());
  EXPECT_EQ(PB::Common::ResultCode::UNKNOWN, d.response->result());
}

TEST(CliHelperShowQuery, TheChecksOwnOptionsAreEchoed) {
  query_case c({"host=db01", "port=5432", "verbose", "time=5m", "time=1m", "show-all", "show-query"});
  EXPECT_FALSE(c.run());
  EXPECT_TRUE(c.has("host=db01")) << c.message();
  EXPECT_TRUE(c.has("port=5432")) << c.message();
  EXPECT_TRUE(c.has("verbose=true")) << c.message();
  EXPECT_NE(std::string::npos, c.message().find("\"time=5m\" \"time=1m\" ")) << c.message();
  EXPECT_TRUE(c.has("show-all=true")) << c.message();
  // show-all is already applied to the top-syntax shown next to it, and
  // applying it again is a no-op: the line runs the same pasted back.
  EXPECT_TRUE(c.has("top-syntax=${status}: %(list)")) << c.message();
  for (const std::string meta : {"help=", "help-pb=", "help-short=", "show-default=", "show-query="})
    EXPECT_EQ(std::string::npos, c.message().find("\"" + meta)) << meta << ": " << c.message();
}

// Pasted back, the line is the same query.
TEST(CliHelperShowQuery, TheLineRoundTrips) {
  query_case first({"-w", "2", "host=db01", "show-all", "show-query"});
  EXPECT_FALSE(first.run());
  string_list args;
  const std::string line = first.message();
  for (std::string::size_type at = line.find('"'); at != std::string::npos; at = line.find('"', line.find('"', at + 1) + 1))
    args.push_back(line.substr(at + 1, line.find('"', at + 1) - at - 1));
  args.push_back("show-query");
  query_case second(args);
  EXPECT_FALSE(second.run());
  EXPECT_EQ(first.message(), second.message());
}

// The arguments parse_options() leaves to the check are part of its query.
TEST(CliHelperShowQuery, TheExtraArgumentsAreEchoed) {
  short_threshold_case c({"\\Processor(_total)\\% Processor Time", "-w", "80", "show-query"});
  query_helper helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  helper.add_short_threshold("value", sh::above);
  string_list extra;
  ASSERT_TRUE(helper.parse_options(extra)) << c.message();
  QueryFilter filter;
  EXPECT_FALSE(helper.build_filter(filter));
  EXPECT_NE(std::string::npos, c.message().find("\"warning=value > 80\" ")) << c.message();
  EXPECT_NE(std::string::npos, c.message().find("\"\\Processor(_total)\\% Processor Time\" ")) << c.message();
}

// Without -w / -c it is still there for whatever goes through build_filter().
TEST(CliHelperShowQuery, WithoutShortThresholds) {
  short_threshold_case c({"show-query", "warning=x > 1"});
  query_helper helper(c.request, c.response, c.data);
  helper.add_options("", "", "", {});
  helper.disable_short_thresholds();
  ASSERT_TRUE(helper.parse_options()) << c.message();
  QueryFilter filter;
  EXPECT_FALSE(helper.build_filter(filter));
  EXPECT_EQ(PB::Common::ResultCode::OK, c.response->result());
  EXPECT_NE(std::string::npos, c.message().find("\"warning=x > 1\" ")) << c.message();
}

// A check with a show-query of its own keeps it, and is not answered for it.
TEST(CliHelperShowQuery, AnOptionOfTheChecksOwnIsLeftAlone) {
  short_threshold_case c({"show-query"});
  query_helper helper(c.request, c.response, c.data);
  bool own = false;
  helper.add_options("load > 80", "", "", {});
  helper.get_desc().add_options()("show-query", boost::program_options::value<bool>(&own)->implicit_value(true)->default_value(false), "Own");
  ASSERT_TRUE(helper.parse_options()) << c.message();
  EXPECT_TRUE(own);
  EXPECT_FALSE(helper.show_query_owned);
  QueryFilter filter;
  EXPECT_TRUE(helper.build_filter(filter));
}

TEST(CliHelperShowQuery, IsACommonBooleanOption) {
  query_case c({});
  ASSERT_TRUE(c.run());
  const boost::program_options::option_description &op = c.helper.get_desc().find("show-query", false);
  EXPECT_NE(std::string::npos, op.description().find("Common option for all filter checks.")) << op.description();
  EXPECT_NE(nullptr, dynamic_cast<const boost::program_options::typed_value<bool> *>(op.semantic().get()));
  // help lists it with its default, as every other flag.
  query_case h({"help"});
  EXPECT_FALSE(h.helper.parse_options());
  EXPECT_NE(std::string::npos, h.message().find("show-query=ARG")) << h.message();
}

TEST(CliHelperShowQuery, RenderOptionValue) {
  string_list out;
  EXPECT_TRUE(query_helper::render_option_value(boost::any(std::string("x")), out));
  EXPECT_EQ(string_list({"x"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(std::string()), out));
  EXPECT_TRUE(out.empty());
  EXPECT_TRUE(query_helper::render_option_value(boost::any(string_list({"a", "", "b"})), out));
  EXPECT_EQ(string_list({"a", "b"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(false), out));
  EXPECT_EQ(string_list({"false"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(-3), out));
  EXPECT_EQ(string_list({"-3"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(static_cast<unsigned short>(7)), out));
  EXPECT_EQ(string_list({"7"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(12LL), out));
  EXPECT_EQ(string_list({"12"}), out);
  EXPECT_TRUE(query_helper::render_option_value(boost::any(0.5), out));
  EXPECT_EQ(string_list({"0.5"}), out);
  // A type it cannot render is left out rather than guessed at.
  EXPECT_FALSE(query_helper::render_option_value(boost::any(std::pair<int, int>(1, 2)), out));
  EXPECT_FALSE(query_helper::render_option_value(boost::any(), out));
}
