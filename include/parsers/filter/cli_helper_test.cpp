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
